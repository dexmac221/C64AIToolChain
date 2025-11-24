import socket
import time
import sys

# C64 Screen Codes to ASCII (Simplified)
SCREEN_CODE_MAP = {
    32: ' ',  # Space
    81: 'O',  # Snake (Circle)
    160: '#', # Wall (Block)
    83: 'A',  # Apple (Heart)
    65: 'T',  # Tree (Spade)
}

# Add numbers 0-9
for i in range(48, 58):
    SCREEN_CODE_MAP[i] = chr(i)
# Add letters A-F (Screen Codes 1-6)
for i in range(1, 7):
    SCREEN_CODE_MAP[i] = chr(64 + i) # 1='A', 2='B'...

def connect_vice(host='localhost', port=6510):
    print(f"Attempting to connect to VICE at {host}:{port}...")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0) # 2 second timeout
        s.connect((host, port))
        # Receive welcome message
        try:
            s.recv(1024)
        except socket.timeout:
            pass # Welcome message might be delayed or already sent
        print("Connected successfully!")
        return s
    except ConnectionRefusedError:
        print("Could not connect to VICE. Make sure it is running with -remotemonitor")
        print("Example: x64 -remotemonitor snake.prg")
        return None

def send_command(s, cmd):
    try:
        s.send(f"{cmd}\n".encode())
        time.sleep(0.05) # Reduced latency
        data = b""
        while True:
            chunk = s.recv(4096)
            data += chunk
            # print(f"DEBUG: Received chunk: {chunk}") 
            if b"(C:$" in chunk: # Prompt end
                break
            if len(chunk) == 0:
                break
        return data.decode()
    except BrokenPipeError:
        print("Connection lost.")
        sys.exit(1)

def get_screen(s):
    # Dump screen RAM $0400 - $07E7 (1000 chars)
    # Monitor command: m 0400 07e7
    response = send_command(s, "m 0400 07e7")
    
    # Parse hex dump
    screen_data = [32] * 1000 # Initialize with spaces
    lines = response.splitlines()
    for line in lines:
        if line.startswith(">C:"):
            try:
                # Parse address
                # >C:0400 ...
                addr_str = line[3:7]
                addr = int(addr_str, 16)
                
                # Calculate offset from $0400
                offset = addr - 0x0400
                
                if 0 <= offset < 1000:
                    # Parse hex bytes
                    # Heuristic: split the rest of the line and find hex bytes
                    # The line usually has the address, then hex bytes, then ascii
                    # We can just look for 2-digit hex strings after the address
                    
                    parts = line[8:].split() # Skip address part
                    
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
                            # Likely the start of ASCII dump or garbage
                            break
            except ValueError:
                pass
    
    return screen_data

def print_screen(screen_data):
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
                char = SCREEN_CODE_MAP.get(code, ' ') # Default to space for unknown to reduce noise
                row_str += char
            else:
                row_str += " "
        row_str += "|"
        print(row_str)
    print("-" * 42)

def get_vars(s):
    # Read head_x ($02) and head_y ($03)
    # Monitor command: m 0002 0003
    response = send_command(s, "m 0002 0003")
    
    vars = {}
    lines = response.splitlines()
    for line in lines:
        if line.startswith(">C:"):
            parts = line[8:].split()
            try:
                if len(parts) >= 1: vars['head_x'] = int(parts[0], 16)
                if len(parts) >= 2: vars['head_y'] = int(parts[1], 16)
            except ValueError:
                pass
    return vars

def main():
    frames = 0
    max_frames = 0
    once = False
    
    if len(sys.argv) > 1:
        if sys.argv[1] == "--once":
            once = True
        elif sys.argv[1] == "--frames":
            max_frames = int(sys.argv[2])

    s = connect_vice()
    if not s:
        sys.exit(1)
    
    if not once:
        print("Connected to VICE Monitor.")
    
    while True:
        # Get variables first
        game_vars = get_vars(s)
        screen = get_screen(s)
        
        if not once:
            print("\033[2J\033[H") # Clear console
        
        print(f"Head X: {game_vars.get('head_x', '?')}  Head Y: {game_vars.get('head_y', '?')}")
        print_screen(screen)
        
        if once:
            break
        
        if max_frames > 0:
            frames += 1
            if frames >= max_frames:
                break
        
        # Resume execution (exit monitor) so the game can run
        s.send(b"x\n")
            
        time.sleep(0.2) # Faster updates

if __name__ == "__main__":
    main()
