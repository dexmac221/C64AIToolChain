# The Commodore 64 Toolchain Agent Benchmark: Claude Code with Fable

*Two games, one session ladder: a Boulder Dash tribute with an autonomous player, then a pixel-smooth scrolling shoot-em-up with raster interrupts, double buffering, and a SID music engine.*

---

## Author's Preface

**By the human in the loop**

This workspace has become an informal benchmark for AI coding agents. The rules are always the same: a real Commodore 64 toolchain (cc65, VICE with remote monitor, Python tooling, a local vision model as the agent's eyes), a mandatory build-reload-look loop, and a human who plays the results and pushes back.

Earlier sessions produced Meteor Storm and Dreadline, each documented in its own article. Dreadline set the previous technical ceiling: mixed C and assembly, a custom character set generated from bitmap art, multicolor sprites from an ASCII asset pipeline, VIC bank relocation.

This article documents the next two rungs, built with **Claude Code running the Fable model**:

1. **Boulder Rush** — a Boulder Dash tribute. The challenge here is not raw VIC-II tricks but *system fidelity*: authentic cave physics, and a new benchmark dimension — the game plays itself, driven by an external agent process through the emulator's monitor port.
2. **ION RIFT** — a horizontal shoot-em-up designed explicitly to go past Dreadline: pixel-smooth hardware scrolling, a raster-interrupt screen split, double-buffered screen RAM, procedural multicolor terrain, and an in-game SID music engine.

As always, what makes the sessions interesting is not the final PRG files. It is watching the agent debug the machine, the toolchain, and itself.

---

## The Ladder So Far

| Rung | Project | Techniques |
|------|---------|-----------|
| 1 | snake, pong, invaders... | conio character output |
| 2 | Stellar Assault | 8 hardware sprites, SID sfx, attract mode |
| 3 | Frogger | multicolor bitmap mode, custom linker config |
| 4 | Meteor Storm / Dreadline | C + assembly, generated charset and sprites, VIC bank 1 |
| 5 | **Boulder Rush** | cave physics engine, custom charset, **autonomous agent player** |
| 6 | **ION RIFT** | **smooth scroll, raster IRQ split, double buffer, SID music** |

---

## Step 1: Boulder Rush

### The game

A single evening's target: a faithful Boulder Dash-style cave game. Character mode with a custom charset (dirt, boulders, gems, brick, steel, the miner, the exit), 40x22 caves generated from fixed seeds, a HUD, a timer, lives, and SID sound effects.

The physics engine is the heart of any Boulder Dash clone, and it is a beautiful fit for a 1 MHz machine because it is pure cellular logic:

- boulders and gems fall when unsupported
- they *roll* off "rounded" objects (boulders, gems, brick walls) when a side and its diagonal are free
- they kill only while *falling* — you can walk under a resting boulder
- the cave is scanned bottom-up so an object moves exactly one cell per tick, with per-tick parity flags so a rolled object is not processed twice

### Bug one: hearts instead of letters

The first VLM look at the title screen reported garbage: hearts, clubs and box-drawing symbols where text should be. The screen RAM dump showed the text was *written* correctly — the charset itself was wrong.

The cause is a classic cc65 detail: conio emits screen codes $41-$5A for uppercase text, which are letters only in the C64's *second* character set (lower/upper at $D800 in char ROM), not in the uppercase/graphics set at $D000 that had been copied. One changed source address fixed the entire alphabet.

### The new benchmark dimension: the game plays itself

The toolchain already had a dual-loop agent architecture (a fast heuristic "System 1" and an LLM "System 2") built around a convention: an *agent input byte* at $033C, with the same bit layout as the CIA joystick register.

Boulder Rush adopted the convention, and a new tool was written: `demo_bot.py`. It is a complete autonomous player that:

1. reads the whole cave state from screen RAM through the VICE remote monitor
2. rebuilds the grid (each tile is one screen code — the screen *is* the game state)
3. plans with a weighted BFS (a cost penalty for stepping under a boulder)
4. picks targets: open exit, else nearest reachable gem, else dig toward gems
5. writes one direction bit to $033C per planned step

### Bug two: the tap that moved four cells

The first bot runs oscillated forever below a gem: down three cells, up one, down three, up one. The instrumented step-by-step debug made the cause obvious: a "tap" sent through the monitor has no reliable duration. Monitor connections pause the emulator, so a 0.18-second hold could last anywhere from one to four game moves. The bot overshot its target every single time, in both directions.

The fix was not to calibrate timing — it was to change the *contract*. The game now consumes and clears $033C when it polls input, and polls only when the move cooldown has expired. The input became **edge-triggered**: one monitor write is exactly one move, no matter what the host timing does.

The next run collected a gem every two seconds, 109 planned moves without a single stall, died once under a boulder (authentically), and kept playing on its next life while the session monitored gem-count milestones from the outside.

---

## Step 2: ION RIFT

### Designed to out-scroll Dreadline

Dreadline scrolls its deck one character at a time — visibly chunky, 8 pixels per step. The explicit goal for ION RIFT was everything Dreadline does not do:

| Technique | Dreadline | ION RIFT |
|-----------|-----------|----------|
| Scrolling | 1 char per step | pixel-smooth, $D016 fine scroll at 50 fps |
| Screen | single | double-buffered ($4400/$4800, flip via $D018) |
| Raster IRQ | none | 2 interrupts per frame: steady HUD / scrolling playfield |
| Terrain | pre-authored bitmap tiles | procedural, generated column by column |
| Audio | sfx | 2-voice SID music engine + sfx on voice 3 |

### The engine

The screen is split by raster interrupt: three fixed HUD rows (score, high score, shields, speed), a separator line, then 22 rows of playfield that scroll with the VIC-II fine-scroll register.

The scroll cycle is the classic C64 dance:

- every frame, fine scroll decreases by one pixel
- mid-cycle (fine = 4), an assembly routine copies all 22 rows one character left from the *visible* buffer into the *hidden* one, and a freshly generated terrain column is written into the new rightmost slot
- when fine scroll wraps, the interrupt flips $D018 to the prepared buffer, colour RAM (which has no second buffer on a C64) is shifted in one assembly pass, and the terrain ring model advances

Terrain is procedural: a random walk for ceiling and floor thickness with a guaranteed flyable corridor, occasional blue towers, and sparse hires stars in the gap. Multicolor characters give the plating a metallic look with per-tile accent colours; the game never stores a map — the world is generated column by column, forever.

The memory layout puts VIC bank 1 ($4000-$7FFF) at the centre: two screens, the custom charset, and sprite frames, with program code kept below $4000 and constant data above at $8000 — a linker config with an explicitly filled gap so the PRG loads across the video area it will later overwrite.

### Bug three: the same PETSCII trap, one layer deeper

The first boot looked glorious — scrolling plates, towers, stars, sprites, music — but every HUD *letter* was invisible while every digit rendered fine. Same family as the Boulder Rush bug, different layer: cc65 translates uppercase letters in C string literals to PETSCII $C1-$DA. Digits pass through unchanged. The game's own text renderer now folds $C1-$DA down to screen codes $41-$5A.

The signature is worth remembering: *digits fine, letters blank or wrong = PETSCII/screen-code mismatch.*

### Verifying motion with a vision model

A static screenshot cannot prove smooth scrolling. The toolchain's `vlm_look.py` has a multi-frame motion mode, which turned out to hardcode a retired model name — so the session patched the tool itself to honor the `-m` flag, then captured four frames 0.4 seconds apart.

The motion analysis confirmed: terrain scrolling steadily leftward, HUD rows perfectly static across frames, sprites moving coherently, no tearing reported. The same run also showed the demo autopilot surviving multiple game-over cycles with the high score carried over.

### The human ear

The first music pattern was serviceable but polite. The human feedback was precise: *"more tense, like R-Type."* The rework: an octave-hammering minor-key bass on the pulse voice, a faster step rate, and the lead arpeggio turned to sawtooth, cycling Am-F-G before leaning on an E major turn — the dominant that never quite resolves. Tension, in three tables of bytes.

### The human eye, and a one-frame bug

The subtlest bug of the whole session was found by the human, not the tooling: *"it scrolls, but it stutters — maybe a double buffer problem?"* Static screenshots looked perfect; even multi-frame motion analysis passed. The cause was one frame of timing skew: the main loop runs early in the frame, *before* the raster split, so a new fine-scroll value took effect immediately — while the $D018 buffer flip it belonged with was latched by the interrupt one frame later. Result: once per coarse step, one frame showed the old buffer at the new scroll position, a 7-pixel back-jump at 6 Hz. The fix latches the fine scroll and the flip together in the top interrupt, and the games' text screens went fully static (text inside a fine-scrolled zone wobbles by construction — so ignition now happens when the game starts).

The rework also made the engine honest about its budget: colour RAM became static (scrolling its 880 playfield bytes costs more than a PAL frame allows), the row copy was spread across four frames, and a missed-frame counter at $033D turned "feels smooth" into a number: **0.1% missed frames** over thirteen minutes — 38 overruns in ~39,000 frames. A missed frame here means exactly one thing: the raster interrupt ticked a new frame while the main loop was still working, i.e. a game-loop deadline overrun. It does not count interrupt failures (the IRQ always runs) or emulator pauses from monitor connections (emulation is frozen then, no frames tick).

Static colour RAM deserves a precise word, because an earlier draft claimed "per-tile accent colours" and that is no longer true. In the final engine every playfield cell holds one colour value, so every multicolor "11" pixel on screen is the same light blue. The terrain still reads as varied because a multicolor character has *four* colour sources and only one comes from colour RAM: the background and the two shared registers ($D022 dark gray, $D023 mid gray) are free, per-register, and cost nothing to scroll. Ceiling, floor and towers differ by pattern density across those four colours — variety by texture, not by hue. That is the actual compromise, and it is the same one shipped scrollers of the era made.

### A second agent, and a latency lesson

Like Boulder Rush, ION RIFT plays itself — but a 50 fps shmup broke the Boulder Rush recipe. Reading a kilobyte of screen RAM per decision is too slow and too hard on the emulator's fragile monitor port. The replacement contract: the game publishes six bytes of telemetry per frame (ship, corridor bounds, nearest enemy, state), and takes steering through a *hold* input byte with an in-game watchdog — it expires after half a second unless the agent's heartbeat bit refreshes it, so a dead agent can never pin the ship.

The agent memory map, for the record: `$033C` edge-triggered input (fire), `$033D` missed-frame counter, `$033E` hold input (steering), and the telemetry block at `$0340`: ship Y, first flyable pixel line under the ceiling at the ship's column, top pixel line of the floor or tower at that column, nearest enemy Y ahead ($FF if none), its horizontal distance in half-pixels, and the game state (0 title, 1 playing, 2 game over).

The watchdog works by *value change*, not by bit inspection: the game merges only the low five bits of `$033E` into the input, and any change of the raw byte — including the agent toggling bit 7, which the input path never sees — resets a 25-frame time-to-live. Unchanged byte, TTL counts down; at zero the game clears it. Re-sending the same direction with a flipped heartbeat bit therefore refreshes the hold without altering the input, and the real joystick never touches this byte at all.

`ion_bot.py` flies the corridor, dodges, fires, and restarts its own game overs at ~3 Hz. It plays honestly but mortally: with a 0.3-second reaction time, a dart crossing 60 pixels between decisions is often unavoidable. The in-game demo autopilot, reacting every frame, flies far better — a clean measurement of what reaction latency costs in an action game, and a hint of why the fast loop of the toolchain's dual-loop architecture has to live close to the metal.

---

## What the Loop Caught

Every one of these was found by the toolchain loop — build, reload, screenshot, *look* — plus the emulator's monitor as a memory microscope:

| Symptom | Instrument | Cause |
|---------|-----------|-------|
| Hearts instead of letters | VLM + screen RAM dump | wrong char ROM half copied |
| Cave bottom missing | PIL pixel analysis | screenshot taken mid-draw (transient) |
| "VICE went crazy, CPU pegged" | jiffy-clock speed measurement | emulator launched without audio, speed regulation lost |
| Bot oscillating forever | instrumented step logging | monitor taps have unreliable duration |
| HUD letters invisible, digits fine | own eyes + VLM | PETSCII literals vs screen codes |
| Scroll smoothness | patched multi-frame motion VLM | verified, no fix needed |

The quieter lesson: several fixes landed in the *toolchain*, not the games — the reload tool's wrong default entry address, the motion mode's hardcoded model, the emulator launch environment on a snap-packaged desktop. A benchmark that lets the agent repair its own instruments measures something a static benchmark cannot.

One honest open item: the very first ION RIFT build was once seen dropping to the BASIC `READY.` prompt mid-demo — the signature of a stray BRK. A breakpoint trap was planted on the KERNAL BRK path and the demo left running under watch; the current build has not reproduced the crash in over forty minutes of continuous autoplay. The trap stays set. Retro development keeps you humble.

---

## Technical Summary

### Boulder Rush

| Area | Choice |
|------|--------|
| Mode | character mode, custom charset at $3800 |
| Physics | bottom-up cave scan, parity move-flags, rounded-object rolling |
| Caves | seeded procedural, 40x22; exit opens at 60% of spawned gems |
| Agent I/O | $033C edge-triggered input byte |
| Autonomous player | `demo_bot.py`: screen-RAM read → weighted BFS → one write per move |
| Binary | ~8 KB |

### ION RIFT

| Area | Choice |
|------|--------|
| Video | VIC bank 1: screens $4400/$4800, charset $5000, sprites $5800 |
| Scrolling | $D016 fine scroll, 50 fps, coarse shift in ca65 assembly |
| Buffering | double-buffered screen RAM, $D018 flip in the raster IRQ |
| IRQ | 2 raster interrupts/frame (HUD split + playfield), KERNAL chained once |
| Terrain | procedural ring-buffer model, multicolor tiles + hires stars |
| Assets | single `assetgen.py` pipeline: MCM/hires tiles + MCM sprite frames |
| Audio | 2-voice pattern engine (pulse bass, saw lead) + sfx voice |
| Agent I/O | $033C edge fire, $033E watchdog hold, telemetry at $0340 |
| External agent | `ion_bot.py`: 6-byte telemetry read → steer/dodge/fire at ~3 Hz |
| Binary | 31.9 KB PRG on disk; ~9.8 KB real code+data (the rest is the zero-filled VIC bank gap the loader streams over) |

---

## Conclusion

Two games in one arc. Boulder Rush proves the agent can implement somebody else's *rules* faithfully — cellular physics whose edge cases generations of players know by heart — and then step outside the machine and play its own game through the emulator's monitor port. ION RIFT proves it can climb the hardware ladder deliberately: interrupts, fine scroll, buffer flips and colour RAM compromises, chosen and combined the way C64 programmers actually did it.

The benchmark continues.

---

**Tags:** `#AI` `#RetroComputing` `#Commodore64` `#GameDev` `#LLM` `#cc65` `#6502` `#VICII` `#ClaudeCode`
