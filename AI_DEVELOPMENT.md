# Commodore 64 AI Development Toolchain

This project includes a toolchain to help AI agents (like GitHub Copilot) develop and debug C64 software by providing feedback from the running emulator.

## Prerequisites

1.  **VICE Emulator**: You need `x64` or `x64sc` installed.
2.  **Python 3**: To run the interface script.

## Setup

1.  **Build the Project**:
    ```bash
    ./build.sh
    ```

2.  **Run VICE with Remote Monitor**:
    Start VICE with the `-remotemonitor` flag to enable the telnet interface on port 6510.
    ```bash
    x64 -remotemonitor snake.prg &
    ```
    *Note: You might need to configure the monitor port in VICE settings if it's not 6510.*

3.  **Run the AI Interface**:
    This script connects to VICE, reads the screen memory ($0400), and displays it in the terminal. This allows the AI (or you) to see the game state without needing the graphical window.
    ```bash
    python3 ai_toolchain.py
    ```

## How it Works

-   **`ai_toolchain.py`**: Connects to localhost:6510 via TCP.
    -   Sends `m 0400 07e7` to dump the screen RAM.
    -   Parses the hex output and maps C64 screen codes to ASCII characters.
    -   Displays the 40x25 screen in the console.
-   **`vlm_look.py`**: Uses a local Vision Language Model (via Ollama) to analyze screenshots.
    -   Provides high-level semantic understanding ("The snake is hitting the wall").
    -   Useful for debugging graphical issues that ASCII cannot capture.

## Automating Feedback

To fully automate this for an AI agent:
1.  The agent modifies the code.
2.  The agent runs `./build.sh`.
3.  The agent runs `python3 reload_game.py` to inject the new code.
4.  The agent runs `python3 ai_toolchain.py` (for logic) or `python3 vlm_look.py` (for visuals) to verify the state.
5.  The agent analyzes the output to determine if the game is working.

## Toolchain Updates

-   **Resume Execution**: The `ai_toolchain.py` script now sends the `x` command to the VICE monitor after reading the screen. This prevents the emulator from staying paused in monitor mode, allowing the AI to see the game in motion.
-   **Robust Parsing**: The screen parser has been improved to handle various monitor output formats and potential garbage data.
