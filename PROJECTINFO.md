# Project Information & Troubleshooting

This document contains important information about the development environment, common issues, and best practices for this repository.

## Project Structure

The workspace contains multiple subprojects demonstrating different aspects of C64 development:

| Project | Type | Description |
|---------|------|-------------|
| `snake/` | Assembly | Classic snake game with AI autopilot |
| `snake2/` | Assembly | Enhanced snake with optimizations |
| `tetris_v1/` | Assembly | Tetris (Claude Sonnet implementation) |
| `tetris_v2/` | Assembly | Tetris (Gemini implementation) |
| `pacman/` | Assembly | Pac-Man (experimental) |
| `pacman_c/` | C | Pac-Man (recommended, fully functional) |
| `matrix/` | C | Matrix-style falling character effect |
| `christmas/` | C | Image display with SID music |

## Running Projects

### Quick Start
Each subproject has its own `run_vice.sh` script:

```bash
cd <project>/
make clean && make
./run_vice.sh
```

### Generic Runner
Use the root-level generic runner for any project:

```bash
./run_vice_generic.sh <project>/program.prg
```

## VICE Emulator in VS Code

When running the VICE emulator (`x64sc`) directly from the VS Code integrated terminal, you may encounter environment variable conflicts, particularly with Snap installations of VICE on Linux.

### The Problem
VS Code injects certain environment variables (like `GTK_PATH` and `GDK_PIXBUF_MODULE_FILE`) that can conflict with the libraries VICE expects, leading to errors such as:
- `symbol lookup error: ... undefined symbol: __libc_pthread_init`
- GTK-related crashes or warnings.

### The Solution
In all run scripts (`run_vice.sh`), we must unset these conflicting variables before launching the emulator:

```bash
unset GTK_PATH
unset GDK_PIXBUF_MODULE_FILE
x64sc -autostart program.prg
```

## Script Pathing

To ensure build and run scripts work correctly regardless of where they are called from (e.g., root of the repo vs. inside the project folder), scripts should always set their working directory to the script's location.

### Best Practice
Start shell scripts with:

```bash
#!/bin/bash
cd "$(dirname "$0")"
```

This ensures that relative paths to source files, libraries, and output files work consistently.

## Build System

Most subprojects use a `Makefile` or a `build.sh` script.
- **Compiler**: `cc65` / `cl65` (C) or `ca65` (Assembly)
- **Linker**: `ld65`
- **Target**: `c64` (Commodore 64)

### Standard Build Commands

**C Projects:**
```bash
cl65 -O -t c64 -o program.prg main.c
```

**Assembly Projects:**
```bash
ca65 -t c64 main.s -o main.o
ld65 -C c64.cfg -o program.prg main.o c64.lib
```

### Root Build Script
The root `build.sh` can build any subproject:

```bash
./build.sh              # builds snake/ (default)
./build.sh matrix       # builds matrix/
./build.sh pacman_c     # builds pacman_c/
```

## Dependencies

### Python Tools
Install Python dependencies for the AI toolchain:

```bash
pip install -r requirements.txt
```

Required packages: `Pillow`, `ollama`

## AI Toolchain

The project includes tools for AI-assisted development:

| Tool | Purpose |
|------|---------|
| `ai_toolchain.py` | Read C64 screen RAM and display as ASCII |
| `vlm_look.py` | Visual analysis using local VLM (Ollama) |
| `reload_game.py` | Hot-reload programs into running VICE |

### Typical Development Loop
```bash
# 1. Build
make

# 2. Reload into running VICE
python3 ../reload_game.py .

# 3. Analyze screen
python3 ../ai_toolchain.py --once
```

## Image Processing Tools

Several projects include image conversion for C64 graphics:

- `image.py` - Convert images to C64 character maps
- `fetch_and_process.py` - Download, resize, and convert external images

**Usage:**
```bash
cd christmas
python3 fetch_and_process.py "https://example.com/image.png"
make && ./run_vice.sh
```

## Custom Character Sets

Projects can use custom character sets created with VChar64:

1. Design characters in VChar64 (`.vchar64proj` files)
2. Export to header: `python3 vchar2charsmap.py input.vchar64proj output.h`
3. Include in C code and load at runtime

## C64 Hardware Reference

| Resource | Address | Size | Notes |
|----------|---------|------|-------|
| Screen RAM | $0400-$07FF | 1000 bytes | 40×25 characters |
| Color RAM | $D800-$DBE7 | 1000 bytes | Per-character colors |
| Character ROM | $D000-$DFFF | 4KB | Can be copied to RAM |
| VIC-II | $D000-$D3FF | - | Graphics chip registers |
| SID | $D400-$D7FF | - | Sound chip registers |
| CIA1 | $DC00-$DCFF | - | Keyboard, joystick |
| CIA2 | $DD00-$DDFF | - | Serial, timers |

## Troubleshooting

### Build Errors
- Ensure `cc65` is installed: `sudo apt install cc65`
- Check that you're in the correct project directory
- Run `make clean` before `make` to clear stale objects

### VICE Won't Start
- Kill existing instances: `pkill x64sc`
- Unset GTK variables (see above)
- Check that the `.prg` file exists

### Remote Monitor Connection Failed
- Start VICE with `-remotemonitor` flag
- Default port is 6510
- Check firewall settings
