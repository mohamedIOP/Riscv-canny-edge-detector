#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdint>
using namespace std;
// Function to load a raw grayscale image into RVV-aligned memory
uint8_t* load_raw_image(const char* filename, size_t width, size_t height) {
    size_t total_pixels = width * height;
    uint8_t* image_data = static_cast<uint8_t*>(aligned_alloc(64, total_pixels));
    
    if (image_data == nullptr) {
        cerr << "Memory allocation failed!" << endl;
        return nullptr;
    }

    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Failed to open image file: " << filename << endl;
        free(image_data);
        return nullptr;
    }

    file.read(reinterpret_cast<char*>(image_data), total_pixels);
    file.close();
    cout << "Successfully loaded " << total_pixels << " pixels." << endl;
    return image_data;
}

// Function to save processed image back to disk
void save_raw_image(const char* filename, uint8_t* image_data, size_t width, size_t height) {
    size_t total_pixels = width * height;
    ofstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Failed to open file for writing: " << filename << endl;
        return;
    }

    file.write(reinterpret_cast<const char*>(image_data), total_pixels);
    file.close();
    cout << "Successfully saved to " << filename << endl;
}

int main() {
    // For testing, we'll assume a small 256x256 image
    size_t width = 256;
    size_t height = 256;

    cout << "--- Starting I/O Test ---" << endl;
    
    // 1. Load the image
    // Update this path to match your exact home directory path
    uint8_t* image = load_raw_image("input.raw", width, height);
    if (image == nullptr) return -1;

    // 2. Immediately save it to see if our pass-through works
    save_raw_image("output.raw", image, width, height);

    // 3. Clean up memory
    free(image);
    
    cout << "--- Test Complete ---" << endl;
    return 0;
}