#include <stdlib.h>
#include <stdio.h>

#ifndef track_h
#define track_h
extern void* t_malloc(size_t size);
extern void* t_calloc(size_t count, size_t size);
extern void* t_realloc(void * old_ptr, size_t size);
extern void* t_recalloc(void * old_ptr, size_t new_count, size_t size);
extern void t_free(void* ptr);
extern void t_isfree(void* ptr);
#endif
