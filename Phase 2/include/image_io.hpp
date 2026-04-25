#ifndef IMAGE_IO_HPP
#define IMAGE_IO_HPP

#include <cstdint>
#include <cstddef>

namespace canny {

// Allocates aligned memory and loads raw grayscale bytes from file.
// Caller must know width and height (pass as CLI args).
// Returns nullptr on failure. Caller must free() the buffer.
uint8_t* load_raw_image(const char* filename, int width, int height);

// Writes width*height bytes to disk. Returns true on success.
bool save_raw_image(const char* filename, const uint8_t* data,
                    int width, int height);

// Aligned allocation helper — 64-byte alignment helps vectorization later.
uint8_t* alloc_image(int width, int height);

// Matching free (aligned_alloc requires free, not delete).
void free_image(uint8_t* ptr);

}  // namespace canny

#endif