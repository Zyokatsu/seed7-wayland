#ifndef INCLUDE_SHARED_MEMORY_H
#define INCLUDE_SHARED_MEMORY_H
#include <stddef.h>

void randname (char *buf);
int create_shm_file (void);
int allocate_shm_file (size_t size);
#endif