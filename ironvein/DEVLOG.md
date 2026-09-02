# IRON VEIN — development log

An 8-way scrolling action demo in the Turrican class, built to find out
whether this toolchain can reach that class at all. Raw notes, kept as the
work happens, dead ends included. This file feeds the article.

## Day 1 — 2026-08-04 — what we are actually up against

The human asked for the most complex game we could attempt. We reasoned
through four candidates (8-way scroller, Dropzone, isometric, Elite) and
picked the scroller, with a rule: **spike first**. A bare engine — 8-way
scroll with colour, a dozen multiplexed sprites, nothing else — profiled
until it says 50 fps or says no. No tiles get drawn before the verdict.

### The wall: colour RAM

Screen RAM we can double-buffer (two 1000-byte screens, flip `$D018`).
Colour RAM lives at `$D800`, one copy, and the VIC reads it live. Every
coarse scroll step moves the picture by one character, so the colours
must move by one character *in the same frame the new screen appears*,
with the beam already on its way down.

Options weighed:

- *Copy colour RAM behind the beam.* One frame of wrong colour on every
  row (the beam draws a row with old colour, then we fix it). A 6 Hz
  colour flicker. No.
- *Copy ahead of the beam, from colour RAM itself.* Works for "content
  moves up/left" (read from later addresses), fails for "content moves
  down" (you overwrite the source before reading it) unless you go
  bottom-up, and bottom-up cannot be ahead of a beam that starts at the
  top. No.
- **Colour shadow.** Keep the next screen's colours in a plain RAM
  array, built during the frames *before* the flip, in whatever order is
  convenient. On the flip frame, blast the 880 bytes into `$D800` from
  the top down, starting in the bottom border of the previous frame so
  the copy is always rows ahead of the beam. Direction no longer matters.
  Yes.

The blast is fully unrolled `lda abs / sta abs`: 8 cycles a byte, 7 000
cycles, 5.3 KB of generated code that lives under the BASIC ROM at
`$A000`, which cc65 banks out anyway.

### The second wall: you cannot prepare what you cannot predict

Preparing the shifted back buffer takes several frames, so we must know
the scroll direction before the coarse boundary is crossed. A player can
reverse at any frame. Rules adopted:

- the engine prepares for the direction the camera is *going*, and
  restarts the preparation if that changes;
- the camera is not allowed to cross a boundary until the buffer for
  that exact crossing is ready — it **stalls**, at most three frames;
- diagonal motion with the two fine positions out of phase settles by
  itself into alternating X and Y crossings four frames apart, which is
  exactly the preparation time. That is not luck, it is the stall
  re-phasing the axes once.

In the game the camera will have a dead zone, so reversals are usually
known long before a boundary. The spike counts stalls to see how often
they really happen.

### The third wall: the vertical split

HUD on top, playfield below, and `$D011` YSCROLL must change between
them. A bad line fires when `(raster & 7) == YSCROLL`, so writing a new
YSCROLL on a line whose low bits equal it fires a *late* bad line and
garbles the first characters of the row. And if the new bad line cuts the
row in progress before its eighth line, the VIC re-displays the same
matrix row (VCBASE only advances at RC=7).

Design: rows 0-1 HUD, row 2 a blank spacer, rows 3-24 playfield. HUD at
YSCROLL 7 in 24-row mode (so row 0 sits exactly on the top of the
window). The split IRQ writes YSCROLL at line `$48`, or `$49` when the
value is 0. Worked through on paper, the playfield's first line lands on
`$4F..$56` in the fine-scroll order 7,0,1,2,3,4,5,6 — contiguous, so the
spacer's cut-or-repeat is invisible. **To be verified on the machine**;
this is the kind of thing paper gets wrong by one line.

### Found while designing: a latent race in the older games

ION RIFT and GHOST KEEP build the multiplexer table in the main loop
while the raster IRQs of the same frame are walking it. It mostly works
because the build is quick and sorted, but it is a race. IRON VEIN
double-banks the table: C fills the bank the IRQ is not reading, then
publishes it; the split IRQ latches the bank once per frame.

### Colour per character, not per cell

Each character code gets one colour (a 256-byte table). It is a classic
C64 constraint and it is what makes the shadow cheap: the colour of a
new edge cell is a table lookup, and the shadow shift is a byte copy from
colour RAM. Per-cell colour would need a second full map layer.

### Budget, on paper

Per crossing: 880-byte screen shift + 880-byte colour shift spread over 3
frames (~7.5 k cycles a frame), plus ~1 k for the edge. Crossing frame:
7 k blast. A frame is ~19 600 cycles. Single-axis scrolling costs ~20% a
frame; steady diagonal costs ~40% on three frames of four and 36% on the
fourth. The game logic must fit in the rest. Numbers to be replaced by
measurements.

## Day 1, later — the spike runs, and the machine corrects the paper

First light: HUD, scrolling tiles, sprites, all on screen. Then the
list of things the paper did not know.

**An unterminated multiplexer table.** The IRQ walked the table before
C had built one: 64 bytes of zero, no `$FF`, so it kept going through
the rest of BSS, using whatever it found as sprite slots. `sta $d000,x`
with garbage in X sprayed the VIC registers; `sta $47F8,x` overwrote the
hero's sprite pointer and six characters of the HUD. Symptoms: garbled
text, a vanished hero, wrong colours. Cause: one missing `memset`.

**The split IRQ waited with `==`.** The handler synced to line `$48`
with `cpx $d012 / bne`. When the interrupt landed one line late — sprite
DMA near the HUD is enough — the loop spun until the *next* frame's
`$48`: 19 600 cycles gone inside an interrupt, and that frame drew the
playfield with the HUD's YSCROLL, so the picture jumped. That was the
"out of phase" background the human saw. Now the IRQ fires a line early
and waits with `>=`.

**Measuring wrong, twice.** Raster-line deltas alias above line 255, so
the first probes were fiction. The replacement, a free-running CIA
timer, is exact — and its self-test (200 empty loop turns = 5 400
cycles, an empty probe = 450) is what made the next numbers believable.

**cc65 and the C stack.** `write_edges` measured 27 000 cycles for at
most 62 cells. The generated code had `tosumula0` (a multiply, from
`r * 40`) and seven `staxysp`: every pointer local lived on the C stack,
and every `*p++` was a hundred cycles. Same disease in `update_drones`:
`int` temporaries on the stack, 18 000 cycles for twelve drones. The
cure is `register` pointers (zero page under `-Or`), globals for the
hot ints, and a `row_ofs[]` table instead of any `* 40`. The rule for
this machine: *nothing hot on the C stack, no `*` anywhere.*

**The vertical split holds.** Eight frozen captures while the camera
moved up past the floor, reading `cam_cy` and `fy` from RAM in the same
monitor session as the screenshot: the floor edge satisfied
`y + 8*cam_cy + fy = const` to the pixel across fine positions 3, 5, 6
and 7. No 8-line jump. The spacer-row cut-or-repeat behaves as worked
out on paper. Phases 0, 1, 2, 4 still to be caught.

## Day 1, night — verdict

Colour per row in, shadow and blast out, the hot loops in assembly,
diagonals phase-aligned so a diagonal step is one crossing every eight
frames, the copy spread over six of them (four chunks, then the column
edge, then the row edge). Measured over 20 seconds of the diagnostic
route - reversals, diagonals, the lot - with eight world-anchored
drones through the multiplexer:

| | 12 drones, profiler on | 8 drones, profiler on | 8 drones, profiler off |
|---|---|---|---|
| frames lost | 8% | 0 | 0 |
| frames late | — | 20% | **1 in 1057** |
| worst iteration | 33 800 | 21 700 | under a frame |

Two lessons hidden in that table.

**A lost frame is not the only bad frame.** The miss counter (two
vsyncs while working) sat at zero while one iteration in five ended
*after* its vsync: the fine scroll for that frame was latched from stale
values, the picture stood still for one frame and jumped two pixels on
the next. Same iteration count, same vsync count, invisible to both. A
"late" counter - the flag already set when the loop comes round to wait
- is the number that matters, and it was the last one added.

**The profiler was a fifth of the problem.** Six CIA probes a frame
cost 2 700 cycles, 14% of the budget. With them compiled out the late
count went from 212 to 1. Measure with it on, ship with it off, and
never trust the last 15% of the budget while it is on.

The engine's own cost per coarse step is now about 20 000 cycles
spread over six frames, with the worst single frame around 8 000 in
the visible-frame currency. That leaves roughly 11 000 a frame for the
game, which this test spends on eight objects in assembly with room for
none in C. The per-object code of the game must be assembly or the
strict byte-only C dialect; that was true for ION RIFT too and it is
truer here.

The human, watching the 12-drone build with its late frames, said
"very fluid". The eye forgives a late frame in five. The counter does
not, and next session's game will be judged by the counter first.

**The vertical split, all eight phases.** A test hook pins the vertical
fine position from a monitor byte; eight frozen captures at the same
camera row, floor edge measured in 36 columns each: `y(fy) = y(0) - fy`
exactly for fy = 0..7, including the YSCROLL=0 case that has its own
write line. The spacer-row cut-or-repeat behaves exactly as worked out
on paper on day one. (The earlier sweeps that suggested a 5-pixel
discrepancy were reading a stale variable address from an old label
file and a detector that sometimes found the view's edge instead of the
world's. Re-read the label file after every build.)

## What the spike proves, and what it does not

Proved on the machine, with counters, not eyes:

- 8-way scrolling at one pixel a frame, colour, HUD split, at a true
  50 frames a second with eight multiplexed sprites: 1 late frame in
  1 057 on a route built to provoke stalls;
- the double-buffered screen plus colour-per-row scheme costs the game
  about 8 000 cycles on its worst frame and nothing on most;
- the camera's commit-and-stall rule works and the stalls are where
  predicted (diagonal onsets, reversals).

Not proved, because it is not built: the game. A hero with gravity and
platforms, enemies that fall and land - the human's first remark when
the drones went by was that they "lack physicality" - weapons, a boss
of stacked sprites, a level designed instead of generated. Each of
those is per-object logic, and the spike's last table says exactly how
that logic has to be written.

## Day 2 — physics

The human's first remark on seeing the drones: "they lack physicality".
So: gravity first, art later.

- **Hero**: gravity every other frame to a terminal speed of 4, jump
  at -6 (42 pixels, clears a 32-pixel metatile with margin), landing
  snapped to the tile top, head bump, walls probed at waist height. One
  pixel a frame, which is also the camera's speed - the two never
  disagree.
- **Walkers** (the eight drones, now with feet): the same physics in
  assembly, plus wall reversal, and the even-numbered ones turn at a
  ledge instead of walking off it. They drop in from above the view.
- **Camera**: a dead zone, 120..176 horizontally and 56..112
  vertically; the camera moves only when the hero leaves it. That is
  what makes the diagonal phase-alignment stall invisible - by the time
  the camera has to move diagonally the hero has been heading that way
  for a while.
- **solid_at** in assembly: ~90 cycles a probe. It is the unit of cost
  in all of this: a walker doing full physics was 1 100 cycles a frame
  at six probes; at three (centre foot, centre head, waist wall) it is
  about 700. Twelve-pixel objects on 32-pixel tiles do not need corner
  probes.

Budget after the cut: 0 lost frames, 36 late in 1 059 (3.4%) with the
hero, eight walkers and the multiplexer. The prep is spread over seven
frames now (five chunks, column edge, row edge), one to spare.

Small traps: a comment tail swallowed by a text replace and assembled
as macro arguments; the phase-align counter landing on the same address
as a calibration slot; the monitor "clear probes" command wiping the
calibration values, so those are only readable right after launch.

## Day 3 — a game layer on the engine

One step, one measurement, as agreed: the human is paying for tokens.

- **Hero animation**: stand, four run frames (stride, pass, stride,
  contact - four ticks each), jump. Drawn facing right; the left set is
  generated by reversing each row of the ASCII, since a wide pixel is
  one character. Twelve frames from six drawings. Judged first on the
  magnified sheet (assetsheet.py grew a `--sprite-col` option so it can
  show a game other than GHOST KEEP in its own colours), and the sheet
  is where the run cycle was read as a cycle - the game only ever shows
  one 21-pixel frame at a time.
- **Shots**: four objects appended to the walkers' arrays - same SoA,
  same multiplexer, no new code path on the IRQ side. A bolt flies at
  four pixels a frame for forty frames, dies on rock (one probe), or on
  a walker. The walker then counts down through three explosion frames
  and respawns. The hit test is byte arithmetic against a shifted range
  ("shot minus walker plus 6 must be 0..20 with a zero high byte") -
  two 16-bit subtractions and two compares per pair, 8 pairs per live
  shot.
- **Contact**: the same range test between the hero and the eight
  walkers, once a frame. A hit costs an energy bar and a second of
  blinking (sprite 0 enable toggled from C - the IRQ never touches
  $D015). At zero the hero drops back in from the top of the view and
  every walker on screen is blown away; no camera relocation, no
  full redraw, so a death costs nothing.
- **HUD**: energy as eight bars - a multicolour-defined tile whose all-
  set rows come out solid in hires, coloured by yellow colour RAM in
  the HUD's own row - and a five-digit score. Both rewritten only when
  they change, in both screen buffers.
- The multiplexer took per-object sprite pointers (dr_ptr) instead of
  one global frame toggle. The walkers set theirs in their own update,
  the explosions set theirs from the countdown, the shots at spawn.

What moved out of C: everything per pair. What stayed in C: spawning,
the HUD, the hero's own frame selection - things that happen once a
frame or less.

Measured once, on the autopilot, 2 856 frames: 1 lost, 145 late (5.1%,
up from 3.4% with the same eight walkers). Four shots and the two
range tests bought that. Not chased today.

Two traps, both old ones wearing new clothes. A fire-only input handed
control to the autopilot, because "no direction pressed" was the test
for "nobody playing"; a standing hero shooting is a player too. And
the first reading of the shot state said every bolt died in its first
frame - it was read at the previous build's addresses, four bytes off.
The lesson from day one, re-learned: re-read the label file after
every build, before every monitor command.
