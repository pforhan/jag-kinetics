#ifndef JAGUAR

#include <stdio.h>

int printf(const char *format, ...) {
    (void)format;
    return 0;
}

int putchar(int character) {
    (void)character;
    return 0;
}

#endif /* JAGUAR */
