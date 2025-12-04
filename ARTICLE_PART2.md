# Teaching AI to "See": Building Visual Feedback Loops for Blind Coding Agents

**Part 2 of the C64AIToolChain Series**

*How we gave an AI the ability to develop graphics software without eyes*

---

## The Paradox

Here's a strange situation: I asked an AI to help me build a Snake game for the Commodore 64. The AI wrote assembly code, designed sprite graphics, and debugged collision detection. 

**But the AI couldn't see any of it.**

Modern AI coding agents like GitHub Copilot, Claude, and GPT are extraordinarily capable at generating code. But they share a fundamental limitation: **they are blind**. They can write graphics routines, but they cannot observe the visual output. They can position sprites, but they cannot see if those sprites are actually visible.

This is Part 2 of our C64AIToolChain series. In Part 1, we built the basic infrastructure—a bridge between a modern AI and a 40-year-old computer. Now, we tackle a deeper problem: **How can a blind entity develop visual software?**

---

## The "Blind Programmer" Problem

Let's be precise about what we mean by "blind" in this context.

When you paste an image into a chat with Claude or GPT-4V, the model *can* analyze it. These are "Vision Language Models" (VLMs) with genuine image understanding capabilities. 

But here's the catch:

1. **The AI cannot proactively capture images.** It must wait for you to provide them.
2. **In coding environments, images aren't injected automatically.** The AI sees only text—code, terminal output, file contents.
3. **The feedback loop is broken.** The AI writes code → something visual happens → the AI doesn't know what happened.

This is analogous to a blind programmer working on graphics software. They can write the code, but they need external feedback—screen readers, verbal descriptions, or data representations—to understand the visual result.

**Our task: Build that external feedback system.**

---

## The Three Eyes of C64AIToolChain

We developed three methods for the AI to "see" the C64 screen, each with different trade-offs:

### Eye #1: Memory Reading (The Precise Eye)

The Commodore 64's screen is ultimately just memory. Screen RAM lives at `$0400-$07E7` (1000 bytes for a 40×25 character display). Color RAM is at `$D800-$DBE7`. Sprite positions are in VIC-II registers at `$D000-$D00F`.

```python
# Read screen RAM via VICE monitor
response = send_command(socket, "m 0400 07e7")
# Parse hex dump into character grid
screen_data = parse_hex_dump(response)
```

**Advantages:**
- Precise character-level data
- Fast (milliseconds)
- Low computational overhead
- Works for any text-mode game

**Disadvantages:**
- Doesn't capture sprites (they're overlays)
- No visual layout—just raw data
- Requires understanding C64 memory map

### Eye #2: ASCII Art Conversion (The Practical Eye)

Take a screenshot from VICE, convert it to ASCII art:

```python
def image_to_ascii(image_path, width=80, height=25):
    img = Image.open(image_path).convert('L')  # Grayscale
    img = img.resize((width, height))
    
    chars = ' .:-=+*#%@'
    pixels = list(img.getdata())
    
    result = ''
    for i, pixel in enumerate(pixels):
        char_idx = int(pixel / 256 * len(chars))
        result += chars[min(char_idx, len(chars)-1)]
        if (i + 1) % width == 0:
            result += '\n'
    return result
```

The result:

```
********************************************************************************
***************+++****+*******+*******++***++********+++++++++++++++++++********
******+=-:---::   .... ::::::- .:.:-::  :.-. :.::--.:                  .=*******
*******=.   :                                                       .. .=*******
*******=.   .     .:                                      .:           .=*******
********:          .--=-                                               :********
********:          +@%*=                                               :********
********:         .=*+.                                                :********
********-..............................................................-********
********************************************************************************
```

The AI can now "see" that:
- There's a bordered play area
- Bright elements (`@%*`) indicate the snake sprites
- The layout matches expectations

**Advantages:**
- Captures everything visible (including sprites)
- Text-native for AI processing
- Conveys layout and structure
- Low bandwidth (kilobytes vs megabytes)

**Disadvantages:**
- Loses color information
- Low resolution
- Interpretation requires pattern matching

### Eye #3: Human Injection (The Clear Eye)

Sometimes, nothing beats a real image. The human supervisor takes a screenshot and pastes it into the chat. The AI's VLM capabilities kick in, providing full visual analysis.

**Advantages:**
- Full fidelity
- Color information preserved
- AI can use genuine visual reasoning

**Disadvantages:**
- Requires human in the loop
- Breaks autonomous development flow
- Not scalable for rapid iteration

---

## The Energy Equation

Here's an insight that emerged during development: **these methods have dramatically different computational costs**.

| Method | Approximate Cost | Accuracy | Speed |
|--------|-----------------|----------|-------|
| Memory Reading | ~0.001 kWh | Data only | <100ms |
| ASCII Conversion | ~0.01 kWh | Layout + shapes | ~500ms |
| VLM Image Analysis | ~0.1 kWh | Full visual | ~2s |

For iterative game development—where you might make 50+ build-test cycles in a session—the difference matters. Using full VLM analysis for every iteration would consume 100× more energy than ASCII conversion.

**The optimal strategy is hierarchical:**

1. Use **memory reading** for quick state checks (Is the sprite enabled? What's the score?)
2. Use **ASCII conversion** for layout verification (Is the snake in the right place? Are walls visible?)
3. Use **VLM analysis** sparingly for final visual polish (Are the colors right? Does it look good?)

This mirrors how a blind programmer might work: use automated tools for routine checks, save the "ask a sighted person" step for when it really matters.

---

## The Feedback Loop in Practice

Here's how a development session actually works:

```
┌─────────────────┐
│  AI modifies    │
│   snake.s       │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Build with     │
│   cc65/ld65     │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Load into      │
│    VICE         │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Capture via    │
│  ASCII/Memory   │◄───────┐
└────────┬────────┘        │
         │                 │
         ▼                 │
┌─────────────────┐        │
│  AI analyzes    │        │
│    output       │        │
└────────┬────────┘        │
         │                 │
         ▼                 │
    ┌────┴────┐            │
    │ Working?│────No──────┘
    └────┬────┘
         │ Yes
         ▼
      Done!
```

Each cycle takes 5-10 seconds. The AI can iterate rapidly, testing hypotheses about bugs, verifying fixes, and building features incrementally.

---

## A Real Debugging Session

Let me show you a real example. We had a bug where all sprites displayed the same shape. Here's what the AI's "vision" showed:

**Memory dump of sprite pointers ($07F8-$07FF):**
```
>C:07F8  2e 2e 2e 2e 2e 2e 2e 2e
```

All pointers were `$2E` (46 decimal)—which is the screen code for the grass character! The screen-clear routine was overwriting the sprite pointers.

The AI couldn't "see" the visual bug directly. But by reading memory, it could diagnose the cause: sprite pointers were in screen RAM range, and the clear loop was stomping on them.

**The fix:**

```asm
; Initialize game - ORDER MATTERS!
    jsr init_screen          ; Clear screen first (overwrites $07F8-$07FF)
    jsr set_sprite_pointers  ; THEN set sprite pointers (fixes them)
    jsr init_sprites         ; Then copy sprite data
```

This is genuinely impressive. The AI diagnosed a *visual* bug using *textual* memory analysis. It never saw the sprites looking wrong—it deduced the problem from data.

---

## The Philosophical Implications

This project raises interesting questions about AI and perception:

**Can a blind entity develop visual software?**

Yes, but with scaffolding. The entity needs tools that translate visual output into its native modality. For text-based AI, this means ASCII art, memory dumps, and structured data.

**Is this "real" vision?**

No—and yes. The AI isn't processing raw pixels like a human visual cortex. But it's extracting meaningful information about the visual state. It's more like reading a detailed description than "seeing," but the end result—understanding what's on screen—is achieved.

**What are the limits?**

Aesthetic judgments are hard. The AI can verify that a sprite is visible at position (100, 150), but it struggles to assess whether the sprite *looks good*. Color harmony, visual balance, and "game feel" remain challenging without true vision.

---

## Tools for Your Own Experiments

The C64AIToolChain is open source. Here are the key components:

**`look_screen.py`** - Screenshot to ASCII converter
```bash
python3 look_screen.py --truecolor  # Color ASCII art
python3 look_screen.py -w 120 -h 40 # Higher resolution
```

**`ai_toolchain.py`** - Memory reader and dev loop
```bash
python3 ai_toolchain.py --screenshot  # Screenshot mode
python3 ai_toolchain.py --loop snake2/ -n 3  # Full dev cycle
```

**`AGENT_HOWTO.md`** - Instructions for AI agents

This last file is interesting: it's documentation *designed to be read by AI agents*, teaching them how to use the toolchain to develop C64 games autonomously.

---

## Conclusion

We set out to build a Snake game for the Commodore 64 with AI assistance. Along the way, we discovered something more fundamental: **how to create sensory feedback for entities that lack senses**.

The techniques here—memory reading, format conversion, hierarchical observation strategies—apply beyond retro computing. Any situation where an AI needs to understand visual output but only receives text could benefit from similar approaches.

The AI never truly "saw" our Snake game. But it diagnosed sprite pointer bugs, verified screen layouts, and iteratively improved the code until the game worked. For practical purposes, that's vision enough.

---

## What's Next?

In Part 3, we'll tackle an even more ambitious goal: **autonomous game development**. Can we set up a system where the AI proposes features, implements them, tests them, and iterates—all without human intervention?

The toolchain is ready. The eyes are open (metaphorically). Now we see how far autonomous development can go.

---

*Clone the repository and start experimenting:*  
**github.com/dexmac221/C64AIToolChain**

*Questions or ideas? Open an issue or reach out on Twitter.*

---

**About the Author**

This article documents an experimental collaboration between a human developer and AI coding assistants (GitHub Copilot/Claude). The C64AIToolChain project explores the frontier of AI-assisted retro game development.

---

*Tags: AI, Machine Learning, Retro Computing, Commodore 64, Game Development, Vision Language Models, Developer Tools*
