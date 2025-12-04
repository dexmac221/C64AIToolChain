#!/usr/bin/env python3
"""
ai_toolchain.py - AI-Assisted C64 Development Feedback Loop

This script enables AI models to iteratively develop C64 games by:
1. Building the code (cc65 assembler)
2. Running in VICE emulator
3. Capturing screen state (via RAM or screenshots)
4. Analyzing the visual output
5. Providing feedback for code changes
6. Repeating until convergence to target

Usage:
    python3 ai_toolchain.py                      # Interactive screen view
    python3 ai_toolchain.py --once               # Single capture
    python3 ai_toolchain.py --frames N           # Capture N frames
    python3 ai_toolchain.py --screenshot         # Use screenshot method
    python3 ai_toolchain.py --loop <dir> [N]     # Full dev cycle

For AI Integration:
    The script provides visual feedback through ASCII/color representation
    enabling text-based AI models to "see" and iterate on game development.

Author: AI Toolchain Project  
Date: December 2024
"""

import socket
import time
import sys
import os
import subprocess
import argparse
from pathlib import Path

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

# C64 Screen Codes to ASCII (Simplified)
SCREEN_CODE_MAP = {
    32: ' ',  # Space
    81: 'O',  # Snake (Circle)
    160: '#',  # Wall (Block)
    83: 'A',  # Apple (Heart)
    65: 'T',  # Tree (Spade)
    46: '.',  # Ghost Piece
    102: '*', # Bush/checkerboard
}

# Add numbers 0-9
for i in range(48, 58):
    SCREEN_CODE_MAP[i] = chr(i)
# Add letters A-Z (Screen Codes 1-26)
for i in range(1, 27):
    SCREEN_CODE_MAP[i] = chr(64 + i)  # 1='A', 2='B'...

# ANSI Color Map for C64 Colors
ANSI_COLORS = {
    0: '\033[30m', 1: '\033[37m', 2: '\033[31m', 3: '\033[36m', 
    4: '\033[35m', 5: '\033[32m', 6: '\033[34m', 7: '\033[33m',
    8: '\033[38;5;208m', 9: '\033[33m', 10: '\033[91m', 11: '\033[90m', 
    12: '\033[37m', 13: '\033[92m', 14: '\033[94m', 15: '\033[97m'
}
RESET = '\033[0m'


# =============================================================================
# VICE Communication
# =============================================================================

def connect_vice(host='localhost', port=6510):
    """Connect to VICE remote monitor."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((host, port))
        try:
            s.recv(1024)
        except socket.timeout:
            pass
        return s
    except ConnectionRefusedError:
        return None


def check_vice_running(host='localhost', port=6510):
    """Check if VICE is running with remote monitor."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(1.0)
        s.connect((host, port))
        s.close()
        return True
    except:
        return False


def send_command(s, cmd):
    """Send command to VICE monitor and get response."""
    try:
        s.send(f"{cmd}\n".encode())
        time.sleep(0.05)
        data = b""
        while True:
            chunk = s.recv(4096)
            data += chunk
            if b"(C:$" in chunk:
                break
            if len(chunk) == 0:
                break
        return data.decode()
    except BrokenPipeError:
        return ""


# =============================================================================
# Screen RAM Reading (Direct Memory Access)
# =============================================================================

def get_screen(s):
    """Read screen RAM $0400-$07E7 (1000 chars)."""
    response = send_command(s, "m 0400 07e7")
    
    screen_data = [32] * 1000
    lines = response.splitlines()
    for line in lines:
        if line.startswith(">C:"):
            try:
                addr_str = line[3:7]
                addr = int(addr_str, 16)
                offset = addr - 0x0400
                
                if 0 <= offset < 1000:
                    parts = line[8:].split()
                    current_offset = offset
                    for p in parts:
                        if len(p) == 2:
                            try:
                                val = int(p, 16)
                                if 0 <= current_offset < 1000:
                                    screen_data[current_offset] = val
                                    current_offset += 1
                            except ValueError:
                                pass
                        elif len(p) > 2:
                            break
            except ValueError:
                pass
    
    return screen_data


def get_color_ram(s):
    """Read color RAM $D800-$DBE7 (1000 chars)."""
    response = send_command(s, "m d800 dbe7")
    
    color_data = [0] * 1000
    lines = response.splitlines()
    for line in lines:
        if line.startswith(">C:"):
            try:
                addr_str = line[3:7]
                addr = int(addr_str, 16)
                offset = addr - 0xD800
                
                if 0 <= offset < 1000:
                    parts = line[8:].split()
                    current_offset = offset
                    for p in parts:
                        if len(p) == 2:
                            try:
                                val = int(p, 16)
                                if 0 <= current_offset < 1000:
                                    color_data[current_offset] = val & 0x0F
                                    current_offset += 1
                            except ValueError:
                                pass
                        elif len(p) > 2:
                            break
            except ValueError:
                pass
    
    return color_data


def get_sprite_data(s):
    """Read sprite positions and enable status."""
    # Sprite X positions: $D000, $D002, $D004...
    # Sprite Y positions: $D001, $D003, $D005...
    # Sprite enable: $D015
    response = send_command(s, "m d000 d01f")
    
    sprites = []
    data = []
    
    for line in response.splitlines():
        if line.startswith(">C:"):
            parts = line[8:].split()
            for p in parts:
                if len(p) == 2:
                    try:
                        data.append(int(p, 16))
                    except:
                        pass
    
    if len(data) >= 22:
        enable = data[21]  # $D015
        for i in range(8):
            if enable & (1 << i):
                x = data[i * 2]
                y = data[i * 2 + 1]
                sprites.append({'id': i, 'x': x, 'y': y})
    
    return sprites


def get_game_vars(s):
    """Read common game variables from zero page."""
    response = send_command(s, "m 0002 000f")
    
    vars = {}
    data = []
    for line in response.splitlines():
        if line.startswith(">C:"):
            parts = line[8:].split()
            for p in parts:
                if len(p) == 2:
                    try:
                        data.append(int(p, 16))
                    except:
                        pass
    
    if len(data) >= 2:
        vars['head_x'] = data[0]
        vars['head_y'] = data[1]
    
    return vars


# =============================================================================
# Screen Display
# =============================================================================

def print_screen(screen_data, color_data=None, use_color=True):
    """Print screen RAM as ASCII with optional colors."""
    if not screen_data:
        print("No screen data.")
        return

    print("-" * 42)
    for y in range(25):
        row_str = "|"
        for x in range(40):
            idx = y * 40 + x
            if idx < len(screen_data):
                code = screen_data[idx]
                char = SCREEN_CODE_MAP.get(code, '?')
                if code == 32: 
                    char = ' '
                
                if use_color and color_data:
                    color = color_data[idx]
                    ansi = ANSI_COLORS.get(color, RESET)
                    row_str += f"{ansi}{char}{RESET}"
                else:
                    row_str += char
            else:
                row_str += " "
        row_str += "|"
        print(row_str)
    print("-" * 42)


# =============================================================================
# Screenshot-based Vision (for sprites and graphics)
# =============================================================================

def take_screenshot(output_path, timeout=2.0):
    """Take screenshot via VICE monitor."""
    if not HAS_PIL:
        return False
    
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(timeout)
        s.connect(('localhost', 6510))
        s.sendall(f'screenshot "{output_path}" 2\n'.encode())
        time.sleep(0.5)
        s.close()
        return os.path.exists(output_path)
    except:
        return False


def screenshot_to_ascii(image_path, width=80, height=25):
    """Convert screenshot to ASCII art."""
    if not HAS_PIL:
        return "PIL not available"
    
    chars = ' .:-=+*#%@'
    try:
        img = Image.open(image_path)
        img = img.convert('L')
        img = img.resize((width, height))
        
        pixels = list(img.getdata())
        lines = []
        for y in range(height):
            line = ''
            for x in range(width):
                pixel = pixels[y * width + x]
                char_idx = min(int(pixel / 256 * len(chars)), len(chars) - 1)
                line += chars[char_idx]
            lines.append(line)
        return '\n'.join(lines)
    except Exception as e:
        return f"Error: {e}"


def look_screenshot():
    """Take screenshot and return ASCII representation."""
    tmp_path = '/tmp/vice_ai_screen.png'
    if take_screenshot(tmp_path):
        result = screenshot_to_ascii(tmp_path)
        try:
            os.remove(tmp_path)
        except:
            pass
        return result
    return "Failed to take screenshot"


# =============================================================================
# Build & Run Functions
# =============================================================================

def build_project(project_dir):
    """Build project using build.sh."""
    build_script = os.path.join(project_dir, 'build.sh')
    if not os.path.exists(build_script):
        return False, f"No build.sh in {project_dir}"
    
    try:
        result = subprocess.run(
            ['bash', './build.sh'],
            cwd=project_dir,
            capture_output=True,
            text=True,
            timeout=30
        )
        return result.returncode == 0, result.stdout + result.stderr
    except Exception as e:
        return False, str(e)


def start_vice(project_dir, prg_file=None):
    """Start VICE with project."""
    if prg_file is None:
        prg_files = list(Path(project_dir).glob('*.prg'))
        if not prg_files:
            return False
        prg_file = prg_files[0].name
    
    env = os.environ.copy()
    for var in ['LD_LIBRARY_PATH', 'GTK_PATH', 'GIO_MODULE_DIR']:
        env.pop(var, None)
    
    try:
        subprocess.Popen(
            ['x64', '-remotemonitor', prg_file],
            cwd=project_dir,
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        time.sleep(2)
        return check_vice_running()
    except:
        return False


def reload_program(project_dir, prg_file=None):
    """Reload program in running VICE."""
    if prg_file is None:
        prg_files = list(Path(project_dir).glob('*.prg'))
        if not prg_files:
            return False
        prg_file = prg_files[0].name
    
    prg_path = os.path.abspath(os.path.join(project_dir, prg_file))
    
    try:
        # Simple approach: just send commands without waiting for full response
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1.0)
        sock.connect(('localhost', 6510))
        
        sock.sendall(b"reset 0\n")
        time.sleep(1.0)
        
        sock.sendall(f'l "{prg_path}" 0\n'.encode())
        time.sleep(0.5)
        
        sock.sendall(b"g 0810\n")
        time.sleep(0.3)
        
        sock.close()
        return True
    except Exception as e:
        print(f"Reload error: {e}")
        return False


# =============================================================================
# AI Development Loop
# =============================================================================

def development_loop(project_dir, iterations=1, use_screenshot=False):
    """
    Run the AI development feedback loop.
    
    This is the core function for AI-assisted development:
    1. Build code
    2. Run/reload in VICE
    3. Capture visual state
    4. Return analysis for AI to process
    """
    print(f"\n{'='*60}")
    print(f"AI Development Loop - {iterations} iteration(s)")
    print(f"Project: {project_dir}")
    print(f"{'='*60}\n")
    
    for i in range(iterations):
        print(f"\n--- Iteration {i+1}/{iterations} ---\n")
        
        # Build
        print("Building...")
        success, output = build_project(project_dir)
        if not success:
            print(f"✗ Build failed:\n{output}")
            continue
        print("✓ Build successful")
        
        # Run/Reload
        if check_vice_running():
            print("Reloading in VICE...")
            reload_program(project_dir)
        else:
            print("Starting VICE...")
            start_vice(project_dir)
        
        # Wait for game to initialize
        print("Waiting for game to start...")
        time.sleep(3)
        
        # Capture
        print("\nCapturing screen state...")
        if use_screenshot:
            screen = look_screenshot()
            if "Failed" not in screen and "Error" not in screen:
                print("\n" + "="*80)
                print(screen)
                print("="*80)
            else:
                print(f"Screenshot failed: {screen}")
        else:
            s = connect_vice()
            if s:
                vars = get_game_vars(s)
                screen_data = get_screen(s)
                color_data = get_color_ram(s)
                sprites = get_sprite_data(s)
                s.send(b"x\n")  # Resume
                s.close()
                
                print(f"\nGame State: {vars}")
                print(f"Sprites: {len(sprites)} active")
                print_screen(screen_data, color_data)
            else:
                print("Could not connect to VICE")
        
        if i < iterations - 1:
            time.sleep(1)
    
    print(f"\n{'='*60}")
    print("Loop complete")
    print(f"{'='*60}\n")


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='AI-Assisted C64 Development Toolchain',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Modes:
  (default)         Interactive screen monitoring
  --once            Single capture and exit
  --frames N        Capture N frames
  --screenshot      Use screenshot method (shows sprites)
  --loop DIR [N]    Full build/run/capture cycle

Examples:
  %(prog)s                           # Monitor screen RAM
  %(prog)s --once                    # Single snapshot
  %(prog)s --screenshot              # Screenshot-based view
  %(prog)s --loop snake2/ 3          # 3 dev cycles

AI Feedback Loop:
  This enables AI to iteratively develop C64 games by "seeing"
  the screen output and making informed code changes.
        """
    )
    
    parser.add_argument('--once', action='store_true', help='Single capture')
    parser.add_argument('--frames', type=int, default=0, help='Capture N frames')
    parser.add_argument('--screenshot', action='store_true', help='Use screenshot')
    parser.add_argument('--loop', metavar='DIR', help='Development loop')
    parser.add_argument('--iterations', '-n', type=int, default=1, help='Loop iterations')
    parser.add_argument('--no-color', action='store_true', help='Disable colors')
    
    args = parser.parse_args()
    
    # Development loop mode
    if args.loop:
        development_loop(args.loop, args.iterations, args.screenshot)
        return 0
    
    # Screenshot mode
    if args.screenshot:
        if not check_vice_running():
            print("VICE is not running. Start with: x64 -remotemonitor game.prg")
            return 1
        print(look_screenshot())
        return 0
    
    # Default: Interactive RAM monitoring
    s = connect_vice()
    if not s:
        print("Could not connect to VICE.")
        print("Start with: x64 -remotemonitor snake.prg")
        return 1
    
    frames = 0
    while True:
        game_vars = get_game_vars(s)
        screen = get_screen(s)
        color = get_color_ram(s)
        sprites = get_sprite_data(s)
        
        if not args.once:
            print("\033[2J\033[H")  # Clear
        
        print(f"Head: ({game_vars.get('head_x', '?')}, {game_vars.get('head_y', '?')})  Sprites: {len(sprites)}")
        print_screen(screen, color, not args.no_color)
        
        if args.once:
            break
        
        if args.frames > 0:
            frames += 1
            if frames >= args.frames:
                break
        
        s.send(b"x\n")  # Resume
        time.sleep(0.2)
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
