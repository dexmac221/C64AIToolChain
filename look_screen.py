#!/usr/bin/env python3
"""
look_screen.py - C64 VICE Screenshot Viewer for AI Analysis

This script captures a screenshot from VICE emulator via remote monitor
and converts it to ASCII art for text-based AI analysis.

Usage:
    python3 look_screen.py [options]

Options:
    -o, --output FILE    Save screenshot to FILE (default: vice_screen.png)
    -w, --width WIDTH    ASCII output width (default: 80)
    -h, --height HEIGHT  ASCII output height (default: 25)
    --no-save            Don't save PNG, just display ASCII
    --charset CHARSET    Character set for ASCII art (default: standard)
                         Options: standard, blocks, simple, detailed
    --help               Show this help message

Examples:
    python3 look_screen.py                    # Take screenshot, show ASCII
    python3 look_screen.py -o game.png        # Save as game.png
    python3 look_screen.py --charset blocks   # Use block characters
    python3 look_screen.py -w 120 -h 40       # Higher resolution ASCII

For AI/LLM Integration:
    This tool enables vision-less AI models to "see" the C64 screen
    by converting the graphical output to text representation.

Requirements:
    - VICE emulator running with -remotemonitor flag
    - PIL/Pillow library (pip install Pillow)
    - netcat (nc) for VICE communication

Author: AI Toolchain Project
Date: December 2024
"""

import subprocess
import socket
import time
import sys
import os
import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow library required. Install with: pip install Pillow")
    sys.exit(1)


# Character sets for different ASCII art styles
CHARSETS = {
    'standard': ' .:-=+*#%@',
    'blocks': ' ░▒▓█',
    'simple': ' .-+#',
    'detailed': ' .\'`^",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$',
    'c64': ' ·•○●□■▪▫',
}

# ANSI 256-color escape codes
def rgb_to_ansi256(r, g, b):
    """Convert RGB to nearest ANSI 256 color code."""
    # Check for grayscale
    if r == g == b:
        if r < 8:
            return 16
        if r > 248:
            return 231
        return round((r - 8) / 247 * 24) + 232
    
    # Convert to 6x6x6 color cube
    return 16 + (36 * round(r / 255 * 5)) + (6 * round(g / 255 * 5)) + round(b / 255 * 5)

def ansi_fg(code):
    """Return ANSI escape sequence for foreground color."""
    return f"\033[38;5;{code}m"

def ansi_bg(code):
    """Return ANSI escape sequence for background color."""
    return f"\033[48;5;{code}m"

def ansi_reset():
    """Return ANSI reset sequence."""
    return "\033[0m"

# True color (24-bit) ANSI codes
def ansi_fg_rgb(r, g, b):
    """Return ANSI escape sequence for 24-bit foreground color."""
    return f"\033[38;2;{r};{g};{b}m"

def ansi_bg_rgb(r, g, b):
    """Return ANSI escape sequence for 24-bit background color."""
    return f"\033[48;2;{r};{g};{b}m"


def take_vice_screenshot(output_path: str, timeout: float = 3.0) -> bool:
    """
    Take a screenshot from VICE via remote monitor.
    
    Args:
        output_path: Full path for the PNG screenshot
        timeout: Timeout in seconds for VICE communication
        
    Returns:
        True if screenshot was taken successfully
    """
    # Ensure absolute path
    output_path = os.path.abspath(output_path)
    
    # VICE remote monitor command for PNG screenshot (format 2)
    command = f'screenshot "{output_path}" 2\n'
    
    try:
        # Connect to VICE monitor
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect(('localhost', 6510))
        
        # Send screenshot command
        sock.sendall(command.encode())
        
        # Wait briefly for VICE to process
        time.sleep(0.5)
        
        # Read response (optional, just to clear buffer)
        try:
            sock.recv(1024)
        except socket.timeout:
            pass
        
        sock.close()
        
        # Check if file was created
        time.sleep(0.3)
        return os.path.exists(output_path)
        
    except socket.error as e:
        print(f"Error connecting to VICE: {e}")
        print("Make sure VICE is running with -remotemonitor flag")
        return False


def image_to_ascii(image_path: str, width: int = 80, height: int = 25, 
                   charset: str = 'standard', color_mode: str = 'none') -> str:
    """
    Convert an image to ASCII art.
    
    Args:
        image_path: Path to the image file
        width: Output width in characters
        height: Output height in characters
        charset: Name of character set to use
        color_mode: 'none', '256', 'truecolor', or 'bg' (background blocks)
        
    Returns:
        ASCII art string representation of the image
    """
    chars = CHARSETS.get(charset, CHARSETS['standard'])
    
    try:
        img = Image.open(image_path)
    except Exception as e:
        return f"Error loading image: {e}"
    
    # Keep original for color, create grayscale for character selection
    img_color = img.convert('RGB')
    img_gray = img.convert('L')
    
    # Resize to target dimensions
    img_color = img_color.resize((width, height))
    img_gray = img_gray.resize((width, height))
    
    # Get pixel data
    pixels_gray = list(img_gray.getdata())
    pixels_color = list(img_color.getdata())
    
    # Build ASCII output
    lines = []
    for y in range(height):
        line = ''
        for x in range(width):
            idx = y * width + x
            pixel_gray = pixels_gray[idx]
            r, g, b = pixels_color[idx]
            
            # Map pixel value (0-255) to character index
            char_idx = int(pixel_gray / 256 * len(chars))
            char_idx = min(char_idx, len(chars) - 1)
            char = chars[char_idx]
            
            if color_mode == 'none':
                line += char
            elif color_mode == '256':
                # 256-color mode
                color_code = rgb_to_ansi256(r, g, b)
                line += f"{ansi_fg(color_code)}{char}"
            elif color_mode == 'truecolor':
                # 24-bit true color mode
                line += f"{ansi_fg_rgb(r, g, b)}{char}"
            elif color_mode == 'bg':
                # Background color blocks (half-blocks for better resolution)
                line += f"{ansi_bg_rgb(r, g, b)} "
        
        if color_mode != 'none':
            line += ansi_reset()
        lines.append(line)
    
    return '\n'.join(lines)


def image_to_color_blocks(image_path: str, width: int = 80, height: int = 50) -> str:
    """
    Convert image to colored Unicode half-blocks for maximum fidelity.
    Uses upper/lower half blocks to double vertical resolution.
    
    Args:
        image_path: Path to the image file
        width: Output width in characters
        height: Output height in pixels (will be halved for block pairs)
        
    Returns:
        Colored block representation of the image
    """
    try:
        img = Image.open(image_path)
    except Exception as e:
        return f"Error loading image: {e}"
    
    img = img.convert('RGB')
    img = img.resize((width, height))
    pixels = list(img.getdata())
    
    lines = []
    # Process two rows at a time using half-blocks
    for y in range(0, height - 1, 2):
        line = ''
        for x in range(width):
            # Upper pixel
            r1, g1, b1 = pixels[y * width + x]
            # Lower pixel
            r2, g2, b2 = pixels[(y + 1) * width + x]
            
            # Use upper half block with fg=top color, bg=bottom color
            line += f"{ansi_fg_rgb(r1, g1, b1)}{ansi_bg_rgb(r2, g2, b2)}▀"
        
        line += ansi_reset()
        lines.append(line)
    
    return '\n'.join(lines)


def get_image_stats(image_path: str) -> dict:
    """Get basic statistics about the screenshot."""
    try:
        img = Image.open(image_path)
        return {
            'size': img.size,
            'mode': img.mode,
            'format': img.format,
        }
    except Exception as e:
        return {'error': str(e)}


def main():
    parser = argparse.ArgumentParser(
        description='Capture VICE screenshot and convert to ASCII art',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                        Take screenshot and display ASCII
  %(prog)s -o game.png            Save screenshot as game.png
  %(prog)s --charset blocks       Use Unicode block characters
  %(prog)s -w 120 -h 40           Higher resolution ASCII output
  %(prog)s --color                Enable 256-color output
  %(prog)s --truecolor            Enable 24-bit true color output
  %(prog)s --color-blocks         High-fidelity colored block output

For AI integration, this tool enables text-based "vision" of the C64 screen.
        """
    )
    
    parser.add_argument('-o', '--output', default='vice_screen.png',
                        help='Output filename for screenshot (default: vice_screen.png)')
    parser.add_argument('-w', '--width', type=int, default=80,
                        help='ASCII output width (default: 80)')
    parser.add_argument('-ht', '--height', type=int, default=25,
                        help='ASCII output height (default: 25)')
    parser.add_argument('--no-save', action='store_true',
                        help="Don't save the PNG file after display")
    parser.add_argument('--charset', choices=list(CHARSETS.keys()), default='standard',
                        help='Character set for ASCII art (default: standard)')
    parser.add_argument('--stats', action='store_true',
                        help='Show image statistics')
    parser.add_argument('--existing', metavar='FILE',
                        help='Convert an existing image instead of taking new screenshot')
    
    # Color options
    color_group = parser.add_mutually_exclusive_group()
    color_group.add_argument('--color', action='store_true',
                             help='Enable 256-color ANSI output')
    color_group.add_argument('--truecolor', action='store_true',
                             help='Enable 24-bit true color output')
    color_group.add_argument('--color-blocks', action='store_true',
                             help='High-fidelity colored Unicode blocks (best quality)')
    
    args = parser.parse_args()
    
    # Determine color mode
    if args.color:
        color_mode = '256'
    elif args.truecolor:
        color_mode = 'truecolor'
    elif args.color_blocks:
        color_mode = 'blocks'
    else:
        color_mode = 'none'
    
    # Determine working directory
    script_dir = Path(__file__).parent.absolute()
    
    if args.existing:
        # Use existing image
        image_path = args.existing
        if not os.path.exists(image_path):
            print(f"Error: File not found: {image_path}")
            sys.exit(1)
    else:
        # Take new screenshot
        if not os.path.isabs(args.output):
            image_path = str(script_dir / args.output)
        else:
            image_path = args.output
            
        print(f"Taking screenshot from VICE...")
        if not take_vice_screenshot(image_path):
            print("Failed to take screenshot.")
            print("Is VICE running with -remotemonitor flag?")
            sys.exit(1)
        print(f"Screenshot saved: {image_path}")
    
    # Show stats if requested
    if args.stats:
        stats = get_image_stats(image_path)
        print(f"\nImage Statistics:")
        for key, value in stats.items():
            print(f"  {key}: {value}")
        print()
    
    # Convert to ASCII
    print(f"\n{'='*args.width}")
    
    if color_mode == 'blocks':
        # Use high-fidelity color blocks
        ascii_art = image_to_color_blocks(image_path, args.width, args.height * 2)
    else:
        ascii_art = image_to_ascii(image_path, args.width, args.height, 
                                   args.charset, color_mode)
    print(ascii_art)
    print(f"{'='*args.width}\n")
    
    # Clean up if requested
    if args.no_save and not args.existing:
        os.remove(image_path)
        print("(Screenshot file removed)")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
