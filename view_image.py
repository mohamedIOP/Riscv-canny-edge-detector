from PIL import Image
import sys

path   = sys.argv[1]
width  = int(sys.argv[2])
height = int(sys.argv[3])

with open(path, "rb") as f:
    data = f.read()

img = Image.frombytes("L", (width, height), data)
img.save(path.replace(".raw", ".png"))
print(f"Saved: {path.replace('.raw', '.png')}")
