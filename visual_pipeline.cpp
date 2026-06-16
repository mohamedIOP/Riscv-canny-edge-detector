#include <iostream>
#include <fstream>
#include <vector>
#include "gaussian.hpp"
#include "sobel.hpp"
#include "magnitude.hpp"
#include "direction.hpp"
#include <cmath>

using namespace std;

// ── Read raw grayscale image ──────────────────
vector<uint8_t> readImage(const string& path, int width, int height) {
    ifstream file(path, ios::binary);
    vector<uint8_t> data(width * height);
    file.read(reinterpret_cast<char*>(data.data()), width * height);
    return data;
}

// ── Write raw grayscale image ─────────────────
void writeImage(const string& path, const vector<uint8_t>& data) {
    ofstream file(path, ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cout << "Usage: " << argv[0] << " <width> <height> <input.raw>" << endl;
        return 1;
    }

    int width      = atoi(argv[1]);
    int height     = atoi(argv[2]);
    string input_path = argv[3];

    // Step 1: Read input image
    vector<uint8_t> input = readImage(input_path, width, height);

    // Step 2: Gaussian Blur
    vector<uint8_t> blurred(width * height);
    canny::gaussian_blur_5x5(input.data(), blurred.data(), width, height);
    writeImage("./Output_Images/output_gaussian.raw", blurred);
    cout << "Gaussian blur done → output_gaussian.raw" << endl;

    // Step 3: Sobel Gradient
    vector<int16_t> Gx(width * height);
    vector<int16_t> Gy(width * height);
    canny::sobel_gradients(blurred.data(), Gx.data(), Gy.data(), width, height);

    // Step 4: Magnitude L2 (using magnitude.cpp, not inline)
    vector<uint8_t> mag_l2(width * height);
    // Just replace the broken line with:
    canny::magnitude_l1(Gx.data(), Gy.data(), mag_l2.data(), width, height);
    writeImage("./Output_Images/output_edges.raw", mag_l2);
    cout << "Magnitude L2 done → output_edges.raw" << endl;

    // Step 5: Magnitude L1
    vector<uint8_t> mag_l1(width * height);
    canny::magnitude_l1(Gx.data(), Gy.data(), mag_l1.data(), width, height);
    writeImage("./Output_Images/output_magnitude.raw", mag_l1);
    cout << "Magnitude L1 done → output_magnitude.raw" << endl;

    // Step 6: Gradient Direction
    vector<uint8_t> dir_out(width * height);
    canny::gradient_direction(Gx.data(), Gy.data(), dir_out.data(), width, height);
    writeImage("./Output_Images/output_direction.raw", dir_out);
    cout << "Direction done → output_direction.raw" << endl;

    return 0;
}
