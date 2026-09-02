# IRON VEIN: an 8-way scroller on the C64, built by an agent that had to measure everything

*A Turrican-class demo written in cc65 and assembly by Claude (Fable 5.1)
inside the C64AIToolChain, with a human who grew up on the VIC-20 watching
over its shoulder. What it took, where it went wrong, and what the numbers
said. The raw diary is in `ironvein/DEVLOG.md`; this is the short version.*

![IRON VEIN](../ironvein/ironvein_demo.gif)

## The brief

"Push this project as far as it goes with a game of high complexity." We
argued through four candidates: an 8-way scroller in the Turrican mould,
a Dropzone-style horizontal blaster, an isometric room game, a wireframe
Elite. The scroller won because its difficulty is *engineering*
difficulty, the kind that a profiler can arbitrate: either the machine
scrolls in eight directions with colour and a dozen sprites at 50 frames a
second, or it does not, and no amount of prose changes which.

The terms, in the human's words: it does not have to be Turrican, a
similar demo that proves the technique is reliable; keep notes on the
development and the difficulties; then write this.

One rule was set before any code: **spike first**. A bare engine, profiled
until it says 50 fps or says no, before a single tile is drawn. That rule
turned out to be the most valuable decision of the project, because the
first version of the engine said no.

## Three walls

The C64 does not want to scroll in eight directions. It has hardware fine
scroll of up to seven pixels in each axis, after which the whole screen
of characters must be moved by one cell, and the 40x25 character screen
has a companion 1000-byte colour RAM at `$D800` that the VIC reads live
and that cannot be double-buffered. Three walls follow from that.

**Colour RAM.** Screen RAM can be double-buffered (two screens, flip
`$D018` in the border). Colour RAM cannot, so on every coarse step the
colours must move by one cell in the same frame the new screen appears,
with the raster beam already on its way down. Copying behind the beam
means a frame of wrong colours on every row, a 6 Hz flicker. The first
design kept a *colour shadow* in plain RAM, built ahead of time, and
blasted it into `$D800` from the top down starting in the bottom border,
7 000 cycles of unrolled `lda abs / sta abs` per crossing. It worked,
and it was measured at 38 000 cycles per step once the shadow shift was
included. Two frames. Dead end.

What replaced it is a constraint dressed as a design: **one colour per
world row**. The cave is banded, blue near the surface, cyan where the
crystals are, green in the middle, red at the depths, and a row's colour
is a table lookup by world row. A horizontal step moves no colour at
all. A vertical step repaints only the rows whose colour changes, which
with four bands is one row at a band edge, forty stores. The art
direction follows the engine, not the other way round, and the cave
looks better for it.

**You cannot prepare what you cannot predict.** Shifting an 880-byte
screen into the back buffer takes several frames, so the engine must
know the direction before the boundary is crossed, and a player can
reverse at any frame. The camera therefore *commits*: it prepares for
the direction it is going, restarts if that changes, and may not cross a
coarse boundary until the buffer for that exact crossing is ready. If it
is not ready, the camera stalls for a frame. Diagonal motion with the
two axes out of phase would cross a boundary every four frames,
alternating axes, so the nearer axis waits once for the other to catch
up and from then on both cross together, once every eight frames. The
preparation is spread over seven of those eight: five chunks of rows,
the entering column, the entering row.

**The vertical split.** A HUD on top and a playfield below need
different `$D011` fine-scroll values, and the VIC fires a bad line
whenever `(raster & 7) == YSCROLL`. Write the new value on a line whose
low bits already match and you get a late bad line and a garbled row;
cut a row before its eighth line and the VIC shows the same row twice.
The layout is two HUD rows, a blank spacer row, 22 playfield rows in
24-row mode, the write at line `$48`, or `$49` when the value is zero.
Worked out on paper, then verified on the machine in all eight phases
by freezing the emulator and reading the floor's edge in 36 columns:
`y(fy) = y(0) - fy`, exactly.

## What the machine said

The spike ran, and the machine corrected the paper. In order of how
long each took to find:

- The multiplexer table had no terminator before C built one, so the
  IRQ walked into the rest of BSS and used what it found as sprite
  slots: `sta $d000,x` with garbage in X sprayed the VIC registers,
  `sta $47F8,x` overwrote the hero's pointer and six HUD characters. A
  vanished hero, wrong colours, garbled text. One missing `memset`.
- The split interrupt waited for its line with `cpx $d012 / bne`. When
  it landed one line late, sprite DMA is enough, it spun until the next
  frame's line: 19 600 cycles inside an interrupt, and a frame drawn
  with the HUD's scroll value. That was the "background out of phase
  with the sprites" the human reported. Fire a line early, wait with
  `>=`.
- The first profiler used raster-line deltas, which alias above line
  255; its numbers were fiction. A free-running CIA timer replaced it,
  with a self-test (200 empty loop turns, an empty probe) that made the
  later numbers believable. The probes cost 450 cycles each, six a
  frame, 14% of the budget: they went behind a compile flag.
- A lost-frame counter counted only frames that were skipped outright.
  A second counter for frames that arrived *late*, the flag already set
  when the loop came round to wait, found 20% of frames late in a build
  the first counter called clean. Late is the real metric.

And the finding that shaped every line of game code afterwards: **cc65
promotes byte arithmetic to `int`**, keeps locals on a software stack at
forty cycles a touch, and turns `r * 40` into a subroutine. The edge
fetch was 27 000 cycles for 62 cells; the drone update 18 000 for
twelve. The same work in assembly, byte-indexed, was a quarter of that.
The rule adopted: anything per object or per cell is assembly, or C
with static globals and no `int` in sight; C keeps what happens once a
frame.

The verdict, with counters rather than eyes: 8-way scrolling at one
pixel a frame with colour and a HUD split, true 50 fps, eight
multiplexed sprites, one late frame in 1 057 on a route built to
provoke stalls. Twelve sprites: 8% lost. The engine's worst frame costs
about 8 000 cycles; most cost nothing. About 11 000 remain for a game.

## Making it a game

Then came the part the spike had deliberately postponed, in big steps,
one measurement each, because the human was paying for the tokens.

**Physics.** The human's first remark when the drones flew past: "they
lack physicality". Gravity every other frame to a terminal speed, a
jump measured at 42 pixels up and about 28 along, landing snapped to
the tile top, walls probed at waist height. The solid-tile test is 90
cycles in assembly and it is the unit of cost for everything else: a
twelve-pixel object on 32-pixel tiles needs three probes a frame, not
six. The camera follows through a dead zone, which is what makes the
diagonal alignment stall invisible.

**Art the author can see.** Sprites in this toolchain are ASCII in a
Python file and are only ever seen 21 pixels tall in a screenshot,
which is to say never. `assetsheet.py` renders them magnified in the
game's own colours; the hero's four run frames were judged as a strip
before they were compiled, and six right-facing drawings became twelve
frames by reversing each row of text.

**Shots, contact, HUD.** Four shots are objects in the same
structure-of-arrays as the enemies, through the same multiplexer, now
with a sprite pointer per object. Shot-against-enemy and hero-against-
enemy are byte range tests in assembly: "shot minus walker plus six must
be 0..20 with a zero high byte". Enemies explode through three frames
and drop back in from above the view.

**A designed level.** The procedural cave was scaffolding. The level is
now a 64x32 drawing in the asset file, rock and air and decorations,
with the mossy tops, the undersides and the side walls derived at build
time. Its four areas follow the four colour bands, and every gap is one
tile or less because the jump had been measured.

**A boss.** A 24x42 mech cut into four sprites, one object to C and four
to the multiplexer. Nothing in the assembly knows the word boss: the
shot test covers "every target below the first shot", and the only
branch is which counter a hit bumps.

## The budget, re-earned

The boss build lost 116 frames in a minute. The profile build named the
culprits, and none of them was the boss.

The score went through ten 16-bit divisions on every kill, cc65's
`udiv` at 700 cycles each: doing well cost the player 8 000 cycles and
a lost frame. The score is five decimal digits now, incremented with a
ripple carry. A death set eight enemies exploding on the same countdown,
so sixteen frames later eight spawns landed together. Staggered.

And the frame itself: with eight walkers, shots, boss, contact tests
and HUD, the loop ran *slightly over one frame on most frames*: 62% of
frames late, almost none lost, which is exactly what "1.02 frames each"
looks like in those two counters. The spike had said eight objects fit
and twelve did not; the game had crept to eleven without anyone
deciding it. Six walkers, then. Zero lost frames on the plateau and in
the arena, 5% late in both. The object count is a budget line, not a
default.

## What the diary is really about

Every one of the hard problems above was found by a number, not by
looking. The human looked, and their reports ("out of phase", "flashing
box", "lacks physicality", "very fluid, great sprites") were the
prompts; the diagnosis every time came from a counter, a frozen capture
with RAM read in the same session as the screenshot, or a probe that
had first been proven against known work. Twice the numbers were wrong
and the wrongness was itself found by a number: the raster deltas by
the CIA self-test, the shots that "died in their first frame" by
noticing the addresses came from the previous build's label file.

The agent's actual limitation was not the 6502. It was the temptation
to trust a reading without asking what produced it, and the cure was
the same each time: one more measurement, taken more carefully than the
last.

*Sources: `ironvein/DEVLOG.md`, the commits from "the 8-way scrolling
engine spike, and its verdict" onward, and the human's interruptions.*
