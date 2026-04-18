#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <riscv_vector.h>

using namespace std;

// --- BARE METAL QEMU SYSCALL BYPASS ---
// Bypasses the broken bare-metal C library by speaking directly to QEMU Linux.
#define AT_FDCWD -100
#define LINUX_O_RDONLY 00
#define LINUX_O_WRONLY 01
#define LINUX_O_CREAT  0100
#define LINUX_O_TRUNC  01000

long linux_openat(const char *pathname, int flags, int mode) {
    register long a0 asm("a0") = AT_FDCWD;
    register long a1 asm("a1") = (long)pathname;
    register long a2 asm("a2") = flags;
    register long a3 asm("a3") = mode;
    register long a7 asm("a7") = 56; // SYS_openat
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
    return a0;
}

long linux_read(long fd, void *buf, size_t count) {
    register long a0 asm("a0") = fd;
    register long a1 asm("a1") = (long)buf;
    register long a2 asm("a2") = count;
    register long a7 asm("a7") = 63; // SYS_read
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}

long linux_write(long fd, const void *buf, size_t count) {
    register long a0 asm("a0") = fd;
    register long a1 asm("a1") = (long)buf;
    register long a2 asm("a2") = count;
    register long a7 asm("a7") = 64; // SYS_write
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}

long linux_close(long fd) {
    register long a0 asm("a0") = fd;
    register long a7 asm("a7") = 57; // SYS_close
    asm volatile ("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

// --- IMAGE I/O FUNCTIONS ---
uint8_t* load_raw_image(const char* filename, size_t width, size_t height) {
    long fd = linux_openat(filename, LINUX_O_RDONLY, 0);
    if (fd < 0) {
        printf("CRITICAL ERROR: Failed to open %s (Syscall returned %ld)\n", filename, fd);
        return nullptr;
    }

    size_t size = width * height;
    uint8_t* data = (uint8_t*)aligned_alloc(64, size);
    if (!data) {
        printf("ERROR: Memory allocation failed!\n");
        linux_close(fd);
        return nullptr;
    }

    linux_read(fd, data, size);
    linux_close(fd);
    return data;
}

void save_raw_image(const char* filename, uint8_t* data, size_t width, size_t height) {
    long fd = linux_openat(filename, LINUX_O_WRONLY | LINUX_O_CREAT | LINUX_O_TRUNC, 0666);
    if (fd >= 0) {
        linux_write(fd, data, width * height);
        linux_close(fd);
        printf("-> Successfully saved actual file to %s\n", filename);
    } else {
        printf("CRITICAL ERROR: Failed to save %s\n", filename);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        printf("Usage: %s <input_file> <width> <height> <output_file>\n", argv[0]);
        return 0;
    }

    const char* input_filename = argv[1];
    size_t width = atoi(argv[2]);
    size_t height = atoi(argv[3]);
    const char* output_filename = argv[4];

    printf("--- RISC-V Canny Test Start ---\n");

    uint8_t* image = load_raw_image(input_filename, width, height);
    if (!image) return 0; 

    printf("-> Successfully loaded actual file: %s\n", input_filename);

    // --- Vector Initialization ---
    size_t vl = __riscv_vsetvl_e8m1(width);
    printf("-> SUCCESS! Vector Length (VLEN) for e8m1 is: %zu\n", vl);

    save_raw_image(output_filename, image, width, height);

    free(image);
    printf("--- RISC-V Canny Test End ---\n");
    return 0;
}