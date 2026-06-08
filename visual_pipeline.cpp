#include <iostream>
#include <fstream>
#include <vector>
#include "gaussian.hpp"
#include "sobel.hpp"
#include "magnitude.hpp"
#include "direction.hpp"
#include <cmath>

using namespace std;

vector<uint8_t> readImage(const string& path, int width, int height) {
    ifstream file(path, ios::binary);
    vector<uint8_t> data(width * height);
    file.read(reinterpret_cast<char*>(data.data()), width * height);
    return data;
}

void writeImage(const string& path, const vector<uint8_t>& data) {
    ofstream file(path, ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cout << "Usage: " << argv[0] << " <width> <height> <input.raw>" << endl;
        return 1;
    }

    int width     = atoi(argv[1]);
    int height    = atoi(argv[2]);
    string input_path = argv[3];
    int total     = width * height;

    // ── Step 1: Read input ────────────────────
    vector<uint8_t> input = readImage(input_path, width, height);
    cout << "Loaded: " << input_path << " (" << width << "x" << height << ")" << endl;

    // ── Step 2: Gaussian Blur ─────────────────
    vector<uint8_t> blurred(total);
    canny::gaussian_blur_5x5(input.data(), blurred.data(), width, height);
    writeImage("./Output_Images/output_gaussian.raw", blurred);
    cout << "Gaussian blur done → output_gaussian.raw" << endl;

    // ── Step 3: Sobel Gradients ───────────────
    vector<int16_t> Gx(total);
    vector<int16_t> Gy(total);
    canny::sobel_gradients(blurred.data(), Gx.data(), Gy.data(), width, height);

    // Save Gx and Gy as visible uint8 (clamp abs to 255)
    vector<uint8_t> gx_vis(total), gy_vis(total);
    for (int i = 0; i < total; i++) {
        int ax = Gx[i] < 0 ? -Gx[i] : Gx[i];
        int ay = Gy[i] < 0 ? -Gy[i] : Gy[i];
        gx_vis[i] = (uint8_t)(ax > 255 ? 255 : ax);
        gy_vis[i] = (uint8_t)(ay > 255 ? 255 : ay);
    }
    writeImage("./Output_Images/output_sobel_gx.raw", gx_vis);
    writeImage("./Output_Images/output_sobel_gy.raw", gy_vis);
    cout << "Sobel gradients done → output_sobel_gx.raw, output_sobel_gy.raw" << endl;

    // ── Step 4: Magnitude L2 ─────────────────
    vector<uint8_t> mag_l2(total);
    canny::magnitude_l2(Gx.data(), Gy.data(), mag_l2.data(), width, height);
    writeImage("./Output_Images/output_128.raw", mag_l2);
    cout << "Magnitude L2 done → output_128.raw" << endl;

    // ── Step 5: Magnitude L1 ─────────────────
    vector<uint8_t> mag_l1(total);
    canny::magnitude_l1(Gx.data(), Gy.data(), mag_l1.data(), width, height);
    writeImage("./Output_Images/output_magnitude_l1.raw", mag_l1);
    cout << "Magnitude L1 done → output_magnitude_l1.raw" << endl;

    // ── Step 6: Gradient Direction ────────────
    vector<uint8_t> dir_out(total);
    canny::gradient_direction(Gx.data(), Gy.data(), dir_out.data(), width, height);
    // Scale 0-3 → 0/85/170/255 for visibility
    for (int i = 0; i < total; i++) dir_out[i] *= 85;
    writeImage("./Output_Images/output_direction.raw", dir_out);
    cout << "Direction done → output_direction.raw" << endl;

    return 0;
}