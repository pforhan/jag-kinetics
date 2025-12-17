#ifndef _STRING_H_
#define _STRING_H_

#include <stddef.h>

void* memcpy(void* dest, const void* src, size_t n);
void bcopy(const void *src, void *dest, size_t n);

#endif /* _STRING_H_ */
