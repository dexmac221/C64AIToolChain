# Dreadline

Original C64 game inspired by fast side-scrolling dreadnought attack games.

This is not a direct clone of any commercial game. It uses the same broad arcade fantasy: skim across an enormous hostile vessel, dodge defenses, and destroy targets.

## Gameplay
- Fly the white attack craft over a scrolling alien dreadnought.
- The deck is drawn once, then scrolled by updating columns instead of redrawing the whole background.
- The row-copy hot path is implemented in 6502 assembly (`fastscroll.s`) for smoother scrolling.
- Ship, drone, turret, and core sprites use generated C64 multicolor frames and animate by swapping sprite pointers.
- The scrolling deck image is generated from the bitmap source `deck_bitmap.pgm` into hi-res custom character tiles plus screen/color tables.
- VIC display memory uses bank `$4000-$7fff`: screen `$4400`, custom charset `$6000`, sprite data `$7800`.
- Avoid red drones and turrets.
- Fire at targets to score points.
- Survive as long as possible while the speed increases.
- Demo mode auto-starts from the title screen.

## Controls
- Joystick Port 2: move ship
- Fire: shoot / start manual play
- Wait on title screen: demo autoplay

## Build
```bash
./build.sh
```

The build runs `spritegen.py` and `bggen.py` first. Edit the sprite ASCII art or the deck bitmap, then rebuild.

Sprite art symbols:
- ` ` transparent
- `.` shared multicolor 0 (`$D025`)
- `#` per-sprite color (`$D027-$D02E`)
- `+` shared multicolor 1 (`$D026`)

Background art lives in `deck_bitmap.pgm`, a plain PGM bitmap. `bggen.py` converts 8x8 pixel blocks into a deduplicated C64 hi-res character set, then emits screen/color tables. The generated deck still scrolls through the assembly row-copy routine while C fills the new rightmost column from `background_mc.h`.

## Run
```bash
./run_vice.sh
```
