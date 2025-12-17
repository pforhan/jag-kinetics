#include <string.h>

void* memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void bcopy(const void *src, void *dest, size_t n) {
    memcpy(dest, src, n);
}
