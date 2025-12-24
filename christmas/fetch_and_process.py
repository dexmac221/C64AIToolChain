import sys
import requests
from PIL import Image, ImageEnhance, ImageFilter
import os
import subprocess

# Official C64 color palette
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

def process_image(url, use_improved=True):
    print(f"Downloading image from {url}...")
    try:
        headers = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'}
        response = requests.get(url, headers=headers, stream=True)
        response.raise_for_status()
        
        # Open image from stream
        img = Image.open(response.raw)
        
        # Convert to RGB
        img = img.convert('RGB')
        
        # Resize to 320x200
        print("Resizing to 320x200...")
        img = img.resize((320, 200), Image.Resampling.LANCZOS)
        
        # Enhance Contrast
        print("Enhancing contrast...")
        enhancer = ImageEnhance.Contrast(img)
        img = enhancer.enhance(1.3)
        
        # Enhance Saturation (helps with C64 limited palette)
        print("Enhancing saturation...")
        enhancer = ImageEnhance.Color(img)
        img = enhancer.enhance(1.2)
        
        # Enhance Sharpness
        print("Sharpening...")
        enhancer = ImageEnhance.Sharpness(img)
        img = enhancer.enhance(1.2)

        # Save processed image
        output_filename = "downloaded.png"
        img.save(output_filename)
        print(f"Saved processed image to {output_filename}")
        
        return output_filename, use_improved

    except Exception as e:
        print(f"Error: {e}")
        return None, use_improved

def run_pipeline(image_file, use_improved=True):
    if not image_file:
        return

    if use_improved:
        print("Running improved image conversion (image_improved.py)...")
        try:
            subprocess.run(["python3", "image_improved.py", image_file, "--preview"], check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error running image_improved.py: {e}")
            print("Falling back to original converter...")
            subprocess.run(["python3", "image.py", image_file], check=True)
    else:
        print("Running image conversion (image.py)...")
        try:
            subprocess.run(["python3", "image.py", image_file], check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error running image.py: {e}")
            return

    print("Building project (make)...")
    try:
        subprocess.run(["make"], check=True)
        print("Build successful! You can now run ./run_vice.sh")
    except subprocess.CalledProcessError as e:
        print(f"Error running make: {e}")
        return

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 fetch_and_process.py <image_url> [--simple]")
        print("  --simple: Use original simple converter instead of improved one")
        sys.exit(1)
    
    url = sys.argv[1]
    use_improved = "--simple" not in sys.argv
    
    saved_file, use_improved = process_image(url, use_improved)
    run_pipeline(saved_file, use_improved)
