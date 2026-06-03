#ifndef __BASIC_ALLOCATOR_H__
#define __BASIC_ALLOCATOR_H__

#include <sbi/sbi_types.h>

void basic_allocator_init();
void *basic_malloc(size_t size);
void *basic_calloc(size_t num, size_t size);
void *basic_realloc(void *ptr, size_t new_size);
void basic_free(void *ptr);

#endif
