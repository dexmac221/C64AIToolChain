# Pac-Man for Commodore 64

A Pac-Man clone written in C using the cc65 compiler, demonstrating AI-assisted game development for the C64.

## Features

- Classic Pac-Man gameplay with maze navigation
- 5 hardware sprites (Pac-Man + 4 ghosts)
- Dot eating with scoring (+10 per dot)
- Power pellets that make ghosts vulnerable (+50 points)
- SID sound effects for eating and power-ups
- Demo mode with AI-controlled Pac-Man
- Title screen with "Press Fire to Start"
- Win/lose conditions with game restart

## Building

Requires cc65 cross-compiler:

```bash
./build.sh
```

This produces `pacman.prg` (~6KB).

## Running

With VICE emulator:

```bash
./run_vice.sh
```

Or use the hot-reload system:

```bash
cd ..
python3 reload_game.py pacman_c/pacman.prg
```

## Controls

- **Joystick Port 2**: Move Pac-Man
- **Fire Button**: Start game from title screen

## Technical Details

- Written in C for easier AI comprehension and modification
- Uses VIC-II sprites at $3000 (block 192) - safe for cc65
- 8x8 pixel sprites positioned in top-left of 24x21 sprite area
- 17-row maze fits within 25-line screen
- Grid-aligned movement (8-pixel cells)

## AI Development

This game was created through AI-assisted development, demonstrating:
- C is more suitable than assembly for AI-generated code
- VLM (Vision Language Model) feedback for visual debugging
- Iterative development with hot-reload testing

Part of the C64 AI ToolChain project.
