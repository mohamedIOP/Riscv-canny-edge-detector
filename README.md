# RISC-V Canny Edge Detector

## Visual Pipeline — How to Run

### Requirements
```bash
pip3 install pillow --break-system-packages
```

### Step 1 — Convert your image to raw
```bash
python3 convert_image.py your_image.jpg
```

### Step 2 — Run the pipeline
```bash
make visual
```
This will generate:
- `output_gaussian.raw` → image after Gaussian blur
- `output_edges.raw`    → image after Sobel edge detection

### Step 3 — View the results
```bash
python3 view_image.py input.raw 128 128
python3 view_image.py output_gaussian.raw 128 128
python3 view_image.py output_edges.raw 128 128
```

### Step 4 — Open PNG results
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
