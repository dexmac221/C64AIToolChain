# C64 AI Toolchain - Agent Rules and Guidelines

This document defines the **mandatory rules** and best practices for AI agents working with the Commodore 64 AI Development Toolchain.

## 🎯 Core Principle

> **The VLM (Visual Language Model) is your eyes. ALWAYS wait for its analysis before making decisions about the visual state of the program.**

This is not optional. This is the foundation of the entire toolchain.

---

## 🚨 MANDATORY RULES - READ CAREFULLY

### Rule 0: The VLM is Non-Negotiable

- **ALWAYS** wait for VLM analysis to complete
- **NEVER** skip VLM analysis to save time
- **NEVER** tell the user "the VLM takes too long" or "I'll skip this step"
- **NEVER** assume what's on screen without VLM confirmation
- **ALWAYS** use VLM feedback to guide your decisions

The VLM may take 30-90 seconds. This is expected. Wait for it.

---

## 📋 Mandatory Rules

### Rule 1: VLM Feedback is Sacred

- **NEVER skip VLM analysis** - It provides the only true visual feedback
- **ALWAYS wait for VLM response** - Even if it takes 30-60 seconds
- **NEVER assume** what's on screen without VLM confirmation
- **NEVER tell the user** "the VLM takes too long" - just wait for it

```bash
# Correct workflow:
1. Make code change
2. Build (./build.sh)
3. Reload in VICE (reload_game.py)
4. Take screenshot
5. Run VLM analysis
6. WAIT for VLM response
7. Analyze VLM output
8. Decide next action
```

### Rule 2: The Build-Test-Analyze Loop

Every code modification MUST follow this cycle:

```
┌─────────────────────────────────────────────────────────────┐
│                    DEVELOPMENT LOOP                         │
├─────────────────────────────────────────────────────────────┤
│  1. EDIT    → Modify source code (.c, .s, .h files)        │
│  2. BUILD   → Run build.sh or cl65/ca65                    │
│  3. RELOAD  → python3 reload_game.py <program.prg>         │
│  4. CAPTURE → Take screenshot via VICE monitor             │
│  5. ANALYZE → Run vlm_look.py --existing <screenshot>      │
│  6. WAIT    → Wait for complete VLM response               │
│  7. DECIDE  → Based on VLM output, plan next change        │
│  8. REPEAT  → Go back to step 1                            │
└─────────────────────────────────────────────────────────────┘
```

### Rule 3: Error Handling Priority

When encountering errors, follow this priority:

1. **Build errors** - Fix syntax/compilation errors first
2. **Link errors** - Check memory layout, symbol references
3. **Runtime crashes** - Use VICE monitor to debug
4. **Visual bugs** - Use VLM to identify and describe issues

### Rule 4: C64 Hardware Constraints

Always remember these C64 limitations:

| Resource | Limit | Notes |
|----------|-------|-------|
| RAM | 64KB total | ~38KB usable for programs |
| Screen RAM | $0400-$07FF | 1000 bytes (40x25) |
| Color RAM | $D800-$DBE7 | 1000 bytes |
| Sprites | 8 max | 24x21 pixels each |
| Sprite memory | 64 bytes each | Must be on 64-byte boundary |
| Colors | 16 total | Fixed palette |
| CPU | 1 MHz 6510 | Very limited cycles |

### Rule 5: Safe Memory Regions for Sprites

When using cc65 C programs, use these safe memory regions:

| Address | Block # | Use |
|---------|---------|-----|
| $3000 | 192 | Safe for sprite data |
| $3040 | 193 | Safe for sprite data |
| $3080 | 194 | Safe for sprite data |
| $30C0 | 195 | Safe for sprite data |

**Avoid**: $0800-$2FFF (used by BASIC/C runtime)

---

## 🛠️ Toolchain Components

### 1. vlm_look.py - Visual Analyzer

**Purpose**: Analyze VICE screenshots using a VLM (Vision Language Model)

**Usage**:
```bash
# Analyze current screen (takes new screenshot)
python3 vlm_look.py

# Analyze existing screenshot
python3 vlm_look.py --existing /path/to/screenshot.png

# Get JSON structured output
python3 vlm_look.py --json --existing screenshot.png

# Custom analysis prompt
python3 vlm_look.py --prompt "Describe the sprite positions" --existing img.png

# Compare two screenshots (before/after)
python3 vlm_look.py --compare before.png --with after.png

# Multi-frame motion analysis
python3 vlm_look.py --motion 3 --motion-interval 0.5
```

**Expected Wait Time**: 20-60 seconds depending on model and hardware

### 2. reload_game.py - Hot Reload

**Purpose**: Reload PRG files into running VICE instance without restart

**Usage**:
```bash
python3 reload_game.py path/to/program.prg
```

**Requirements**: VICE must be running with `-remotemonitor` flag

### 3. screenshot.sh - Screenshot Capture

**Purpose**: Take screenshot from VICE via remote monitor

**Usage**:
```bash
./screenshot.sh [filename] [format]
# Format: 0=BMP, 1=PCX, 2=PNG (default), 3=GIF, 4=IFF
```

### 4. ai_toolchain.py - Legacy Analyzer

**Purpose**: ASCII-art based screen analysis (deprecated in favor of vlm_look.py)

---

## 📁 Project Structure

Standard project layout for C64 programs:

```
project_name/
├── build.sh          # Build script
├── run_vice.sh       # VICE launcher
├── program.s         # Assembly source (if using asm)
├── program.c         # C source (if using cc65)
├── program.cfg       # Linker configuration (optional)
├── program.prg       # Compiled output
└── README.md         # Project documentation
```

### build.sh Template (C)

```bash
#!/bin/bash
cd "$(dirname "$0")"
cl65 -t c64 -O -o program.prg program.c
if [[ -f program.prg ]]; then
    echo "Built program.prg ($(stat -c%s program.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
```

### build.sh Template (Assembly)

```bash
#!/bin/bash
cd "$(dirname "$0")"
ca65 program.s -o program.o
ld65 -C program.cfg -o program.prg program.o
if [[ -f program.prg ]]; then
    echo "Built program.prg ($(stat -c%s program.prg) bytes)"
else
    echo "Build failed!"
    exit 1
fi
```

### run_vice.sh Template

```bash
#!/bin/bash
cd "$(dirname "$0")"
x64 -remotemonitor program.prg
```

---

## 🔧 VICE Remote Monitor Commands

Connect to VICE monitor on port 6510:

| Command | Description |
|---------|-------------|
| `load "file.prg" 0` | Load PRG file |
| `g $0801` | Start execution at address |
| `reset 0` | Soft reset |
| `screenshot "file.png" 2` | Save PNG screenshot |
| `x` | Exit/continue execution |
| `m $0400 $0500` | Memory dump |
| `d $0800` | Disassemble at address |

---

## 🐛 Common Issues and Solutions

### Issue: Sprites not visible

**Possible causes**:
1. Sprite not enabled: Check `$D015` (VIC_SPR_ENA)
2. Wrong sprite pointer: Check `$07F8-$07FF`
3. Sprite data at wrong address: Must be on 64-byte boundary
4. Sprite position off-screen: X must be 24-343, Y must be 50-249
5. Memory conflict: Sprite data overwritten by program

**Solution**: Use safe memory at $3000+ for sprite data

### Issue: C program crashes

**Possible causes**:
1. Stack overflow (too many nested calls)
2. Array out of bounds
3. Null pointer dereference
4. Memory conflict with sprite/screen RAM

**Solution**: Keep code simple, use static variables

### Issue: Assembly branch out of range

**Cause**: Relative branches limited to -128 to +127 bytes

**Solution**: Use `jmp` with conditional:
```asm
    ; Instead of: bcc far_label
    bcs skip
    jmp far_label
skip:
```

### Issue: Maze/screen looks wrong

**Possible causes**:
1. Screen RAM pointer wrong
2. Color RAM not set
3. Character encoding mismatch

**Solution**: Screen RAM is at $0400, Color RAM at $D800

---

## 📊 VLM Analysis Best Practices

### What to ask the VLM

Good prompts:
- "Describe all visible sprites and their positions"
- "Is there any text on screen? What does it say?"
- "Are there any visual glitches or rendering issues?"
- "Compare this to the expected layout: [description]"

### Interpreting VLM output

The VLM will describe:
- **Sprite positions**: "top-left", "center", "bottom-right"
- **Colors**: Based on C64's 16-color palette
- **Text**: OCR of any displayed text
- **Game state**: Title, playing, game over, etc.
- **Bugs**: Missing elements, flickering, corruption

### When VLM says something is wrong

1. **Trust the VLM** - It has actual vision
2. **Re-check your code** - The bug is likely in your logic
3. **Take another screenshot** - Ensure timing was right
4. **Compare screenshots** - Use `--compare` to see changes

---

## 🚀 Quick Reference Commands

```bash
# Build project
cd project_name && ./build.sh

# Start VICE with remote monitor
x64 -remotemonitor program.prg

# Reload without restarting VICE
python3 reload_game.py project_name/program.prg

# Take screenshot
./screenshot.sh screenshot_name

# Analyze with VLM (ALWAYS WAIT for response!)
python3 vlm_look.py --existing screenshot_name.png

# Get JSON structured output
python3 vlm_look.py --json --existing screenshot_name.png

# Use custom prompt for specific question
python3 vlm_look.py --prompt "Is the sprite visible in the center?" --existing img.png

# Full development cycle (example)
./build.sh && \
    python3 ../reload_game.py program.prg && \
    sleep 2 && \
    python3 ../vlm_look.py
```

### Complete Development Cycle Example

```bash
# Step 1: Edit source code
vim program.c

# Step 2: Build
./build.sh
# Output: Built program.prg (4588 bytes)

# Step 3: Reload in VICE (if running) or start VICE
python3 ../reload_game.py program.prg

# Step 4: Wait for program to start (important!)
sleep 2

# Step 5: Analyze screen with VLM - THIS IS MANDATORY
python3 ../vlm_look.py

# Step 6: WAIT for VLM output (30-90 seconds)
# ... VLM analyzes and describes screen ...

# Step 7: Based on VLM output, decide next action
#   - If bugs found: fix code, go to Step 1
#   - If working: proceed to next feature
#   - If unclear: take another screenshot with different prompt
```

---

## ⚠️ Anti-Patterns (What NOT to Do)

1. ❌ **Don't skip VLM analysis** - This is the cardinal sin
2. ❌ **Don't say "VLM is slow"** - Just wait for it silently
3. ❌ **Don't assume** screen state without visual confirmation
4. ❌ **Don't ignore** VLM-reported issues
5. ❌ **Don't use memory below $3000** for sprite data in C programs
6. ❌ **Don't forget** to wait for VICE to fully load before screenshot
7. ❌ **Don't make multiple changes** before testing each one
8. ❌ **Don't rush** - The visual feedback loop is what makes this work

### What To Do When VLM Is Running

While waiting for VLM analysis:
- You may explain what you're waiting for to the user
- You may review the code for other potential issues
- You MUST NOT proceed with assumptions
- You MUST NOT tell the user to skip this step
- You MUST wait for the VLM output before deciding next actions

---

## 📝 Version History

- **v1.0** (December 2024): Initial toolchain with ASCII analysis
- **v2.0** (December 2024): Added VLM integration for true vision
- **v2.1** (December 2024): Added C language support, generalized prompts
- **v2.2** (December 2024): Generalized for all C64 software (not just games)

---

## 🎮 Scope: All C64 Software Development

This toolchain is **NOT limited to games**. It supports development of:

- **Games**: Arcade, puzzle, action, adventure, etc.
- **Applications**: Text editors, databases, utilities
- **Demos**: Visual effects, music players, scrollers
- **Tools**: Disk utilities, file managers, monitors
- **Educational software**: Learning programs, simulations
- **Graphics programs**: Paint programs, sprite editors
- **Music software**: Trackers, SID players

The VLM analyzes any visual output - use it for all C64 development.

---

## 🤝 Contributing

When extending the toolchain:

1. Keep tools simple and focused
2. Use Python 3 for tooling
3. Support both assembly and C workflows
4. Always test with VLM feedback loop
5. Document all new features

---

*This document is the authoritative reference for AI agents using the C64 AI Toolchain.*
