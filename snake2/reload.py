#!/usr/bin/env python3
"""
Quick reload script for snake2 - hot-reloads the game into running VICE
"""
import socket
import time
import os

def reload_snake2():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect(('localhost', 6510))

        # Receive welcome message
        try:
            s.recv(1024)
        except socket.timeout:
            pass

        # Get absolute path to snake.prg
        prg_path = os.path.abspath('snake.prg')
        print(f"Reloading: {prg_path}")

        # Soft reset (keeps VICE running)
        s.send(b"reset 0\n")
        time.sleep(0.5)

        # Load program
        s.send(f'l "{prg_path}" 0\n'.encode())
        time.sleep(0.5)

        # Start at $080D (2061 decimal - BASIC start)
        s.send(b"g 080d\n")
        time.sleep(0.2)

        s.close()
        print("✓ Reloaded successfully!")
        return True

    except ConnectionRefusedError:
        print("✗ Could not connect to VICE.")
        print("  Make sure VICE is running with -remotemonitor")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

if __name__ == "__main__":
    reload_snake2()
