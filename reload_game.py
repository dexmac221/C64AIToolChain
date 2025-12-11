#!/usr/bin/env python3
"""
reload_game.py - Hot-reload C64 programs into running VICE emulator

Connects to VICE remote monitor and injects a new .prg file without
restarting the emulator. Essential for rapid iteration during development.

Usage:
    python3 reload_game.py              # Reload snake/snake.prg (default)
    python3 reload_game.py snake2       # Reload snake2/snake2.prg
    python3 reload_game.py tetris_v1    # Reload tetris_v1/tetris_v1.prg
    python3 reload_game.py path/to/game.prg  # Reload specific file

Requirements:
    - VICE running with -remotemonitor flag (port 6510)

Author: C64AIToolChain Project
"""

import socket
import time
import sys
import os


def connect_vice(host='localhost', port=6510, timeout=3.0):
    """
    Connect to VICE remote monitor.
    
    Args:
        host: VICE host address
        port: Remote monitor port (default 6510)
        timeout: Connection timeout in seconds
        
    Returns:
        Socket connection or None on failure
    """
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(timeout)
        s.connect((host, port))
        # Clear welcome message
        try:
            s.recv(1024)
        except socket.timeout:
            pass
        return s
    except ConnectionRefusedError:
        print(f"Error: Connection refused on {host}:{port}")
        print("Is VICE running with -remotemonitor flag?")
        return None
    except socket.timeout:
        print(f"Error: Connection timeout to {host}:{port}")
        return None
    except Exception as e:
        print(f"Error connecting to VICE: {e}")
        return None


def send_command(s, cmd, timeout=1.0):
    """
    Send command to VICE monitor and get response.
    
    Args:
        s: Socket connection
        cmd: Command string to send
        timeout: Response timeout in seconds
        
    Returns:
        Response string or empty on failure
    """
    try:
        s.settimeout(timeout)
        s.send(f"{cmd}\n".encode())
        time.sleep(0.3)
        data = s.recv(4096)
        return data.decode(errors='ignore')
    except socket.timeout:
        return ""
    except BrokenPipeError:
        print("Error: Connection to VICE lost")
        return ""
    except Exception as e:
        print(f"Error sending command: {e}")
        return ""


def resolve_prg_path(game_arg):
    """
    Resolve the .prg file path from the argument.
    
    Args:
        game_arg: Game name or direct path to .prg
        
    Returns:
        Absolute path to .prg file or None if not found
    """
    # If it's already a .prg path
    if game_arg.endswith('.prg'):
        if os.path.isabs(game_arg):
            prg_path = game_arg
        else:
            prg_path = os.path.abspath(game_arg)
    else:
        # Try common patterns: game/game.prg, game.prg
        candidates = [
            f"{game_arg}/{game_arg}.prg",
            f"{game_arg}/snake.prg",  # snake2 uses snake.prg
            f"{game_arg}/tetris.prg",  # tetris uses tetris.prg
            f"{game_arg}.prg",
        ]
        prg_path = None
        for candidate in candidates:
            if os.path.exists(candidate):
                prg_path = os.path.abspath(candidate)
                break
    
    if prg_path and os.path.exists(prg_path):
        return prg_path
    return None


def reload_game(prg_path, host='localhost', port=6510):
    """
    Reload a .prg file into running VICE.
    
    Args:
        prg_path: Absolute path to the .prg file
        host: VICE host
        port: VICE monitor port
        
    Returns:
        True on success, False on failure
    """
    print(f"Connecting to VICE at {host}:{port}...")
    s = connect_vice(host, port)
    
    if not s:
        return False
    
    try:
        print(f"Loading: {prg_path}")
        
        # Load program to memory (device 0 = computer memory)
        response = send_command(s, f'l "{prg_path}" 0', timeout=2.0)
        if "Error" in response or "error" in response:
            print(f"Load error: {response}")
            return False
        
        # Small delay to ensure load completes
        time.sleep(0.2)
        
        # Start execution at $080D (SYS 2061, standard cc65 entry point)
        # This is after the BASIC stub
        send_command(s, 'g 080d', timeout=0.5)
        
        print("✓ Reload complete - game running")
        return True
        
    except Exception as e:
        print(f"Error during reload: {e}")
        return False
    finally:
        try:
            s.close()
        except:
            pass


def main():
    """Main entry point."""
    # Parse arguments
    game = "snake"
    if len(sys.argv) > 1:
        game = sys.argv[1]
    
    # Handle --help
    if game in ('-h', '--help'):
        print(__doc__)
        return 0
    
    # Resolve path
    prg_path = resolve_prg_path(game)
    
    if not prg_path:
        print(f"Error: Could not find .prg file for '{game}'")
        print("Try: python3 reload_game.py snake2")
        print("  or: python3 reload_game.py path/to/game.prg")
        return 1
    
    # Reload
    success = reload_game(prg_path)
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
