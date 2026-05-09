from PIL import Image
import sys

img_path = sys.argv[1]

img = Image.open(img_path)
img = img.convert("L")
img = img.resize((128, 128))

# save as raw bytes manually
with open("input.raw", "wb") as f:
    f.write(img.tobytes())

print("Done → input.raw (128x128 grayscale)")

