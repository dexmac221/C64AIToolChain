# AI Agent How-To: C64 Game Development

## For AI Agents: Read This First

This document is designed to be read by AI coding agents (GitHub Copilot, Claude, GPT, Gemini, etc.) to enable autonomous or semi-autonomous C64 game development.

---

## The Core Problem: You Are Blind

As an AI agent, you have **no direct visual perception**. You cannot:
- See the emulator window
- Observe sprite movements
- Verify graphical output directly

**Solution**: This toolchain provides "eyes" through text-based screen capture.

---

## Available Tools

### 1. `look_screen.py` - Visual Feedback via Screenshot

Takes a screenshot from VICE and converts to ASCII art.

```bash
# Basic usage - capture and display
python3 look_screen.py

# With color output (if terminal supports)
python3 look_screen.py --truecolor

# Save screenshot and display
python3 look_screen.py -o game_state.png

# Higher resolution
python3 look_screen.py -w 120 -ht 40
```

**Output interpretation:**
- `*` `#` `%` `@` = Bright elements (sprites, walls)
- `.` `:` `-` = Medium elements (bushes, background)
- ` ` (space) = Dark/empty areas
- Border pattern = Walls

### 2. `ai_toolchain.py` - Memory-Level Feedback

Reads C64 memory directly for precise game state.

```bash
# Single snapshot of screen RAM
python3 ai_toolchain.py --once

# Screenshot-based view (shows sprites)
python3 ai_toolchain.py --screenshot

# Full development cycle
python3 ai_toolchain.py --loop snake2/ -n 1
```

**Memory locations you can query:**
| Address | Content |
|---------|---------|
| `$0400-$07E7` | Screen RAM (1000 chars) |
| `$D800-$DBE7` | Color RAM |
| `$D000-$D01F` | Sprite registers |
| `$D015` | Sprite enable bits |
| `$07F8-$07FF` | Sprite pointers |

### 3. Build System

```bash
# Build project
cd snake2 && bash ./build.sh

# Expected output on success:
# (no output = success)

# Check for .prg file
ls -la *.prg
```

### 4. VICE Emulator Control

```bash
# Start VICE with remote monitor
(unset LD_LIBRARY_PATH GTK_PATH; x64 -remotemonitor game.prg) &

# Or use the project script
bash ./run_vice.sh
```

**Monitor commands (via port 6510):**
```
reset 0          # Reset C64
l "file.prg" 0   # Load program
g 0810           # Go to address (start program)
m 0400 07e7      # Memory dump (screen)
x                # Exit monitor, resume execution
screenshot "file.png" 2   # Take PNG screenshot
```

---

## Development Workflow

### Step 1: Make Code Changes

Edit `snake.s` (or target assembly file):
```asm
; Example: Change snake speed
GAME_SPEED = 5      ; Frames between updates (lower = faster)
```

### Step 2: Build

```bash
cd snake2 && bash ./build.sh 2>&1
```

**Check for errors:**
- No output = success
- Error messages indicate line numbers and issues

### Step 3: Run/Reload

If VICE is running:
```bash
python3 ai_toolchain.py --loop snake2/ -n 1
```

If VICE is not running:
```bash
cd snake2 && bash ./run_vice.sh
```

### Step 4: Observe Result

```bash
python3 look_screen.py
```

**Analyze the ASCII output:**
1. Is the border visible? (walls working)
2. Are there bright spots moving? (sprites working)
3. Is text displayed correctly? (screen RAM working)
4. Does the layout match expectations?

### Step 5: Iterate

Based on observation, modify code and repeat.

---

## Common Issues and Solutions

### Sprites Not Visible
**Symptom**: No bright moving objects in ASCII output
**Check**: 
- Sprite enable register `$D015`
- Sprite pointers at `$07F8-$07FF`
- Sprite data copied to correct VIC bank

### Screen Garbage
**Symptom**: Random characters everywhere
**Check**:
- Screen clear routine not overwriting sprite pointers
- Proper initialization order (init_screen BEFORE set_sprite_pointers can cause issues)

### Game Freezes
**Symptom**: Same screen state on multiple captures
**Check**:
- Main loop structure
- Interrupt handling (`sei`/`cli`)
- Wait routines using `$D012`

### Build Errors
**Symptom**: ca65/ld65 error messages
**Check**:
- Syntax errors (missing colons, wrong mnemonics)
- Undefined labels
- Segment overflow

---

## Project Structure

```
C64AIToolChain/
├── ai_toolchain.py      # Memory-based screen reading
├── look_screen.py       # Screenshot-based viewing
├── screenshot.sh        # VICE screenshot helper
├── snake2/
│   ├── snake.s          # Main assembly source
│   ├── snake.cfg        # Linker configuration
│   ├── build.sh         # Build script
│   ├── run_vice.sh      # VICE launcher
│   └── snake.prg        # Compiled program
```

---

## Key Memory Map (C64)

```
$0000-$00FF   Zero Page (fast variables)
$0100-$01FF   Stack
$0400-$07FF   Screen RAM (default)
$0800-$9FFF   BASIC/Program area
$2000-$3FFF   Common sprite data location
$D000-$D3FF   VIC-II registers
$D800-$DBFF   Color RAM
$DC00-$DCFF   CIA1 (keyboard, joystick)
$DD00-$DDFF   CIA2 (serial, bank select)
```

---

## Sprite System Quick Reference

```
Sprite 0: $D000 (X), $D001 (Y), pointer at $07F8
Sprite 1: $D002 (X), $D003 (Y), pointer at $07F9
...
Sprite 7: $D00E (X), $D00F (Y), pointer at $07FF

$D010 = X position MSB (bit 8 for each sprite)
$D015 = Sprite enable (bit per sprite)
$D01C = Multicolor enable
$D01D = X expand
$D017 = Y expand
$D027-$D02E = Sprite colors
```

---

## Energy-Efficient Observation Strategy

For iterative development, use this hierarchy:

1. **First**: Memory reading (lowest cost)
   - Check game variables
   - Verify screen RAM content
   
2. **Second**: ASCII screenshot (medium cost)
   - Verify layout
   - Check sprite positions
   
3. **Third**: Full image (highest cost, requires human)
   - Final visual polish
   - Color verification

---

## Example Session

```bash
# 1. Agent modifies snake.s to add a new feature

# 2. Build
cd snake2 && bash ./build.sh 2>&1
# (no output = success)

# 3. Ensure VICE is running
pgrep x64 || bash ./run_vice.sh

# 4. Wait for startup
sleep 3

# 5. Capture screen state
cd .. && python3 look_screen.py

# Output:
# ************************
# *     SCORE: 00       *
# *......................* 
# *  @##  .    =        *  <- Snake (@##) and apple (=)
# *......................* 
# ************************

# 6. Analyze: Snake visible, apple visible, walls OK
# 7. If issues found, modify code and repeat from step 2
```

---

## For Human Supervisors

When the AI agent gets stuck:
1. Take a screenshot of VICE manually
2. Paste the image into the chat
3. The agent can then use full visual analysis
4. This breaks the text-only limitation temporarily

---

*This document is part of the C64AIToolChain project*
*Enabling AI-assisted retro game development*
