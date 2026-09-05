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
    python3 reload_game.py --start 080d snake  # Override start address (hex)

Requirements:
    - VICE running with -remotemonitor flag (port 6510)

Author: C64AIToolChain Project
"""

import argparse
import re
import sys
from pathlib import Path
from vice_monitor import ViceMonitor, parse_monitor_bytes


def read_prg(path):
    data = Path(path).read_bytes()
    if len(data) < 3:
        raise ValueError('PRG is empty or missing its load address')
    address = int.from_bytes(data[:2], 'little')
    if address + len(data) - 2 > 65536:
        raise ValueError('PRG extends beyond C64 memory')
    return address, data[2:]


def detect_start_address(path):
    """Accept a literal SYS in the first BASIC line; never guess an entrypoint."""
    address, payload = read_prg(path)
    if address == 0x0801 and len(payload) >= 8:
        next_line = int.from_bytes(payload[:2], 'little') - address
        if 4 < next_line <= len(payload) - 2 and payload[next_line - 1] == 0:
            line = payload[4:next_line - 1]
            match = re.fullmatch(rb' *\x9e *([0-9]+) *', line)
            if match:
                entry = int(match.group(1))
                if address <= entry < address + len(payload):
                    return entry
    raise ValueError('No supported literal BASIC SYS entrypoint; specify --start HEX')


def resolve_prg_path(game_arg):
    path = Path(game_arg)
    if path.is_file() and path.suffix.lower() == '.prg':
        return str(path.resolve())
    if path.is_dir():
        preferred = path / (path.resolve().name + '.prg')
        if preferred.is_file():
            return str(preferred.resolve())
        candidates = sorted(path.glob('*.prg'))
        if len(candidates) == 1:
            return str(candidates[0].resolve())
        # Historical output conventions.
        for name in ('snake.prg', 'tetris.prg'):
            candidate = path / name
            if candidate.is_file():
                return str(candidate.resolve())
    candidate = Path(str(path) + '.prg')
    return str(candidate.resolve()) if candidate.is_file() else None


def reload_game(prg_path, host='localhost', port=6510, start_addr=None):
    try:
        path = Path(prg_path).resolve()
        if any(c in str(path) for c in ('"', '\n', '\r')):
            raise ValueError('PRG path contains unsupported monitor characters')
        address, payload = read_prg(path)
        entry = detect_start_address(path) if start_addr is None else start_addr
        if type(entry) is not int or not address <= entry < address + len(payload):
            raise ValueError('Start address must lie inside the loaded PRG')
        with ViceMonitor(host, port, timeout=3.0, reset_on_error=True) as monitor:
            dump = f'm {address:04x} {address + len(payload) - 1:04x}'
            # Keep the CPU stopped until load contents have been checked. RAM bank
            # selection affects monitor access, not the emulated CPU's memory port.
            commands = ['bank cpu', '> d01a 00', '> d019 ff', '> 0314 31',
                        '> 0315 ea', 'bank ram', f'l "{path}" 0', dump, 'bank cpu']
            response = monitor.command_sequence(commands, resume=False)
            actual = bytes(parse_monitor_bytes(response[dump], address, len(payload)))
            if actual != payload:
                raise OSError('PRG readback differs from the file; execution not started')
            monitor.start(entry)
        print(f'PRG verified ({len(payload)} bytes); execution requested at ${entry:04x}')
        return True
    except (OSError, ValueError) as error:
        print(f'Reload failed: {error}', file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', nargs='?', default='snake')
    parser.add_argument('--start', type=lambda value: int(value, 16),
                        help='Explicit hexadecimal entrypoint; default: detect BASIC SYS')
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=6510)
    args = parser.parse_args()
    path = resolve_prg_path(args.game)
    if path is None:
        parser.error(f'Cannot resolve a unique PRG for {args.game!r}; pass its path')
    return 0 if reload_game(path, args.host, args.port, args.start) else 1


if __name__ == '__main__':
    sys.exit(main())
