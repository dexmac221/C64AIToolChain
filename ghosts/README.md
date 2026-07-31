# GHOST KEEP — Ghosts'n Goblins tribute, level 1

A C64 platformer built on the ION RIFT engine. Arthur runs across the
graveyard, jumps the arcade's rigid arc, throws lances, and loses his
armour before he loses his life.

```bash
./build.sh          # assetgen.py + cl65, fails loudly on compiler errors
./run_vice.sh       # or: x64 ... -autostart ghosts.prg
```

## Files

| File | Role |
|------|------|
| `ghosts.c` | game: level drawing, platform physics, enemies, HUD, screens |
| `assetgen.py` | asset pipeline: charset, sprite frames, level map → `.h` files |
| `irq.s` | raster IRQ chain + sprite multiplexer (ported from ION RIFT) |
| `scroll.s` | unrolled row copier for the coarse scroll |
| `ghosts.cfg` | linker config: code below $4000, VIC bank 1 gap, data at $8000 |
| `tiles_mc.h`, `sprites_mc.h`, `level.h` | generated, do not edit by hand |

Memory map (VIC bank 1): screen A `$4400`, screen B `$4800`,
charset `$5000`, sprite data `$5800`.

## Engine (inherited from ION RIFT)

- Pixel-smooth `$D016` scrolling, double-buffered screen RAM, the fine
  scroll latched together with the `$D018` flip inside `irq_top` —
  splitting those two costs a visible 7-pixel back-jump per coarse step.
- Raster split at line 65: two steady HUD rows above a scrolling
  playfield of 23 rows.
- Sprite multiplexer recycling hardware sprites 3-7 for the enemies;
  sprites 0-2 are Arthur, his lance and a spare.
- Coarse-scroll work spread across the 8 fine-scroll frames so no single
  frame carries a heavy job.

## What is implemented

- **Level 1 map**, hand-designed in `assetgen.py`: 208 columns of ground
  height plus a decoration id (headstone, cross, dead tree, railing),
  with a plateau in the middle to jump onto.
- **Arthur**: run animation, fixed 24-entry jump arc (no air control, as
  in the arcade), lance throwing, armour → underwear → death, respawn,
  3 lives, level timer.
- **Enemies**: zombies that rise out of the soil and shamble toward
  Arthur while following the ground height; crows that cross the sky.
  Killed enemies leave a puff. All multiplexed.
- **Parallax**: a thin dithered horizon repainted from its own model at
  half the foreground speed with crosses standing on its crest, plus a
  star field at a third and a quarter speed.
- **Audio**: two-voice SID march (minor bass under a jaunty lead) and
  effects on voice 3.
- **Agent I/O**: the toolchain convention — `$033C` edge-triggered input,
  `$033E` hold input with a watchdog, `$033D` missed-frame counter.
- Title screen, demo autopilot, game over.

## What is not there yet

- Scripted enemy placement (spawns are still random) and the flesh-eating
  plants from the original.
- A real end of level: the map simply runs out.
- Distinct underwear frames for Arthur — losing the armour is only a
  sprite colour change right now.
- The ladder/platform section of the arcade level 1.
- The horizon profile undulates very little; it reads as a slightly too
  regular ribbon.

## C64 lessons this game paid for

**Sprite bit pairs are not character bit pairs.** For characters `11` is
colour RAM; for sprites `11` is a shared register and `10` is the
per-sprite colour. Putting the sprite body on `11` made knight, zombie
and crow all render in the same shared colour. `assetgen.py` therefore
keeps two separate mappings, `MC_TILE` and `MC_SPR`.

**Multicolor text mode allows only colours 0-7 per character.** No grey,
no brown, no orange. Stone and earth have to come from the two shared
registers (`$D022`, `$D023`) and the per-char slot carries the turf
green. A second, darker green does not exist on this machine — the
distant horizon gets one by dithering green against black.

**Sprites cover the background unless `$D01B` says otherwise, and the VIC
counts only bit pairs `10` and `11` as foreground.** Soil drawn with
`01` pixels let a buried sprite show through in speckles. The dirt tiles
were redrawn on `10`/`11` so that zombies with low priority really are
buried until they climb out.

**Enemies must be dragged by the scroll.** Their X lives in screen
space, so every pixel the world scrolls is owed back to them; without it
a zombie chasing the player at 2px/frame exactly matched his pace and
the pair looked glued together.

## Verification techniques used here

- **Raster-border profiler**: paint `$D020` per phase, screenshot, count
  the scanlines of each colour in Python — one line is 63 cycles. This is
  how ION RIFT's real bottleneck was found.
- **Frozen-frame capture**: a single monitor connection that inspects
  state *and* takes the screenshot without resuming, so the picture is
  exactly the frame that was inspected.
- **Model measurement**: read the game's own arrays (`$033D` misses,
  multiplexer tables) through the monitor instead of trusting the eye.
- Note the observer effect: every monitor connection freezes the
  emulator, so polling once a second *looks* like the game stuttering.
  Measure with counters in RAM and read them once at the end.
