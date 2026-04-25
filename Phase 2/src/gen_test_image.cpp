#include "image_io.hpp"
#include <cstring>
#include <cstdio>

using namespace canny;

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <pattern> <width> <height> <output.raw>\n", argv[0]);
        fprintf(stderr, "Patterns: uniform, vedge, hedge, diag, rect\n");
        return 1;
    }
    
    const char* pattern = argv[1];
    int W = atoi(argv[2]);
    int H = atoi(argv[3]);
    const char* out = argv[4];
    
    uint8_t* img = alloc_image(W, H);
    
    if (strcmp(pattern, "uniform") == 0) {
        memset(img, 128, W * H);              // constant gray
    } 
    else if (strcmp(pattern, "vedge") == 0) {
        // Vertical edge: left half black, right half white
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                img[y * W + x] = (x < W/2) ? 0 : 255;
    }
    else if (strcmp(pattern, "hedge") == 0) {
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                img[y * W + x] = (y < H/2) ? 0 : 255;
    }
    else if (strcmp(pattern, "rect") == 0) {
        memset(img, 0, W * H);
        for (int y = H/4; y < 3*H/4; y++)
            for (int x = W/4; x < 3*W/4; x++)
                img[y * W + x] = 255;
    }
    
    save_raw_image(out, img, W, H);
    free_image(img);
    printf("Wrote %s (%dx%d)\n", out, W, H);
    return 0;
}