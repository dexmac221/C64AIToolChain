# FULGOR — development log

An 8-way scrolling action game in the Turrican II class: the first level
as a demo. Started 2026-11-10 on Claude Fable 5.2 (Claude Code), in the
repository that already held IRON VEIN. Raw notes, kept as the work
happens, dead ends included.

## Day 1 — what "the class of Turrican II" asks of this repository

The human asked for an analysis of the project and then for "a high
level C64 game, like a Turrican 2, renamed — a level 1 demo, not the
whole game". IRON VEIN had already proved that the toolchain can scroll
in eight directions at 50 frames a second. Read against Turrican, what
it could not do was a list:

| | IRON VEIN | wanted |
|---|---|---|
| speed | 1 pixel a frame (buffer prepared over 7 frames) | run at 2, fall faster |
| background | black | a sky in raster bands |
| hero | one sprite, 21 pixels | two stacked sprites, ~40 pixels |
| collision | 32-pixel blocks | by character cell, thin ledges |
| budget | 6 enemies, ~5% late frames | room for weapons and music |

The causes were in the code and in IRON VEIN's own diary: camera and
hero in C (2 000 and 1 500 cycles), the copy through `(zp),y`, interrupts
through the KERNAL, the hero's sprite written outside the multiplexer.

So: a new directory, a new engine on the ideas that survived (two screen
buffers, colour per world row, the HUD split at line $48/$49), with the
whole frame in assembly. Art, level and music original; of Turrican only
the class.

### The art: one texture, sixteen tiles

Colour RAM cannot be double-buffered, so the fourth ink of a character
belongs to the world row (IRON VEIN measured why). That makes the level
banded: green on the surface, yellow in the caves, cyan in the crystal
gallery. `$D022/$D023` are brown and light red for the whole level.

Terrain is not drawn tile by tile. `assetgen.py` generates one seamless
32x32 stone texture (Voronoi cells, lit from the top left) and cuts the
sixteen edge variants out of it with rules for outline, grass and
shadow; the level says "rock here" and the exposed sides pick the tile.
Sixteen metatiles cost 36 characters. The whole level is a small
program too: a ground profile, caves carved as rectangles, ledges and
decoration by rule. A preview renderer (`--preview DIR`) draws the map
and the sprite sheet in the game's colours, so the art was judged
before a line of 6502 existed.

Palette test, eight candidates side by side: brown/orange (the first
idea) was mud; brown/light red reads as red canyon rock and keeps a
shadow tone that black outlines would lose.

The first hero had legs rasterised from a skeleton (thigh and shin
angles). At 12 fat pixels wide it produced broken sticks. The run cycle
is four hand drawings in the vocabulary of the standing legs: near leg
three pixels wide, far leg two with a shadow line, white knee pads.

### The engine ran at the first build — and was too slow

First light with scrolling, hero and collision correct. Then the
counter: worst frame done at raster line 219 of 312 with the hero
alone. A profiler of five checkpoints (raster lines since the frame
interrupt, last and worst) named the costs:

- **the multiplexer build: 1 950 cycles to show the hero alone.** It
  scanned and sorted all 16 object slots and padded the table to eight
  entries. Now it keeps a list of live objects in Y order from frame to
  frame (one insertion step each, usually none), and the frame interrupt
  enters an unrolled ladder at the rung for the number of sprites that
  exist.
- **the copy: 4 200 cycles per half.** Now three bands of 13 columns and
  21 rows instead of 22 (the 22nd row never reaches the 24-row window:
  worked out from where the split puts the first row). The copy leaves
  the entering row and column alone, so the edge writers can run first
  and the units are dealt out by how many frames remain before the
  crossing: one a frame at running speed, two when a fall leaves no
  time, never more — a wait at the boundary is kinder than a lost frame.
- **side probes: 115 cycles each.** One routine works out the column and
  its bit once and reads the map again only when a probe enters another
  metatile.

| | idle | running | run + jump + fire |
|---|---|---|---|
| first build | 64 lines | ~120, worst 149 | worst 219 |
| after | 42 | ~94, worst 120 | worst ~195 |

Zero late frames throughout. The worst case is now a fall: a vertical
step every two frames is two bands of copy in one of them.

The level's first draft had a pit three metatiles deep. The jump is 49
pixels. Rules written into the level file: every step up is one
metatile, every gap one metatile (two if the far side is lower), shafts
three wide with ledges on alternate sides.

### The sky

Background colour per world row, like colour RAM: a gradient outdoors,
black underground, for free in the level's data. Changing `$D021` on
the last line of every character row needs an interrupt that the sprite
multiplexer cannot delay, so it is an NMI: CIA 2 timer A, 504 cycles
(eight lines) apart.

The chain has to start on a known cycle every frame. Timer B runs one
PAL frame long (19 656 cycles), started once at boot in step with the
beam — a polling loop of 62 cycles slides one cycle a line until two
reads fall in the same line — and therefore fires at the same raster
position for ever. Its handler reads the timer, slides away its own
jitter and starts timer A to the cycle.

What the machine corrected, one measurement each (breakpoints through
the monitor print raster line and cycle; one connection for all the
samples, because VICE 3.7.1 wedges when connections churn):

- a `bne` that should have been a `jmp` disarmed the chain whenever the
  first delay was under 256 cycles;
- **YSCROLL 5 makes line $55 a bad line** and the handler arrives 40
  cycles late. First attempt: read the timer and slide anyway — but
  with the underflow placed before the stall the timer read fell
  before or after it depending on jitter. The bad line is itself a
  synchronisation (the CPU is released on cycle 55 whatever it was
  doing), so that case has its own handler: open the playfield at once,
  slide afterwards;
- **with YSCROLL 7 the first band is due 46 cycles after the start.**
  On paper it nested into the frame handler's exit and, with sprite
  DMA in the way, could be lost against an NMI line still held down by
  timer B: acknowledge B before starting A, save A in two different
  places, keep every handler in one page so only the vector's low byte
  changes, and let the split interrupt preload timer A's latch while
  it is stopped, so starting it is one write.

The same NMI solved an old wart. The row where the HUD's YSCROLL meets
the playfield's shows a ragged strip (the first row comes and goes).
The split now switches ECM on over multicolour — an invalid mode, black
— and the frame NMI switches it off on a fixed line. The playfield has
a clean top edge at line $56 in all eight phases.

Measured: band writes start between cycle 52 and 62 of the row's last
line in every phase, i.e. in the right border or the blanking; the
opening write on cycle 59 of line $55, every frame. (Corrected at the
end of the day, see "the band writes, checked on screen": those cycles
are where the store *instruction* starts, in VICE's numbering, probably
from an earlier calibration.)

### Where the budget stands

Hero, four shots, sky: worst frame ~195 lines of 312 while falling and
firing. The sprite table costs about 230 cycles a sprite however it is
arranged (two 16-bit subtractions, clipping, five table writes, the X
high bit); a single-pass version was written out on paper and came to
the same number. So the object count is a budget line again: ten or
eleven sprites, not sixteen.

## Day 1, evening — the game layer, and what the human saw

Enemies (three kinds, all low flyers: a 21-pixel sprite standing on the
ground is under the line of a gun held 24 pixels up, and the first
skimmers could not be hit), gems and power units taken out of the map,
HUD, energy and lives, checkpoints, lava, a title with the name in
doubled sprites, a SID driver with wavetables and a tune in A minor
(`music.py`; the pitches of each bar were checked by recording VICE to
a WAV file and looking at the spectrum — nobody has listened to it yet).

The budget again. With enemies and shots the worst frame reached line
253 of 312. The profiler said the sprite table cost 340 cycles a
sprite, not the 230 estimated: three passes over absolute variables. It
is one pass on zero page now (~225), writing the table in last frame's
order and letting the list catch up. The copy went from three bands to
four and falls were capped at 3 pixels a frame, so that the worst
scroll frame is two bands of ten columns. Worst frame in play now:
about 213-233 lines with music, 0 late frames.

**The right-hand flicker.** The human, watching: "a scrolling problem on
the right side". Screenshots taken at random showed nothing; a checker
stopped anywhere by the monitor showed nothing either (VICE stops at
the frame's end, before the scroll code has run). `check.py` stops at
the entry of `_commit`, when the main loop has finished writing, and
compares the buffer the VIC is showing with the map: **14 samples in 60
wrong, always columns 30..39**, exactly on the frames where a step was
taken. The step from one buffer to the next is decided a frame before
it is seen, and in that same frame the engine started the next
preparation — in the buffer still on screen. Its first band is the ten
columns on the right. IRON VEIN never did this (it ran its preparation
before deciding the step); the new unit scheduler did.

Fix: three screen buffers in rotation. The preparation reads the new
front buffer and writes the one that is neither it nor the one on
screen. 24 copy routines instead of 16, 1 KB more code, nothing lost in
speed. After: 0 wrong samples in 70 over 49 camera positions.

The same checker, run while the human was watching, made the game crawl
at a frame a second — every sample is a breakpoint and a 3 KB dump —
and one run died leaving its breakpoint set. Rule already in this
repository's notes, relearned: never touch the monitor while someone is
looking at the picture.

## Day 1, night — closing checks, the film, the article

The human said "ottimo" and asked for an article, as for the other
games in this repository (written by Claude Opus 5.5 from this diary;
the game itself is Fable 5.2's. An earlier version of this diary and
the first commit said "Fable 5": the session transcript records
`claude-fable-5-2` for every step of the development).

Everything below ran on a **second VICE on monitor port 6511**, so the
one the human was playing stayed untouched. `check.py` got `--port` and
`--start` (press fire on the same connection: one connection per VICE
session, a fresh VICE for each run).

- Left and falling, the two cases the BrokenPipe runs had left open:
  from spot 3 holding left (off the plateau and down) 0 wrong in 50
  samples over 38 camera cells; from spot 2 holding left along the
  ground 0 in 60 over 33. With the 70 on the right: **0 wrong in 180
  over 120 positions.** The runs that "hung" earlier had only a short
  timeout: a sample costs about 1.7 s (three breakpoint stops and a
  3 KB dump), so 80 samples need more than 100 s.
- `fulgor_demo.gif` filmed again with the same 44-second script, after
  the fix (294 frames). Telemetry over the whole film, title included:
  9 late frames in 2 809. Not attributed one by one; the film has
  about that many full redraws (title, start, five teleports, the
  respawn after the lava) and an earlier test gave one late frame for
  one teleport.
- VLM pass (`vlm_look.py -m gemma4:31b-cloud`, the local gemma models
  are gone) on three frames: scene, HUD digits and sprites described
  correctly, no defects; lava called "pink/yellow textured tiles".
  Answers in `evidence/vlm_review.md`. A still frame could not have
  shown the right-hand flicker in any case.

**How long it took**, from the session transcript: first message at
16:01, last build of the game at 18:31 (Italian time), with a pause of
37 minutes in between: about two hours of work. The human's
interventions were two pointers about the scrolling (the right-hand
flicker, then the crawl caused by the checker).

### The band writes, checked on screen

A reviewer (another Claude instance, through the human) doubted the
sentence "cycles 52-62, inside the right border": in Bauer's numbering
the 38-column window closes around cycle 55, so a write at 52-54 could
colour the last pixels of a row's last line. Fair, and the sentence was
loose twice over: the numbers were breakpoints on `sta $d021`, i.e. the
instruction's first cycle in VICE's numbering (from 0), not the write,
which is its fourth cycle; and they probably come from an earlier
calibration (D1N_ADJ went from 17 to 15, which moves the writes two
cycles later).

Measured again on the shipping build, on a second VICE (port 6511):

- breakpoints on `nb_w`, eight camera phases with a few frames between
  them: the store starts on cycles 55-60 of the row's last line in every
  phase, so the write lands on 58-63. A first, messier sampling also saw
  starts up to cycle 3 of the next line - sprite fetches delaying the
  NMI - which still writes in the left border, well before the window
  opens on that line;
- on screen, which is what settles it: 32 screenshots over all eight
  phases on the plateau and in the canyon (`evidence/band_edges/`,
  checker `evidence/band_edges.py`). At 40 band edges whose right end is
  open sky, the last visible pixels of the row's last line never show
  the new colour; at 160 whose left end is open sky, the first visible
  pixels of the next row never show the old one. (A first version of
  the checker skipped exactly the edges it was meant to catch - it
  required the colour to change at both ends - and flagged the grass
  line instead; the version kept here finds the edge in the middle of
  the line and judges each end separately.)

## Open

The human's verdict on feel, picture and tune. A boss and the level's
end beyond "reach column 122". The beam and the wheel that make
Turrican Turrican. Waits at the boundary during falls (39 in 11
seconds of a test with a long fall): the camera lags a few pixels.
