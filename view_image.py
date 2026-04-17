import numpy as np
import matplotlib.pyplot as plt

# 1. Load the raw binary data and reshape it into a 256x256 grid
image = np.fromfile('output.raw', dtype=np.uint8).reshape(256, 256)

# 2. Display the image using a grayscale color map
plt.imshow(image, cmap='gray')
plt.title("input.raw")
plt.axis('off') # Hides the axis numbers
plt.show()