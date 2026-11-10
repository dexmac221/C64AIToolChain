#!/usr/bin/env python3
"""
check.py - is the picture on screen the map?

Stops the running game through the VICE monitor a number of times and,
each time, reads which screen buffer the VIC is showing ($D018), the
camera that picture was committed with ($0388) and the buffer itself,
and compares all 40 x 21 playfield cells with what the level says
should be there. A cell whose gem or power unit has been taken may be
blank. Prints the mismatches by screen column.

    ./check.py [--samples N] [--hold HEX] [--spot N]

--hold sets the held joystick bits while sampling (08 = right), --spot
teleports first. The game must be running in VICE (./run_vice.sh) and
past the title. Each sample is taken at the entry of _commit, when the
main loop has done all its writing for the frame; build first, the
address comes from fulgor.lbl.

This is the test that pinned down the right-hand flicker the human saw:
with two buffers the engine began preparing the next step in the one
that was still on screen (14 samples in 60 wrong, columns 30..39).
"""
import argparse
import socket
import sys
import time

import assetgen as A


class Monitor:
    """One connection for the whole run: VICE 3.7.1 wedges when
    connections come and go. A line sent while the game runs stops it;
    "x" lets it go on."""

    def __init__(self, port=6510):
        self.s = socket.create_connection(("127.0.0.1", port), 3)
        self.s.settimeout(0.25)
        self.cmd("")

    def read(self, until=None, t=3.0):
        buf = b""
        end = time.time() + t
        while time.time() < end:
            try:
                d = self.s.recv(65536)
                if not d:
                    break
                buf += d
                if until and until in buf:
                    end = min(end, time.time() + 0.15)
            except socket.timeout:
                if buf and not until:
                    break
        return buf.decode(errors="replace")

    def cmd(self, line, until=None):
        self.s.sendall((line + "\n").encode())
        return self.read(until)

    def go(self):
        self.s.sendall(b"x\n")

    def close(self):
        self.go()
        time.sleep(0.2)
        self.s.close()


def parse(out):
    """monitor 'm' output -> {address: byte}"""
    mem = {}
    for line in out.splitlines():
        if ">C:" not in line:
            continue
        f = line.split(">C:")[1].split()
        base = int(f[0], 16)
        for i, t in enumerate(f[1:17]):
            if len(t) != 2:
                break
            try:
                mem[base + i] = int(t, 16)
            except ValueError:
                break
    return mem


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--samples", type=int, default=30)
    p.add_argument("--port", type=int, default=6510,
                   help="VICE remote monitor port (a second instance keeps "
                        "the one someone is playing untouched)")
    p.add_argument("--hold", default=None)
    p.add_argument("--start", action="store_true",
                   help="press fire first, to leave the title (same "
                        "connection: one connection per VICE session)")
    p.add_argument("--spot", default=None)
    p.add_argument("--gap", type=float, default=0.043,
                   help="seconds the game runs between samples")
    p.add_argument("--skip", type=int, default=2,
                   help="frames between samples")
    p.add_argument("--anywhere", action="store_true",
                   help="stop wherever the monitor stops (VICE does it at "
                        "the frame's end, before the scroll code has run, "
                        "and so sees nothing) instead of at _commit")
    a = p.parse_args()

    chars, mt_tab = A.compile_chars()
    level, grid, top = A.tile_level()
    takeable = {A.MT["GEM"], A.MT["POWER"]}

    def expect(cx, cy):
        mt = level[cy >> 2][cx >> 2]
        return mt_tab[(cy & 3) * 4 + (cx & 3)][mt], mt in takeable

    brk = None
    if not a.anywhere:
        for line in open("fulgor.lbl"):
            f = line.split()
            if len(f) == 3 and f[2] == "._commit":
                brk = f[1][-4:]
    mon = Monitor(a.port)
    if a.start:
        mon.cmd("> 033c 10")
        mon.go()
        time.sleep(2.0)
    if a.spot:
        mon.cmd("> 0341 %02x" % int(a.spot))
        mon.go()
        time.sleep(1.5)
    if a.hold:
        mon.cmd("> 033e %s" % a.hold)
        mon.go()
        time.sleep(0.4)
    if brk:
        mon.cmd("break %s" % brk)
    bad_cols = [0] * 40
    bad_samples = 0
    seen = set()
    for s in range(a.samples):
        if brk:
            # run to the end of the main loop's work for a frame: by then
            # it has written whatever it is going to write
            mon.s.sendall(b"x\n")
            mon.read(until=b"Stop on", t=3.0)
        out = mon.cmd("m d018 d018") + mon.cmd("m 0388 038b")
        out += mon.cmd("m 4400 4fff", until=b">C:4ff0")
        if not brk:
            mon.go()
        m = parse(out)
        if 0xD018 not in m or 0x4FE0 not in m:
            print("sample %d: monitor read failed" % s)
            time.sleep(0.2)
            continue
        scr = 0x4000 + ((m[0xD018] >> 4) & 15) * 0x400
        cam_x = m[0x388] | (m[0x389] << 8)
        cam_y = m[0x38A] | (m[0x38B] << 8)
        cx0, cy0 = cam_x >> 3, cam_y >> 3
        seen.add((cx0, cy0))
        bad = []
        for r in range(21):
            for c in range(40):
                want, soft = expect(cx0 + c, cy0 + r)
                got = m[scr + 120 + r * 40 + c]
                if got != want and not (soft and got == 32):
                    bad.append((c, r))
        if bad:
            bad_samples += 1
            for c, r in bad:
                bad_cols[c] += 1
            cols = sorted(set(c for c, r in bad))
            print("sample %2d: screen $%04X cam %4d,%4d  %3d wrong cells, "
                  "columns %d..%d" % (s, scr, cam_x, cam_y, len(bad),
                                      cols[0], cols[-1]))
        if brk:
            for _ in range(a.skip):          # let some frames go by
                mon.s.sendall(b"x\n")
                mon.read(until=b"Stop on", t=3.0)
        else:
            time.sleep(a.gap)
    if brk:
        mon.cmd("del")
    if a.hold:
        mon.cmd("> 033e 00")
    mon.close()
    print("%d different camera cells sampled" % len(seen))
    print("%d of %d samples had wrong cells" % (bad_samples, a.samples))
    if bad_samples:
        print("by column:", " ".join("%d" % v for v in bad_cols))
    return 1 if bad_samples else 0


if __name__ == "__main__":
    sys.exit(main())
