# RISC-V Canny Edge Detector

## Requirements
```bash
pip3 install pillow --break-system-packages
```

## How to Add Your Own Image

### Copy from Windows to WSL
```bash
# Check your Windows username first
ls /mnt/c/Users/

# Copy image from Desktop
cp /mnt/c/Users/YOUR_USERNAME/Desktop/your_image.jpg .
```

### Convert to raw format
```bash
python3 convert_image.py your_image.jpg
```
> Any format works (jpg, png, jpeg)
> Image will be resized to 128x128 automatically

---

## Run Visual Pipeline

### Step 1 — Run the pipeline
```bash
make visual
```
This generates:
- `output_gaussian.raw` → after Gaussian blur
- `output_edges.raw`    → after Sobel edge detection

### Step 2 — View results as PNG
```bash
python3 view_image.py input.raw 128 128
python3 view_image.py output_gaussian.raw 128 128
python3 view_image.py output_edges.raw 128 128
```

### Step 3 — Open in Windows
```bash
explorer.exe .
```
You will find:
- `input.png`           → original image
- `output_gaussian.png` → after blur  
- `output_edges.png`    → edges only

---

## Run Unit Tests
```bash
make test
./runTests
```
