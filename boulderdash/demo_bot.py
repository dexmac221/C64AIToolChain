#!/usr/bin/env python3
"""
demo_bot.py - Autonomous demo player for Boulder Rush (System 1 style).

Reads the cave directly from C64 screen RAM through the VICE remote
monitor, plans a path with weighted BFS, and drives the miner through
the agent input byte at $033C (same bit convention as
async_agent_control.py: up=1 down=2 left=4 right=8 fire=16).

Usage:
    python3 demo_bot.py [--duration 180] [--host 127.0.0.1] [--port 6510]
"""

import argparse
import heapq
import re
import socket
import time

SCREEN0 = 0x0400
CAVE_H = 24                 # cave occupies screen rows 1..24
CAVE_W = 40

# Screen codes used by the game
SPACE, DIRT, BOULDER, DIAMOND = 32, 128, 129, 130
WALL, STEEL, EXIT_CLOSED, EXIT_OPEN = 131, 132, 133, 134
PLAYER, EXPL = 135, 136

PASSABLE = {SPACE, DIRT, DIAMOND, EXIT_OPEN}

# (dx, dy) -> joystick mask
DIR_MASK = {(0, -1): 0x01, (0, 1): 0x02, (-1, 0): 0x04, (1, 0): 0x08}
FIRE = 0x10


class ViceGone(Exception):
    pass


def mon(host, port, cmd, timeout=3.0):
    """Send one monitor command, return its text output, resume emulation."""
    try:
        s = socket.create_connection((host, port), timeout=timeout)
    except OSError as e:
        raise ViceGone(str(e))
    s.settimeout(0.5)
    try:
        s.recv(4096)                       # eat prompt
    except socket.timeout:
        pass
    s.sendall((cmd + "\nx\n").encode())
    data = b""
    end = time.time() + timeout
    while time.time() < end:
        try:
            chunk = s.recv(65536)
            if not chunk:
                break
            data += chunk
        except socket.timeout:
            break
    s.close()
    return data.decode("latin1", "replace")


def read_screen(host, port):
    """Return the 1000 screen RAM bytes, or None on a bad read."""
    out = mon(host, port, "m 0400 07ff")
    mem = {}
    for line in out.splitlines():
        # search, not match: the first line carries the "(C:$xxxx) " prompt
        m = re.search(r">C:([0-9a-f]{4})\s\s(.{50})", line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        for i, pair in enumerate(re.findall(r"[0-9a-f]{2}", m.group(2))):
            mem[addr + i] = int(pair, 16)
    scr = [mem.get(SCREEN0 + i) for i in range(1000)]
    if any(v is None for v in scr):
        return None
    return scr


def decode_row(scr, row):
    return "".join(chr(c) if 32 <= c < 96 else "." for c in
                   scr[row * 40:row * 40 + 40])


def set_input(host, port, mask):
    mon(host, port, f"> 033c {mask:02x}", timeout=1.5)


def plan(scr, banned=()):
    """Weighted BFS. Returns (path_dirs, goal, description)."""
    grid = [scr[40 + r * 40:40 + r * 40 + 40] for r in range(CAVE_H)]
    ppos = None
    diamonds = []
    exits_open = []
    for r in range(CAVE_H):
        for c in range(CAVE_W):
            v = grid[r][c]
            if v == PLAYER:
                ppos = (c, r)
            elif v == DIAMOND:
                diamonds.append((c, r))
            elif v == EXIT_OPEN:
                exits_open.append((c, r))
    if ppos is None:
        return None, None, "no player on screen"
    diamonds = [d for d in diamonds if d not in banned]

    # Dijkstra with a penalty for stepping right under a boulder
    INF = 1 << 20
    dist = {ppos: 0}
    prev = {}
    pq = [(0, ppos)]
    while pq:
        d, (x, y) = heapq.heappop(pq)
        if d > dist.get((x, y), INF):
            continue
        for (dx, dy) in DIR_MASK:
            nx, ny = x + dx, y + dy
            if not (0 <= nx < CAVE_W and 0 <= ny < CAVE_H):
                continue
            if grid[ny][nx] not in PASSABLE:
                continue
            step = 1
            if ny > 0 and grid[ny - 1][nx] == BOULDER:
                step += 4                  # risky: boulder overhead
            nd = d + step
            if nd < dist.get((nx, ny), INF):
                dist[(nx, ny)] = nd
                prev[(nx, ny)] = (x, y)
                heapq.heappush(pq, (nd, (nx, ny)))

    def path_to(goal):
        steps = []
        cur = goal
        while cur != ppos:
            px, py = prev[cur]
            steps.append((cur[0] - px, cur[1] - py))
            cur = (px, py)
        steps.reverse()
        return steps

    # Goal choice: open exit > reachable diamond > dig toward diamonds
    if exits_open:
        reach = [e for e in exits_open if e in dist]
        if reach:
            goal = min(reach, key=lambda e: dist[e])
            return path_to(goal), goal, f"exit at {goal}"

    reach_dia = [d for d in diamonds if d in dist]
    if reach_dia:
        goal = min(reach_dia, key=lambda d: dist[d])
        return path_to(goal), goal, f"gem at {goal}"

    if diamonds:
        # No gem reachable: move to the reachable cell closest to one
        def h(cell):
            return min(abs(cell[0] - d[0]) + abs(cell[1] - d[1])
                       for d in diamonds)
        cand = [c for c in dist if c != ppos and c not in banned]
        if cand:
            goal = min(cand, key=lambda c: (h(c) * 3 + dist[c]))
            if h(goal) < h(ppos):
                return path_to(goal), goal, f"digging toward gems via {goal}"

    # Nothing better: roam to the farthest reachable cell
    cand = [c for c in dist if c != ppos and c not in banned]
    if cand:
        goal = max(cand, key=lambda c: dist[c])
        return path_to(goal), goal, f"roaming to {goal}"
    return None, None, "stuck"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--duration", type=int, default=180)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=6510)
    args = ap.parse_args()

    host, port = args.host, args.port
    t0 = time.time()
    deadline = t0 + args.duration
    last_hud = ""
    last_gems = None
    no_progress = 0
    banned = {}                 # goal cell -> expiry time

    print(f"[bot] demo for {args.duration}s", flush=True)
    try:
        while time.time() < deadline:
            scr = read_screen(host, port)
            if scr is None:
                print("[bot] bad screen read, retrying", flush=True)
                time.sleep(0.6)
                continue

            full_text = "\n".join(decode_row(scr, r) for r in range(25))
            hud = decode_row(scr, 0).strip()

            # Title or game-over screens: press fire (edge-triggered,
            # the game consumes and clears the byte itself)
            if "TRIBUTE" in full_text or "O V E R" in full_text:
                state = "TITLE" if "TRIBUTE" in full_text else "GAMEOVER"
                print(f"[bot] {state} -> fire", flush=True)
                set_input(host, port, FIRE)
                time.sleep(2.0)
                continue

            if hud != last_hud and hud.startswith("CAVE"):
                print(f"[bot] HUD: {hud}", flush=True)
                last_hud = hud

            # Progress tracking: gems collected means we're doing fine
            m = re.search(r"GEMS (\d+)/", hud)
            gems = int(m.group(1)) if m else None
            if gems is not None and gems != last_gems:
                last_gems = gems
                no_progress = 0

            now = time.time()
            banned = {c: t for c, t in banned.items() if t > now}
            path, goal, why = plan(scr, banned)
            if not path:
                print(f"[bot] wait: {why}", flush=True)
                time.sleep(0.7)
                continue

            # Stuck on the same unattainable goal: ban it for a while
            no_progress += 1
            if no_progress > 20 and goal:
                banned[goal] = now + 45
                no_progress = 0
                print(f"[bot] no progress, banning goal {goal}", flush=True)
                continue

            # One write = exactly one game move (edge-triggered input)
            d0 = path[0]
            elapsed = int(time.time() - t0)
            print(f"[bot] t={elapsed}s {why} | step {d0}", flush=True)
            set_input(host, port, DIR_MASK[d0])
            time.sleep(0.12)
    except ViceGone:
        print("[bot] VICE is gone, stopping demo", flush=True)
    finally:
        try:
            set_input(host, port, 0)
        except (OSError, ViceGone):
            pass
        print("[bot] done", flush=True)


if __name__ == "__main__":
    main()
