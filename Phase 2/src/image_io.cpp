#include "image_io.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace canny {

uint8_t* alloc_image(int width, int height) {
    size_t size = static_cast<size_t>(width) * height;
    // aligned_alloc requires size to be a multiple of alignment.
    // Round up to nearest 64-byte boundary.
    size_t aligned_size = (size + 63) & ~static_cast<size_t>(63);
    return static_cast<uint8_t*>(aligned_alloc(64, aligned_size));
}

void free_image(uint8_t* ptr) {
    free(ptr);
}

uint8_t* load_raw_image(const char* filename, int width, int height) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open %s for reading\n", filename);
        return nullptr;
    }
    
    uint8_t* buffer = alloc_image(width, height);
    if (!buffer) {
        fclose(f);
        return nullptr;
    }
    
    size_t expected = static_cast<size_t>(width) * height;
    size_t read = fread(buffer, 1, expected, f);
    fclose(f);
    
    if (read != expected) {
        fprintf(stderr, "Error: expected %zu bytes, got %zu\n", expected, read);
        free_image(buffer);
        return nullptr;
    }
    
    return buffer;
}

bool save_raw_image(const char* filename, const uint8_t* data,
                    int width, int height) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot open %s for writing\n", filename);
        return false;
    }
    
    size_t to_write = static_cast<size_t>(width) * height;
    size_t written = fwrite(data, 1, to_write, f);
    fclose(f);
    
    return written == to_write;
}

}  // namespace canny