#!/usr/bin/env python3
"""
Convert a PNG image to C64 character data.
Based on christmas/image.py but cleaned up.
Generates img.h, charmap.h, and clrs.h files.
"""

import math
import sys
import os
from PIL import Image

# C64 color palette (RGB values)
C64_COLORS = (
    (0, 0, 0),        # 0 - black
    (255, 255, 255),  # 1 - white
    (136, 0, 0),      # 2 - red
    (170, 255, 238),  # 3 - cyan
    (204, 68, 204),   # 4 - purple
    (0, 204, 85),     # 5 - green
    (0, 0, 170),      # 6 - blue
    (238, 238, 119),  # 7 - yellow
    (221, 136, 85),   # 8 - orange
    (102, 68, 0),     # 9 - brown
    (255, 119, 119),  # 10 - light red
    (51, 51, 51),     # 11 - dark gray
    (119, 119, 119),  # 12 - gray
    (170, 255, 102),  # 13 - light green
    (0, 136, 255),    # 14 - light blue
    (187, 187, 187)   # 15 - light gray
)

def find_closest_color(test_color):
    """Find the closest C64 color to the given RGB color."""
    min_dist = float('inf')
    min_idx = 0
    
    for idx, c64_color in enumerate(C64_COLORS):
        dist = math.sqrt(
            (c64_color[0] - test_color[0]) ** 2 +
            (c64_color[1] - test_color[1]) ** 2 +
            (c64_color[2] - test_color[2]) ** 2
        )
        if dist < min_dist:
            min_dist = dist
            min_idx = idx
    
    return min_idx

def convert_image(image_path, output_dir=None):
    """Convert a 320x200 image to C64 format."""
    
    if output_dir is None:
        output_dir = os.path.dirname(image_path)
    
    # Load image
    img = Image.open(image_path)
    
    # Ensure correct size
    if img.size != (320, 200):
        print(f"Resizing image from {img.size} to 320x200")
        img = img.resize((320, 200), Image.Resampling.LANCZOS)
    
    # Convert to RGB if necessary
    if img.mode != 'RGB':
        img = img.convert('RGB')
    
    # Keep original for color detection - don't threshold!
    # The color detection will use the original pixels
    
    char_data = {}  # Maps character bitmap pattern to index
    char_patterns = []  # List of character patterns (8 bytes each)
    screen_data = []  # Screen memory (40x25 = 1000 bytes)
    color_data = []  # Color memory (40x25 = 1000 bytes)
    
    char_index = 0
    
    # Process each 8x8 character cell
    for row in range(25):
        for col in range(40):
            # Extract 8x8 pixel block
            px_x = col * 8
            px_y = row * 8
            
            # Calculate character bitmap and find dominant color
            char_bytes = []
            pixel_on_count = 0
            # Track brightest pixel color for this cell
            brightest_pixel = (0, 0, 0)
            brightest_value = 0
            
            for y in range(8):
                byte_val = 0
                bit = 128
                for x in range(8):
                    pixel = img.getpixel((px_x + x, px_y + y))
                    
                    # For colored text on black: non-black pixels are ON
                    brightness = (pixel[0] + pixel[1] + pixel[2]) / 3
                    if brightness > 32:  # Lower threshold for colored pixels
                        byte_val |= bit  # Foreground pixel
                        pixel_on_count += 1
                        # Track the brightest pixel to determine the true color
                        if brightness > brightest_value:
                            brightest_value = brightness
                            brightest_pixel = pixel
                    # else: bit stays off (background)
                    bit >>= 1
                
                char_bytes.append(byte_val)
            
            # Determine color from the brightest pixel (least affected by anti-aliasing)
            if pixel_on_count > 0 and brightest_value > 32:
                color_idx = find_closest_color(brightest_pixel)
            else:
                color_idx = 0  # Black for empty cells
            
            # Convert to pattern key
            pattern_key = tuple(char_bytes)
            
            # Check if we already have this pattern
            if pattern_key in char_data:
                screen_idx = char_data[pattern_key]
            else:
                if char_index < 256:
                    char_data[pattern_key] = char_index
                    char_patterns.append(char_bytes)
                    screen_idx = char_index
                    char_index += 1
                else:
                    # Find closest existing pattern
                    min_diff = float('inf')
                    screen_idx = 0
                    for existing_pattern, idx in char_data.items():
                        diff = sum(abs(a - b) for a, b in zip(pattern_key, existing_pattern))
                        if diff < min_diff:
                            min_diff = diff
                            screen_idx = idx
            
            screen_data.append(screen_idx)
            color_data.append(color_idx)
    
    # Pad character patterns to 256 characters
    while len(char_patterns) < 256:
        char_patterns.append([0] * 8)
    
    # Write img.h (screen data)
    img_path = os.path.join(output_dir, "img.h")
    with open(img_path, 'w') as f:
        data_str = ",".join(str(x) for x in screen_data)
        f.write(f"const unsigned char img[]={{{data_str}}};")
    print(f"Created: {img_path}")
    
    # Write charmap.h (character patterns - 256 * 8 = 2048 bytes)
    charmap_path = os.path.join(output_dir, "charmap.h")
    with open(charmap_path, 'w') as f:
        flat_data = []
        for pattern in char_patterns[:256]:
            flat_data.extend(pattern)
        data_str = ",".join(str(x) for x in flat_data)
        f.write(f"const unsigned char charmap[]={{{data_str}}};")
    print(f"Created: {charmap_path}")
    
    # Write clrs.h (color data)
    clrs_path = os.path.join(output_dir, "clrs.h")
    with open(clrs_path, 'w') as f:
        data_str = ",".join(str(x) for x in color_data)
        f.write(f"const unsigned char clrs[]={{{data_str}}};")
    print(f"Created: {clrs_path}")
    
    print(f"Total unique characters: {char_index}")
    return char_index

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 image.py <image_file>")
        sys.exit(1)
    
    convert_image(sys.argv[1])
