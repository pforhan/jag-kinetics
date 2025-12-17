#include <stdint.h>

// 64-bit multiplication
int64_t __muldi3(int64_t a, int64_t b) {
    return a * b;
}

// 64-bit division
int64_t __divdi3(int64_t a, int64_t b) {
    if (b == 0) {
        // Division by zero error
        return 0;
    }
    return a / b;
}
