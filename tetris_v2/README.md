# C64 Tetris with AI Toolchain Support

This project is a fully functional Tetris clone for the Commodore 64, written in 6502 assembly using the cc65 toolchain. It is designed to work with a Python-based AI toolchain for automated testing and gameplay.

![Tetris Demo](tetris_v2.gif)

## Features

*   **Classic Gameplay**: Standard Tetris mechanics including rotation, line clearing, and scoring.
*   **Visual Enhancements**:
    *   Color-coded tetrominoes.
    *   **Ghost Piece**: Shows exactly where the current piece will land (displayed as white dots).
    *   **Next Piece Preview**: Displays the upcoming piece on the side.
*   **Controls**: Joystick Port 2 support with optimized input handling (prioritizes lateral movement over dropping to prevent accidental drops).
*   **AI Integration**: Hooks for AI control (currently disabled for manual play) and screen scraping via VICE remote monitor.

## Prerequisites

*   **cc65**: 6502 C Compiler / Assembler (specifically `cl65`).
*   **VICE**: Commodore 64 Emulator (`x64`).
*   **Python 3**: For the AI toolchain/verification script.

## File Structure

*   `tetris.s`: Main assembly source code containing game logic, rendering, and input handling.
*   `tetris.cfg`: Memory configuration for the linker.
*   `build.sh`: Script to compile and link the project.
*   `run_vice.sh`: Script to launch the game in VICE with remote monitor enabled.
*   `../ai_toolchain.py`: Python script to connect to VICE and visualize the game state in the terminal.

## How to Build

Run the build script from the `tetris` directory:

```bash
./build.sh
```

This will generate `tetris.prg`.

## How to Run

### Manual Play

1.  Build the game.
2.  Run the VICE wrapper script:
    ```bash
    ./run_vice.sh
    ```
3.  Use **Joystick Port 2** (or mapped keys) to play.
    *   **Left/Right**: Move piece.
    *   **Up**: Rotate piece.
    *   **Down**: Soft drop.

### AI Toolchain / Verification

To verify the game state via the Python toolchain (which connects to the running emulator):

1.  Ensure VICE is running (step 2 above).
2.  Run the toolchain script from the project root or tetris folder:
    ```bash
    python3 ../ai_toolchain.py --frames 100
    ```
    This will capture 100 frames of gameplay, rendering the C64 screen (with colors!) to your terminal.

## Development History & Key Fixes

*   **Visuals**: Fixed black screen issues by initializing Color RAM. Added correct screen codes for walls and text.
*   **Flickering**: Implemented a `redraw_flag` system to only update the board when necessary, eliminating screen flicker.
*   **Input**: Refined joystick logic to prevent accidental drops when moving diagonally.
*   **Accumulation**: Fixed rendering logic to ensure locked pieces are displayed correctly as solid blocks.
*   **Ghost Piece**: Added a white "ghost" projection to assist player accuracy.
