#!/usr/bin/env python3
"""
Generate a Happy New Year 2026 image for C64 demo.
Creates a 320x200 image with festive text and decorations.
"""

from PIL import Image, ImageDraw, ImageFont
import os

# C64 resolution
WIDTH = 320
HEIGHT = 200

# C64 color palette (RGB values)
C64_COLORS = {
    'black': (0, 0, 0),
    'white': (255, 255, 255),
    'red': (136, 0, 0),
    'cyan': (170, 255, 238),
    'purple': (204, 68, 204),
    'green': (0, 204, 85),
    'blue': (0, 0, 170),
    'yellow': (238, 238, 119),
    'orange': (221, 136, 85),
    'brown': (102, 68, 0),
    'light_red': (255, 119, 119),
    'dark_gray': (51, 51, 51),
    'gray': (119, 119, 119),
    'light_green': (170, 255, 102),
    'light_blue': (0, 136, 255),
    'light_gray': (187, 187, 187)
}

def create_newyear_image():
    # Create black background with colored text
    img = Image.new('RGB', (WIDTH, HEIGHT), C64_COLORS['black'])
    draw = ImageDraw.Draw(img)
    
    # Try to load a font, fall back to default if not available
    try:
        font_large = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 28)
        font_medium = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 42)
    except:
        font_large = ImageFont.load_default()
        font_medium = ImageFont.load_default()
    
    # Draw "HAPPY" text - bright yellow
    text1 = "HAPPY"
    bbox1 = draw.textbbox((0, 0), text1, font=font_large)
    w1 = bbox1[2] - bbox1[0]
    x1 = (WIDTH - w1) // 2
    draw.text((x1, 45), text1, font=font_large, fill=C64_COLORS['yellow'])
    
    # Draw "NEW YEAR" text - bright cyan
    text2 = "NEW YEAR"
    bbox2 = draw.textbbox((0, 0), text2, font=font_large)
    w2 = bbox2[2] - bbox2[0]
    x2 = (WIDTH - w2) // 2
    draw.text((x2, 80), text2, font=font_large, fill=C64_COLORS['cyan'])
    
    # Draw "2026" in big letters - red
    text3 = "2026"
    bbox3 = draw.textbbox((0, 0), text3, font=font_medium)
    w3 = bbox3[2] - bbox3[0]
    x3 = (WIDTH - w3) // 2
    draw.text((x3, 120), text3, font=font_medium, fill=C64_COLORS['light_red'])
    
    return img

if __name__ == "__main__":
    img = create_newyear_image()
    
    # Save the image
    output_path = os.path.join(os.path.dirname(__file__), "newyear.png")
    img.save(output_path)
    print(f"Created: {output_path}")
    
    # Also create a preview
    preview_path = os.path.join(os.path.dirname(__file__), "newyear_preview.png")
    img.save(preview_path)
    print(f"Preview: {preview_path}")
