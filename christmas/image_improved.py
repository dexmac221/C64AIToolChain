#!/usr/bin/env python3
"""
Improved C64 Image Converter

Features:
- Proper C64 color palette with perceptual color matching (LAB color space)
- Floyd-Steinberg dithering for better color representation
- Optimized character generation
- Configurable luminance threshold
- Multiple dithering modes

Usage:
    python3 image_improved.py input.png [options]
    
Examples:
    python3 image_improved.py christmas.png --preview
    python3 image_improved.py christmas.png --threshold 100 --preview
    python3 image_improved.py christmas.png --invert --threshold 150
    python3 image_improved.py christmas.png --no-dither --no-lab
"""

import math
import sys
import argparse
from PIL import Image

# Global threshold (will be set by argparse)
LUMINANCE_THRESHOLD = 128
INVERT_PATTERN = False

# Official C64 color palette (VICE/Pepto values)
C64_PALETTE = [
    (0, 0, 0),        # 0  Black
    (255, 255, 255),  # 1  White
    (136, 0, 0),      # 2  Red
    (170, 255, 238),  # 3  Cyan
    (204, 68, 204),   # 4  Purple
    (0, 204, 85),     # 5  Green
    (0, 0, 170),      # 6  Blue
    (238, 238, 119),  # 7  Yellow
    (221, 136, 85),   # 8  Orange
    (102, 68, 0),     # 9  Brown
    (255, 119, 119),  # 10 Light Red
    (51, 51, 51),     # 11 Dark Grey
    (119, 119, 119),  # 12 Grey
    (170, 255, 102),  # 13 Light Green
    (0, 136, 255),    # 14 Light Blue
    (187, 187, 187),  # 15 Light Grey
]

# Precompute LAB values for C64 palette
def rgb_to_lab(rgb):
    """Convert RGB to LAB color space for perceptual color matching."""
    r, g, b = rgb[0] / 255.0, rgb[1] / 255.0, rgb[2] / 255.0
    
    # sRGB to linear RGB
    r = ((r + 0.055) / 1.055) ** 2.4 if r > 0.04045 else r / 12.92
    g = ((g + 0.055) / 1.055) ** 2.4 if g > 0.04045 else g / 12.92
    b = ((b + 0.055) / 1.055) ** 2.4 if b > 0.04045 else b / 12.92
    
    # Linear RGB to XYZ
    x = r * 0.4124564 + g * 0.3575761 + b * 0.1804375
    y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750
    z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041
    
    # XYZ to LAB (D65 illuminant)
    x /= 0.95047
    y /= 1.00000
    z /= 1.08883
    
    x = x ** (1/3) if x > 0.008856 else (7.787 * x) + (16/116)
    y = y ** (1/3) if y > 0.008856 else (7.787 * y) + (16/116)
    z = z ** (1/3) if z > 0.008856 else (7.787 * z) + (16/116)
    
    L = (116 * y) - 16
    a = 500 * (x - y)
    b_val = 200 * (y - z)
    
    return (L, a, b_val)

# Precompute LAB values for palette
C64_PALETTE_LAB = [rgb_to_lab(c) for c in C64_PALETTE]


def find_nearest_color(rgb, use_lab=True):
    """Find the nearest C64 color index for a given RGB value."""
    if use_lab:
        lab = rgb_to_lab(rgb)
        min_dist = float('inf')
        best_idx = 0
        
        for idx, palette_lab in enumerate(C64_PALETTE_LAB):
            # CIE76 color difference
            dL = lab[0] - palette_lab[0]
            da = lab[1] - palette_lab[1]
            db = lab[2] - palette_lab[2]
            dist = math.sqrt(dL*dL + da*da + db*db)
            
            if dist < min_dist:
                min_dist = dist
                best_idx = idx
        
        return best_idx
    else:
        # Simple Euclidean RGB distance
        min_dist = float('inf')
        best_idx = 0
        
        for idx, color in enumerate(C64_PALETTE):
            dr = rgb[0] - color[0]
            dg = rgb[1] - color[1]
            db = rgb[2] - color[2]
            dist = dr*dr + dg*dg + db*db
            
            if dist < min_dist:
                min_dist = dist
                best_idx = idx
        
        return best_idx


def apply_floyd_steinberg_dithering(img):
    """Apply Floyd-Steinberg dithering to reduce to C64 palette."""
    width, height = img.size
    pixels = list(img.getdata())
    pixels = [list(p[:3]) for p in pixels]  # Convert to mutable RGB lists
    
    for y in range(height):
        for x in range(width):
            idx = y * width + x
            old_pixel = pixels[idx]
            
            # Find nearest C64 color
            color_idx = find_nearest_color(tuple(old_pixel))
            new_pixel = C64_PALETTE[color_idx]
            
            # Calculate error
            error = [old_pixel[i] - new_pixel[i] for i in range(3)]
            
            # Set the new pixel
            pixels[idx] = list(new_pixel)
            
            # Distribute error to neighbors (Floyd-Steinberg coefficients)
            if x + 1 < width:
                for i in range(3):
                    pixels[idx + 1][i] = max(0, min(255, pixels[idx + 1][i] + error[i] * 7 / 16))
            
            if y + 1 < height:
                if x > 0:
                    for i in range(3):
                        pixels[idx + width - 1][i] = max(0, min(255, pixels[idx + width - 1][i] + error[i] * 3 / 16))
                
                for i in range(3):
                    pixels[idx + width][i] = max(0, min(255, pixels[idx + width][i] + error[i] * 5 / 16))
                
                if x + 1 < width:
                    for i in range(3):
                        pixels[idx + width + 1][i] = max(0, min(255, pixels[idx + width + 1][i] + error[i] * 1 / 16))
    
    # Create new image with dithered pixels
    result = Image.new('RGB', (width, height))
    result.putdata([tuple(int(c) for c in p) for p in pixels])
    return result


def convert_to_c64_direct(img):
    """Convert image directly to C64 palette without dithering."""
    width, height = img.size
    result = Image.new('RGB', (width, height))
    
    for y in range(height):
        for x in range(width):
            pixel = img.getpixel((x, y))[:3]
            color_idx = find_nearest_color(pixel)
            result.putpixel((x, y), C64_PALETTE[color_idx])
    
    return result


def generate_c64_data(img, use_dithering=True, threshold=128, invert=False):
    """Generate C64 character map and color data from image."""
    
    if use_dithering:
        print("Applying Floyd-Steinberg dithering...")
        img = apply_floyd_steinberg_dithering(img)
    else:
        print("Converting to C64 palette (no dithering)...")
        img = convert_to_c64_direct(img)
    
    print(f"Using luminance threshold: {threshold}, invert: {invert}")
    
    width, height = img.size
    
    # Character map: stores unique 8x8 patterns
    charmap = {}  # pattern -> index
    char_data = []  # actual byte data for each unique character
    
    # Screen map: which character index for each 40x25 position
    screen_data = []
    
    # Color map: dominant color for each 40x25 cell
    color_data = []
    
    char_counter = 0
    
    for cell_y in range(25):
        for cell_x in range(40):
            # Extract 8x8 cell
            cell_bytes = []
            cell_colors = {}
            
            for py in range(8):
                byte_val = 0
                bit = 128
                
                for px in range(8):
                    x = cell_x * 8 + px
                    y = cell_y * 8 + py
                    
                    if x < width and y < height:
                        pixel = img.getpixel((x, y))[:3]
                        color_idx = find_nearest_color(pixel, use_lab=False)  # Already quantized
                        
                        # Count color occurrences for dominant color
                        cell_colors[color_idx] = cell_colors.get(color_idx, 0) + 1
                        
                        # Determine if this pixel should be "on" (foreground)
                        # Use luminance to decide
                        lum = 0.299 * pixel[0] + 0.587 * pixel[1] + 0.114 * pixel[2]
                        
                        # Dark pixels set bit (foreground), light pixels clear (background)
                        # With invert, this is reversed
                        if invert:
                            if lum >= threshold:  # Light = foreground when inverted
                                byte_val |= bit
                        else:
                            if lum < threshold:  # Dark = foreground (bit set)
                                byte_val |= bit
                    
                    bit >>= 1
                
                cell_bytes.append(byte_val)
            
            # Create pattern key
            pattern = tuple(cell_bytes)
            
            # Check if we've seen this pattern
            if pattern in charmap:
                char_idx = charmap[pattern]
            else:
                if char_counter < 256:
                    charmap[pattern] = char_counter
                    char_data.append(cell_bytes)
                    char_idx = char_counter
                    char_counter += 1
                else:
                    # Find most similar existing pattern
                    min_diff = float('inf')
                    best_match = 0
                    
                    for existing_pattern, idx in charmap.items():
                        diff = sum(abs(a - b) for a, b in zip(pattern, existing_pattern))
                        if diff < min_diff:
                            min_diff = diff
                            best_match = idx
                    
                    char_idx = best_match
            
            screen_data.append(char_idx)
            
            # Find dominant color (most frequent, excluding black unless it's the only one)
            if cell_colors:
                # Sort by count, prefer non-black colors
                sorted_colors = sorted(cell_colors.items(), 
                                       key=lambda x: (x[0] == 0, -x[1]))
                dominant_color = sorted_colors[0][0]
            else:
                dominant_color = 0
            
            color_data.append(dominant_color)
    
    print(f"Generated {char_counter} unique characters")
    
    return screen_data, char_data, color_data, img


def save_c64_headers(screen_data, char_data, color_data):
    """Save C64 data as C header files."""
    
    # img.h - screen map
    with open("./img.h", "w") as f:
        data_str = ",".join(str(v) for v in screen_data)
        f.write(f"const unsigned char img[]={{{data_str}}};")
    
    # charmap.h - character definitions (pad to 256 chars = 2048 bytes)
    char_bytes = []
    for char in char_data:
        char_bytes.extend(char)
    
    # Pad to 2048 bytes
    while len(char_bytes) < 2048:
        char_bytes.append(0)
    
    with open("./charmap.h", "w") as f:
        data_str = ",".join(str(v) for v in char_bytes[:2048])
        f.write(f"const unsigned char charmap[]={{{data_str}}};")
    
    # clrs.h - color data
    with open("./clrs.h", "w") as f:
        data_str = ",".join(str(v) for v in color_data)
        f.write(f"const unsigned char clrs[]={{{data_str}}};")
    
    print("Saved: img.h, charmap.h, clrs.h")


def main():
    parser = argparse.ArgumentParser(
        description='Convert image to C64 format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s christmas.png --preview
  %(prog)s christmas.png --threshold 100 --preview
  %(prog)s christmas.png --invert --threshold 150
  %(prog)s christmas.png --no-dither --no-lab
        """
    )
    parser.add_argument('input', help='Input image file')
    parser.add_argument('--no-dither', action='store_true', 
                        help='Disable Floyd-Steinberg dithering')
    parser.add_argument('--preview', action='store_true',
                        help='Save preview image')
    parser.add_argument('--threshold', '-t', type=int, default=128,
                        help='Luminance threshold (0-255, default: 128). '
                             'Lower = more dark pixels as foreground')
    parser.add_argument('--invert', '-i', action='store_true',
                        help='Invert pattern: light pixels become foreground')
    parser.add_argument('--no-lab', action='store_true',
                        help='Use simple RGB distance instead of LAB color space')
    parser.add_argument('--contrast', '-c', type=float, default=1.0,
                        help='Contrast adjustment (default: 1.0, >1 = more contrast)')
    parser.add_argument('--brightness', '-b', type=float, default=1.0,
                        help='Brightness adjustment (default: 1.0, >1 = brighter)')
    
    args = parser.parse_args()
    
    print(f"Loading {args.input}...")
    img = Image.open(args.input).convert('RGB')
    
    # Resize to C64 resolution if needed
    if img.size != (320, 200):
        print(f"Resizing from {img.size} to 320x200...")
        img = img.resize((320, 200), Image.Resampling.LANCZOS)
    
    # Apply contrast/brightness adjustments
    if args.contrast != 1.0:
        from PIL import ImageEnhance
        print(f"Adjusting contrast: {args.contrast}")
        enhancer = ImageEnhance.Contrast(img)
        img = enhancer.enhance(args.contrast)
    
    if args.brightness != 1.0:
        from PIL import ImageEnhance
        print(f"Adjusting brightness: {args.brightness}")
        enhancer = ImageEnhance.Brightness(img)
        img = enhancer.enhance(args.brightness)
    
    # Generate C64 data
    screen_data, char_data, color_data, processed_img = generate_c64_data(
        img, 
        use_dithering=not args.no_dither,
        threshold=args.threshold,
        invert=args.invert
    )
    
    # Save header files
    save_c64_headers(screen_data, char_data, color_data)
    
    # Save preview if requested
    if args.preview:
        preview_file = args.input.rsplit('.', 1)[0] + '_c64_preview.png'
        processed_img.save(preview_file)
        print(f"Saved preview: {preview_file}")


if __name__ == "__main__":
    main()
