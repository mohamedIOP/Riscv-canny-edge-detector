# Create a 256x256 raw image file with a simple gradient
with open("input.raw", "wb") as f:
    f.write(bytearray([i % 256 for i in range(256 * 256)]))
print("Successfully generated input.raw!")