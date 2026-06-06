import numpy as np
import matplotlib.pyplot as plt
import sys
import os

def view_raw(filepath, width, height, title=None):
    expected = width * height
    actual = os.path.getsize(filepath)
    
    if actual != expected:
        print(f"WARNING: File size mismatch!")
        print(f"  Expected: {expected} bytes ({width}x{height})")
        print(f"  Actual:   {actual} bytes")
        print(f"  Try different dimensions.")
        return

    img = np.fromfile(filepath, dtype=np.uint8).reshape(height, width)
    plt.figure(figsize=(6, 6))
    plt.imshow(img, cmap='gray', vmin=0, vmax=255)
    plt.title(title or os.path.basename(filepath))
    plt.colorbar(label='Pixel intensity')
    plt.tight_layout()
    plt.show()

def compare_vlen_outputs(width, height):
    files = {
        128: 'Output_Images/output_128.raw',
        256: 'Output_Images/output_256.raw',
        512: 'Output_Images/output_512.raw',
    }

    imgs = {}
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    fig.suptitle('VLEN Sweep — All three should look identical', fontsize=14)

    for ax, (vlen, path) in zip(axes, files.items()):
        if not os.path.exists(path):
            ax.set_title(f'VLEN={vlen} — NOT FOUND')
            ax.axis('off')
            continue

        actual   = os.path.getsize(path)
        expected = width * height
        if actual != expected:
            ax.set_title(f'VLEN={vlen} — SIZE MISMATCH\n({actual} vs {expected})')
            ax.axis('off')
            continue

        img = np.fromfile(path, dtype=np.uint8).reshape(height, width)
        imgs[vlen] = img
        ax.imshow(img, cmap='gray', vmin=0, vmax=255)
        ax.set_title(f'VLEN={vlen}')

    plt.tight_layout()
    plt.show()

    if len(imgs) == 3:
        v = list(imgs.values())
        if np.array_equal(v[0], v[1]) and np.array_equal(v[1], v[2]):
            print("✅ All three VLEN outputs are IDENTICAL — pipeline is vector-length-agnostic!")
        else:
            print("❌ Outputs DIFFER between VLEN values — there is a bug!")
            print(f"   Differing pixels (128 vs 256): {np.sum(v[0] != v[1])}")
            print(f"   Differing pixels (256 vs 512): {np.sum(v[1] != v[2])}")

def view_all_stages(width, height):
    stages = [
        ('Output_Images/output_gaussian.raw',      'Gaussian Blur'),
        ('Output_Images/output_sobel_gx.raw',      'Sobel Gx'),
        ('Output_Images/output_sobel_gy.raw',      'Sobel Gy'),
        ('Output_Images/output_magnitude_l1.raw',  'Magnitude L1'),
        ('Output_Images/output_128.raw',           'Magnitude L2 (edges)'),
        ('Output_Images/output_direction.raw',     'Gradient Direction'),
    ]

    fig, axes = plt.subplots(1, len(stages), figsize=(22, 4))
    fig.suptitle('Canny Pipeline — All Stages', fontsize=14)

    for ax, (path, title) in zip(axes, stages):
        if not os.path.exists(path):
            ax.set_title(f'{title}\nNOT FOUND')
            ax.axis('off')
            continue

        actual   = os.path.getsize(path)
        expected = width * height
        if actual != expected:
            ax.set_title(f'{title}\nSIZE MISMATCH\n({actual} vs {expected})')
            ax.axis('off')
            continue

        img = np.fromfile(path, dtype=np.uint8).reshape(height, width)
        ax.imshow(img, cmap='gray', vmin=0, vmax=255)
        ax.set_title(title)
        ax.axis('off')

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "stages":
        # All stages: python3 view_image.py stages <width> <height>
        view_all_stages(int(sys.argv[2]), int(sys.argv[3]))

    elif len(sys.argv) == 4:
        # Single file: python3 view_image.py <file.raw> <width> <height>
        view_raw(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))

    elif len(sys.argv) == 3:
        # VLEN compare: python3 view_image.py <width> <height>
        compare_vlen_outputs(int(sys.argv[1]), int(sys.argv[2]))

    else:
        print("Usage:")
        print("  Single file:   python3 view_image.py <file.raw> <width> <height>")
        print("  VLEN compare:  python3 view_image.py <width> <height>")
        print("  All stages:    python3 view_image.py stages <width> <height>")