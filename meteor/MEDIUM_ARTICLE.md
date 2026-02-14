# When AI Writes a Commodore 64 Game From Scratch: A Creative Autonomy Experiment

*An AI agent designs, codes, debugs, and ships a complete retro game — with almost zero human intervention.*

---

## Author's Preface

**By the human in the loop**

I've been building a toolchain for developing Commodore 64 games with AI assistance: a set of Python scripts that connect an LLM agent to the cc65 cross-compiler, the VICE emulator, and a Vision Language Model for visual feedback. The agent can write C code, compile it, launch the game in an emulator, take screenshots, and analyze what it sees — a full development loop.

After using this setup to create several classic game clones (Space Invaders, Arkanoid, Pac-Man, Pong, Tetris…), I wanted to push the envelope. **Could the AI go beyond reproduction and actually create something original?**

Not original in the way a human game designer would — I'm not claiming artificial creativity. But could it analyze existing game mechanics, identify interesting combinations, design a coherent new game concept, implement it in a constrained environment (6502 assembly-level C, 64KB RAM, 16 colors, 8 sprites), and then autonomously debug it to a working state?

This is a different kind of benchmark than ARC-AGI or SWE-bench. Those test pattern recognition and isolated coding tasks. What I wanted to test was **sustained autonomous problem-solving across multiple domains simultaneously**: game design, systems programming, hardware constraints, memory layout, visual debugging, and iterative refinement — all in a single unbroken session.

The rules were simple: I'd give the initial prompt and observe. I'd only intervene if the AI got completely stuck or missed something I could see. As it turned out, I intervened exactly once — to hint that the VLM might not be catching sprite corruption. Everything else — the design, the code, the debugging, the memory layout fix — the agent did on its own.

Here's what happened.

---

## The Challenge

The workspace already contained about 15 C64 games, all written in C using the cc65 cross-compiler. Each game was a faithful clone of an arcade classic: Space Invaders with 55 character-based aliens and 6 hardware sprites, an Arkanoid with 8.8 fixed-point ball physics, a Pac-Man with maze pathfinding — real implementations, not toys.

The prompt was deliberately open-ended:

> *"I'd like to understand if you're capable of creating a new game — at least mixing existing game mechanics. Something original that shows your ability to combine ideas."*

No specification. No design document. No pseudocode. Just: *surprise me*.

---

## Phase 1: Analysis and Design (5 minutes)

Before writing a single line of code, I needed to understand what was already in the workspace. I read the full source of `arkanoid.c` (940 lines) and `invaders.c` (1208 lines), studying their architecture patterns:

- **Parallel arrays instead of structs** — idiomatic for 6502 performance
- **Direct VIC-II register manipulation** — `*(unsigned char*)0xD015` for sprite enable
- **SID chip sound** — three-voice synthesis through memory-mapped registers
- **Custom character sets** — copying ROM charset to RAM, then overwriting specific character slots with custom pixel data
- **Sprite data blocks** — 64-byte aligned data at `$3000` for hardware sprites

From this analysis, I designed **METEOR STORM**: a mashup that borrows the best mechanics from three classic games:

| Mechanic | Inspired By |
|----------|-------------|
| Large meteors split into 2 small ones when destroyed | *Asteroids* |
| 4 destructible shield bunkers protect the player | *Space Invaders* |
| Power-ups drop from destroyed meteors (shield repair, double shot, screen bomb) | *Arkanoid* |

Added on top: a scrolling parallax starfield (3 speed layers, 20 stars), a mystery UFO bonus ship, a combo scoring system, progressive wave difficulty, and a full demo AI that plays the game autonomously.

---

## Phase 2: Implementation (1581 Lines of C)

The entire game was written in a single file — `meteor.c` — following the architectural patterns observed in the existing codebase. Here's what went into it:

### Memory Map

```
$0400-$07FF  Screen RAM (40×25 characters)
$07F8-$07FF  Sprite pointers
$3000-$30FF  Sprite data (4 sprites × 64 bytes)
$3800-$3FFF  Custom character set (ROM copy + 14 custom chars)
$D000-$D02E  VIC-II registers (sprites, scroll, colors)
$D400-$D418  SID registers (3 voices)
$D800-$DBFF  Color RAM
```

### Custom Graphics

14 custom characters were designed as byte arrays directly in C — each character is 8 bytes defining an 8×8 pixel grid:

- 4 animation frames for large meteors (left/right halves × 2 frames)
- 2 animation frames for small meteors
- 2 explosion frames
- 2 star types (dim/bright)
- Shield block
- 3 power-up icons

4 hardware sprites were defined byte-by-byte in `init_sprite_data()`:
- Triangular fighter ship
- Thin bullet line
- Diamond-shaped power-up
- UFO saucer

### Game Systems

The game runs several interlocking systems every frame:

1. **Meteor system** — 16 max concurrent meteors with independent position, velocity, size, and HP. Large meteors (2 characters wide) split into 2 small ones on destruction.

2. **Starfield** — 20 stars across 3 parallax speed layers, with bounds checking to avoid overwriting game objects.

3. **Shield system** — 4 bunkers made of custom character tiles, destructible per-tile by meteor impact.

4. **Power-up system** — Random drops from destroyed meteors, collected by sprite-to-sprite proximity check.

5. **Combo system** — Rapid kills build a multiplier, displayed as "COMBO x3!" with timeout decay.

6. **Sound engine** — 3 SID voices for shoot, explosion, and power-up effects, each with independent timers and envelope control.

7. **Demo AI** — Tracks the lowest active meteor, positions ship underneath, fires when aligned. Simple but effective enough to demonstrate the game.

### Compilation

First compile: **success, 11,016 bytes**. No syntax errors. The code compiled cleanly on the first attempt with `cl65 -t c64 -O`.

---

## Phase 3: The VLM Feedback Loop

With the game compiled, I launched it in the VICE emulator and began the verification loop:

1. **ASCII screen dump** via `ai_toolchain.py` — connects to VICE's remote monitor on port 6510, reads Screen RAM at `$0400`, and renders a text representation
2. **Screenshot capture** — takes a PNG via VICE's monitor `screenshot` command
3. **VLM analysis** — sends the screenshot to a local Ollama instance running `qwen3-vl` (8B parameters, Q4_K_M quantization) with a structured prompt asking about screen layout, sprites, text, colors, and visual issues

The VLM confirmed: *"Running Commodore 64 game with no visual flaws."* Ship visible, shields present, HUD readable, demo AI completing waves.

Four VLM cycles, all clean. The game appeared to work.

---

## Phase 4: The Human Intervenes (Once)

This is where the human operator made their single intervention:

> *"The VLM probably doesn't see that some sprites, particularly the meteors, are corrupted at certain moments. Can you check?"*

A fair point. The VLM captures are snapshots — they can miss intermittent rendering glitches. So I switched from visual analysis to **deep code review**, reading all 1,581 lines systematically.

---

## Phase 5: The Bug Hunt

### Round 1: Six Rendering Bugs

Reading through the code, I identified six issues:

1. **`erase_meteor()` was blind** — It cleared screen cells without checking if they still contained meteor characters. If a star or explosion had been drawn over the position, the erase would destroy the wrong graphics.

2. **`draw_meteor()` had no collision awareness** — It drew meteor characters without checking if the cell was occupied by a shield, HUD element, or another meteor.

3. **Large meteor boundary overflow** — 2-character-wide meteors spawning at x≥38 would write the right half off-screen.

4. **Incomplete shield collision** — Only the left character of large meteors was checked for shield hits. The right half could pass through shields.

5. **Starfield/meteor visual conflict** — Stars and meteors could overwrite each other, creating visual artifacts.

6. **`clear_explosions()` was a CPU hog** — Scanning all 720 cells (40×18) every 16 frames to find explosion characters. Replaced with a tracked explosion array (max 8 active) with per-frame timer updates.

I fixed all six and rebuilt. The game compiled with zero errors, zero warnings.

### Round 2: The Real Crash

But when I loaded the fixed game into VICE — black screen. BASIC `READY.` prompt. The program was crashing on startup.

The VICE monitor showed the CPU at `$E5D1` — deep in the KERNAL ROM's input loop. The program had executed and immediately returned to BASIC. Something was catastrophically wrong.

I checked the CPU register dump, the SYS address, the BASIC stub — all correct. The program should start at `$080D`. So why was it crashing?

Then I checked the one thing I should have checked first:

```python
d = open('meteor.prg','rb').read()
load_addr = d[0] | (d[1] << 8)   # $0801
end_addr = load_addr + len(d) - 2  # $3609
print(f'End: ${end_addr:04X}')
print(f'Sprite data at $3000')
```

Output:
```
End:  $3609
Sprite data at $3000
WARNING: Program extends past $3000 by 1545 bytes!
This OVERLAPS with sprite data area!
```

**The program was 11,786 bytes.** Starting at `$0801`, it extended to `$3609` — right through the sprite data area at `$3000` and into the custom charset at `$3800`.

When `init_sprite_data()` wrote the sprite pixel data to `$3000`, it was **overwriting the program's own compiled code**. When `init_custom_charset()` wrote to `$3800`, same thing. The program was destroying itself during initialization.

### The Memory Layout Fix

This is a classic C64 development problem. The standard cc65 linker config places code sequentially from `$0801` upward. Small programs (under ~10KB) fit below `$3000`. Our 12KB program didn't.

The solution: a custom linker configuration that creates a **gap** in the binary:

```
$0801-$080C  BASIC header (SYS 2061)
$080D-$083F  cc65 STARTUP runtime (51 bytes)
$0840-$2FFF  Zero-filled padding
$3000-$3FFF  Reserved for runtime data (sprites + charset)
$4000-$6DC9  Program CODE, RODATA, DATA
```

The key insight is that C64 `.prg` files are flat binaries — no segments, no relocations. The file is loaded byte-by-byte starting at the load address. If there's a gap in the memory layout, the file must contain the gap as explicit zero bytes, or the code after the gap will be loaded at the wrong address.

```cfg
MEMORY {
    MAIN: file = %O, start = $080D, size = $37F3, fill = yes, fillval = $00;
    HIGH: file = %O, start = $4000, size = $9000;
}
```

The `fill = yes` directive pads MAIN to its full size ($37F3 = $080D to $3FFF), ensuring HIGH's code lands exactly at `$4000` when loaded.

Final binary: **26,058 bytes** — 4KB larger than before due to the zero-fill gap, but now with a clean memory layout.

### Verification

```python
gap = prg_data[offset_3000:offset_4000]
zero_count = sum(1 for b in gap if b == 0)
print(f'$3000-$3FFF: {zero_count}/4096 zero bytes')
# Output: $3000-$3FFF: 4096/4096 zero bytes ✅
```

The gap is clean. Sprite data and charset will be written here at runtime without corrupting any code.

---

## Phase 6: It Works

Loaded in VICE. Black screen — then the title screen appears. Demo mode starts. The ship moves, meteors fall, shields get hit, explosions animate, the score climbs, waves advance.

ASCII screen dump from the monitor:

```
|SCORE:00060    DEMO              x2     |
|                                        |
|               ██                       |  ← large meteor (2 chars)
|                                        |
|      · ·     ·     ·                   |  ← parallax stars
|                                        |
|  ████      ████      · ██      ████    |  ← shields (one damaged)
|  █  █      █  █      █  █      █  █    |
|                                        |
|████████████████████████████████████████|  ← boundary
|                WAVE 1           04/08  |
```

VLM analysis: *"Running Commodore 64 game with no visual flaws. All elements correctly aligned, no flickering or color shifts."*

---

## What This Demonstrates

This wasn't a benchmark test. There's no score, no leaderboard, no controlled comparison. But I think it demonstrates something interesting about the current state of AI agents interacting with real development tools.

### What Went Right

- **Creative synthesis** — The agent analyzed existing game mechanics and combined them into a coherent new design, not just copying
- **Systems programming** — 1,581 lines of hardware-level C, writing directly to VIC-II registers, SID sound chips, and managing a 64KB memory map
- **Autonomous debugging** — The memory layout crash was found through systematic investigation: code review → binary analysis → memory map verification → linker config solution
- **Tool chain mastery** — The agent learned to use cc65, VICE monitor protocol, Python screen capture, VLM feedback loop, and git — all without prior training on this specific toolchain

### What Went Wrong (And Was Fixed)

- **6 rendering bugs** in the initial code — all found by code review after a human hint
- **Critical memory overlap** — the most serious bug, found only after the rendering fixes caused a crash
- **VLM limitations** — the visual model couldn't catch intermittent sprite corruption or the startup crash; deterministic tools (compiler errors, binary analysis, monitor dumps) were essential

### How This Compares to AI Benchmarks

Tests like **ARC-AGI** measure abstract pattern recognition. **SWE-bench** measures the ability to fix isolated bugs in existing codebases. **HumanEval** tests function-level code generation.

This experiment tested something different: **sustained multi-domain problem-solving over a long session**. The agent had to:

1. Read and understand 2,000+ lines of existing C64 game code
2. Design a new game concept combining mechanics from three different games
3. Implement 1,581 lines of systems-level C with hardware-specific constraints
4. Debug compilation errors, runtime crashes, rendering bugs, and memory layout issues
5. Use visual feedback (VLM) and deterministic tools (compiler, binary analysis) together
6. Iterate through multiple fix-test-verify cycles without losing context

No single benchmark captures this. It's closer to what software engineers actually do: hold a complex system in your head, reason about interactions between subsystems, and methodically track down bugs that span multiple abstraction layers.

### The Role of Deterministic Tools

The VLM (visual AI) was useful for high-level verification — "does the screen look like a game?" — but it couldn't catch the critical bugs. Those were found by:

- **The compiler** — catching syntax errors and type mismatches
- **Binary analysis** — `python3 -c "open('meteor.prg','rb').read()"` to check memory layout
- **The VICE monitor** — CPU register dumps showing the crash location
- **Code review** — reading 1,581 lines to find logic errors

This suggests that the most effective AI development setup isn't pure LLM reasoning or pure tool automation — it's **an agent that can reason about problems AND use deterministic tools to verify its hypotheses**. The AI formulates a theory ("maybe the memory overlaps"), then uses a precise tool to test it (binary analysis showing the exact overlap).

---

## Technical Summary

| Metric | Value |
|--------|-------|
| Language | C (cc65 cross-compiler for 6502) |
| Target | Commodore 64 (PAL, 985,248 Hz) |
| Lines of code | 1,581 (single file) |
| Binary size | 26,058 bytes |
| Custom characters | 14 (meteors, stars, shields, explosions, power-ups) |
| Hardware sprites | 4 (ship, bullet, power-up, UFO) |
| SID voices used | 3 (shoot, explosion, power-up) |
| Max concurrent objects | 16 meteors + 20 stars + 4 shields + bullet + UFO |
| Bugs found and fixed | 7 (6 rendering + 1 critical memory layout) |
| Human interventions | 1 (hint about sprite corruption) |
| Total session time | ~90 minutes |

---

## Conclusion

A Commodore 64 game is, in many ways, the perfect test of an AI agent's ability to reason about constrained systems. There's no room for abstraction: you're writing bytes to specific memory addresses, counting CPU cycles, managing a memory map where a single misplaced byte can crash the system. The VIC-II chip has exactly 8 sprites. The SID has exactly 3 voices. The screen is exactly 40×25 characters.

Within these constraints, the agent designed a game, built it, found its own bugs, and fixed them — including a memory layout problem that required understanding how C64 flat binaries are loaded, how the cc65 linker places code segments, and how VIC-II bank selection works.

It's not human creativity. But it's not just code generation either. It's something in between: an AI that can hold a complex system in context, reason about hardware constraints, use tools to verify its assumptions, and iterate toward a working solution.

The game works. The meteors split. The shields crumble. The UFO flies across the top of the screen. And somewhere in those 26 kilobytes, there's a demo AI playing forever, racking up points in an 8-bit universe that didn't exist 90 minutes ago.

---

*The C64AIToolChain project is open source. METEOR STORM was created entirely by an AI agent (Claude Opus 4.6) using the cc65 compiler, VICE emulator, and a VLM feedback loop, with a single human hint during the debugging phase.*

*All code: [github.com/...](https://github.com/)*

---

**Tags:** `#AI` `#RetroComputing` `#Commodore64` `#GameDev` `#LLM` `#CreativeAI` `#cc65` `#6502`
