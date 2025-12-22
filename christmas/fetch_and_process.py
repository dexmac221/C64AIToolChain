import sys
import requests
from PIL import Image, ImageEnhance, ImageFilter
import os
import subprocess

def process_image(url):
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
        img = enhancer.enhance(1.5) # Increase contrast by 50%
        
        # Enhance Sharpness
        print("Sharpening...")
        enhancer = ImageEnhance.Sharpness(img)
        img = enhancer.enhance(1.2)

        # Save processed image
        output_filename = "downloaded.png"
        img.save(output_filename)
        print(f"Saved processed image to {output_filename}")
        
        return output_filename

    except Exception as e:
        print(f"Error: {e}")
        return None

def run_pipeline(image_file):
    if not image_file:
        return

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
        print("Usage: python3 fetch_and_process.py <image_url>")
        sys.exit(1)
    
    url = sys.argv[1]
    saved_file = process_image(url)
    run_pipeline(saved_file)
