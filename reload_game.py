import socket
import time
import sys

def connect_vice(host='localhost', port=6510):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((host, port))
        try:
            s.recv(1024)
        except socket.timeout:
            pass
        return s
    except:
        return None

def send_command(s, cmd):
    s.send(f"{cmd}\n".encode())
    time.sleep(0.5)
    try:
        data = s.recv(4096)
        return data.decode()
    except:
        return ""

s = connect_vice()
if s:
    game = "snake"
    if len(sys.argv) > 1:
        game = sys.argv[1]
        
    prg_path = f"{game}/{game}.prg"
    print(f"Reloading {prg_path}...")
    
    # Load to memory
    print(send_command(s, f'l "{prg_path}" 0'))
    # Start game (SYS 2061 = $080D)
    print(send_command(s, 'g 080d'))
    s.close()
else:
    print("Could not connect to VICE.")
