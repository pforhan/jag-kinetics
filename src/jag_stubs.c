#include <stddef.h>

// Stubs for stdlib functions
int abs(int i) { return i < 0 ? -i : i; }

// Stubs for stdio functions
int printf(const char *format, ...) {
  (void)format;
  return 0;
}

int putchar(int c) {
  (void)c;
  return 0;
}
