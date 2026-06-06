from PIL import Image
import numpy as np
import sys

def convert_to_raw(input_path, output_path, width=256, height=256):
    img = Image.open(input_path)
    img = img.convert("L")          # grayscale
    img = img.resize((width, height))
    np.array(img, dtype=np.uint8).tofile(output_path)
    print(f"Converted {input_path} → {output_path} ({width}x{height})")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 convert_image.py <input.jpg> <output.raw> [width] [height]")
        sys.exit(1)
    
    input_path  = sys.argv[1]
    output_path = sys.argv[2]
    width       = int(sys.argv[3]) if len(sys.argv) > 3 else 256
    height      = int(sys.argv[4]) if len(sys.argv) > 4 else 256
    
    convert_to_raw(input_path, output_path, width, height)