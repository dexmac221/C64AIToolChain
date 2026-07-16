#!/usr/bin/env python3
"""
ion_bot.py - External System 1 agent for ION RIFT.

ION RIFT runs at 50 fps, far too fast to replan per move like the
Boulder Rush bot. Instead the game publishes a 6-byte telemetry block
and the bot steers with a HOLD input byte at ~3 Hz:

  $0340 telemetry: ship_y, gap_top, gap_bot, enemy_y, enemy_dist/2, state
  $033E hold input (up=1 down=2 left=4 right=8), auto-expires in-game
        after ~0.5s; bit 7 is toggled every write as a heartbeat
  $033C edge-triggered input (fire=16)

Policy: fly toward the middle of the flyable corridor, bias away from
the nearest enemy, fire when an enemy is roughly aligned ahead.

Usage: python3 ion_bot.py [--duration 120]
"""

import argparse
import re
import socket
import time

HOST, PORT = "127.0.0.1", 6510
TELE = 0x0340
UP, DOWN, FIRE = 0x01, 0x02, 0x10
ST_TITLE, ST_PLAY, ST_OVER = 0, 1, 2


def mon(cmd, timeout=3.0):
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    s.settimeout(0.4)
    try:
        s.recv(4096)
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


def read_telemetry():
    out = mon(f"m {TELE:04x} {TELE + 5:04x}")
    m = re.search(r">C:[0-9a-f]{4}\s\s((?:[0-9a-f]{2}\s+){5}[0-9a-f]{2})",
                  out)
    if not m:
        return None
    return [int(b, 16) for b in m.group(1).split()]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--duration", type=int, default=120)
    args = ap.parse_args()

    t0 = time.time()
    beat = 0
    last_state = None
    print(f"[ion-bot] flying for {args.duration}s", flush=True)

    try:
        while time.time() - t0 < args.duration:
            t = read_telemetry()
            if t is None:
                print("[ion-bot] bad telemetry read", flush=True)
                time.sleep(0.6)
                continue
            ship_y, gap_top, gap_bot, en_y, en_d, state = t

            if state != last_state:
                print(f"[ion-bot] state={state}", flush=True)
                last_state = state

            if state != ST_PLAY:
                mon("> 033c 10")            # fire: start / restart
                time.sleep(1.2)
                continue

            # Steering: corridor middle, biased away from a close enemy
            target = (gap_top + gap_bot) // 2
            fire = False
            if en_d != 0xFF and en_y != 0xFF:
                # dodge early: at 0.3s reaction time a dart covers 60px
                if en_d < 90 and abs(en_y - ship_y) < 20:
                    target = gap_top + 14 if en_y > ship_y else gap_bot - 14
                if en_d < 130 and abs((en_y + 4) - (ship_y + 6)) < 14:
                    fire = True

            hold = 0
            if ship_y < target - 3:
                hold = DOWN
            elif ship_y > target + 3:
                hold = UP

            beat ^= 0x80                     # heartbeat for the watchdog
            mon(f"> 033e {hold | beat:02x}")
            if fire:
                mon("> 033c 10")
            time.sleep(0.3)
    finally:
        try:
            mon("> 033e 00")
        except OSError:
            pass
        print("[ion-bot] done", flush=True)


if __name__ == "__main__":
    main()
