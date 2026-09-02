#!/usr/bin/env python3
"""
record_gif.py - film a running VICE through its monitor and make a GIF.

No screen grabbing: every frame is a VICE screenshot taken through the
remote monitor, so the picture is the emulator's own, whatever is on
top of its window. Each capture pauses the emulation for a few
milliseconds; between captures it runs at full speed, so a 0.15 s
interval is about six game frames per GIF frame.

Monitor commands can be scheduled at a time in seconds - the games'
agent hooks (teleports, held joystick bits) make the film:

    ./record_gif.py --out ironvein/ironvein_demo.gif --seconds 36 \
        --at "8:> 0341 02" --at "17:> 0341 03" --at "26:> 0341 04"

Needs ffmpeg. The frames go to a temp directory and are removed.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time


def monitor(port, cmds):
    """Send the commands and "x" through nc and let it hang up at once.
    VICE's remote monitor sends nothing on connect, flushes its replies
    lazily, and has died on clients that lingered; a batch from nc -q 0
    is what hundreds of hand-typed sessions used, and it is ~0.15 s
    per screenshot including the pause."""
    subprocess.run(["nc", "-q", "0", "127.0.0.1", str(port)],
                   input=("\n".join(cmds) + "\nx\n").encode(),
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--out", required=True)
    p.add_argument("--seconds", type=float, default=30)
    p.add_argument("--interval", type=float, default=0.15,
                   help="seconds of wall time between captures")
    p.add_argument("--fps", type=int, default=7, help="GIF playback rate")
    p.add_argument("--port", type=int, default=6510)
    p.add_argument("--width", type=int, default=384)
    p.add_argument("--at", action="append", default=[],
                   help='"SECONDS:monitor command", may repeat')
    p.add_argument("--keep", action="store_true", help="keep the frames")
    args = p.parse_args()

    sched = []
    for a in args.at:
        t, cmd = a.split(":", 1)
        sched.append((float(t), cmd))
    sched.sort()

    tmp = tempfile.mkdtemp(prefix="vicegif_")
    n = 0
    t0 = time.time()
    next_shot = t0
    try:
        while True:
            now = time.time()
            if now - t0 >= args.seconds:
                break
            while sched and sched[0][0] <= now - t0:
                _, cmd = sched.pop(0)
                monitor(args.port, [cmd])
                print(f"{now - t0:5.1f}s  {cmd}")
            if now >= next_shot:
                path = os.path.join(tmp, f"f{n:05d}.png")
                monitor(args.port, [f'screenshot "{path}" 2'])
                n += 1
                next_shot += args.interval
            else:
                time.sleep(min(0.01, next_shot - now))
    except KeyboardInterrupt:
        pass
    print(f"{n} frames in {time.time() - t0:.1f}s -> {tmp}")
    if n == 0:
        sys.exit("no frames")

    pal = os.path.join(tmp, "pal.png")
    vf = f"scale={args.width}:-1:flags=neighbor"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-framerate", str(args.fps),
                    "-i", os.path.join(tmp, "f%05d.png"),
                    "-vf", f"{vf},palettegen=max_colors=64", pal], check=True)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-framerate", str(args.fps),
                    "-i", os.path.join(tmp, "f%05d.png"), "-i", pal,
                    "-lavfi", f"{vf}[x];[x][1:v]paletteuse=dither=none",
                    "-loop", "0", args.out], check=True)
    print(f"{args.out}: {os.path.getsize(args.out) // 1024} KB, "
          f"{n} frames at {args.fps} fps")
    if not args.keep:
        shutil.rmtree(tmp)


if __name__ == "__main__":
    main()
