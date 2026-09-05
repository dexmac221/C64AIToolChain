"""VICE 3.7 text monitor: finish the monitor session before closing TCP.

A paused transaction must use ``with ViceMonitor(...) as monitor``. The
connection stays open through verification and execution. Leaving the context
resumes VICE (or resets on error when requested); it never abandons the monitor.
"""
import re
import socket
import time
from typing import Iterable

VICE_HOST = '127.0.0.1'
VICE_PORT = 6510
PROMPT = re.compile(rb'\(C:\$[0-9a-fA-F]+\)\s*$')


class ViceMonitor:
    def __init__(self, host=VICE_HOST, port=VICE_PORT, timeout=1.0, *, reset_on_error=False):
        self.host, self.port, self.timeout = host, port, timeout
        self.reset_on_error = reset_on_error
        self._socket = None
        self._managed = False

    def __enter__(self):
        if self._managed:
            raise RuntimeError('nested VICE monitor contexts are not supported')
        self._managed = True
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        try:
            self.close(reset=exc_type is not None and self.reset_on_error)
        finally:
            self._managed = False

    def _connect(self):
        if self._socket is None:
            self._socket = socket.create_connection((self.host, self.port), self.timeout)
            self._drain_greeting(self._socket)
        return self._socket

    def _drain_greeting(self, sock):
        # VICE can delay its initial prompt until after the first command.
        # Synchronize with a read-only command whose response has known content;
        # a prompt alone is NOT acknowledgment of that command.
        sock.sendall(b'r\n')
        response = ''
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            response += self._read_available(sock)
            if re.search(r'(?m)^\.;[0-9a-fA-F]{4}\s', response):
                return
        raise OSError('VICE monitor greeting could not be synchronized')

    def _read_available(self, sock):
        data = bytearray()
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            sock.settimeout(max(0.001, deadline - time.monotonic()))
            try:
                chunk = sock.recv(4096)
            except socket.timeout:
                break
            if not chunk:
                break
            data.extend(chunk)
            if len(data) > 2_000_000:
                raise OSError('VICE response exceeds the bounded receive buffer')
            if PROMPT.search(data):
                response = data.decode(errors='replace')
                cleaned = re.sub(r'\(C:\$[0-9a-fA-F]+\) *', '', response)
                if re.search(r'(?im)^\s*(?:error|failed|unknown (?:command|resource)|syntax error)\b', cleaned):
                    raise OSError(f'VICE rejected command: {cleaned.strip()}')
                return response
        raise OSError('VICE response incomplete or timed out')

    def _finish(self, command):
        sock, self._socket = self._socket, None
        if sock is None:
            return
        try:
            sock.sendall((command + '\n').encode())
            # Drain the final response before closing. Do not wait for a prompt:
            # x/g/reset return control to the emulated CPU, which has no prompt.
            sock.shutdown(socket.SHUT_WR)
            deadline = time.monotonic() + min(self.timeout, 0.2)
            while time.monotonic() < deadline:
                sock.settimeout(max(0.001, deadline - time.monotonic()))
                try:
                    if not sock.recv(4096):
                        break
                except socket.timeout:
                    break
        finally:
            sock.close()

    def close(self, *, reset=False):
        try:
            self._finish('reset 0' if reset else 'x')
        except OSError:
            # Cleanup must not hide the original failure or retry on a dead peer.
            pass

    def start(self, address):
        if type(address) is not int or not 0 <= address <= 65535:
            raise ValueError('invalid execution address')
        try:
            self._connect()
            self._finish(f'g {address:04x}')
        except BaseException:
            if not self._managed:
                self.close()
            raise

    def command(self, command_text, resume=True):
        return self.command_sequence([command_text], resume=resume)[command_text]

    def command_sequence(self, commands: Iterable[str], resume=True):
        if not resume and not self._managed:
            raise RuntimeError('paused VICE transactions require a with ViceMonitor(...) context')
        state = {}
        try:
            sock = self._connect()
            for command in commands:
                if not command.strip() or any(c in command for c in ('\n', '\r')):
                    raise ValueError('one nonempty monitor command per request is required')
                sock.sendall((command.strip() + '\n').encode())
                state[command] = self._read_available(sock)
            if resume:
                self._finish('x')
            return state
        except BaseException:
            if not self._managed:
                self.close()
            raise

    def command_sequence_with_write(self, commands, write_command, resume=True):
        return self.command_sequence([*commands, *([write_command] if write_command else [])], resume=resume)

    def pause_with_state(self, commands):
        """Pause only inside the caller's live context; context exit resumes."""
        return self.command_sequence(commands, resume=False)


def parse_monitor_bytes(response: str, start_address: int, expected_length: int) -> list[int]:
    memory = [0] * expected_length
    seen = set()
    for line in response.splitlines():
        marker = line.find(">C:")
        if marker < 0:
            continue
        line = line[marker:]
        try:
            address = int(line[3:7], 16)
        except ValueError:
            continue
        offset = address - start_address
        if offset >= expected_length:
            continue
        for token in line[8:].split():
            if len(token) != 2:
                break
            try:
                value = int(token, 16)
            except ValueError:
                break
            if 0 <= offset < expected_length:
                memory[offset] = value
                seen.add(offset)
            offset += 1
    if len(seen) != expected_length:
        raise OSError(f"incomplete VICE dump: {len(seen)}/{expected_length} bytes at ${start_address:04x}")
    return memory
