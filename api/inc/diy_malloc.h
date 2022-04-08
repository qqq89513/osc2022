#ifndef __DIY_MALLOC_H_
#define __DIY_MALLOC_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <stdint.h>

// Simple memory allocation -----------------------------------------
#define SIMPLE_MALLOC_POOL_SIZE 2048  // 2 kB
void* simple_malloc(size_t size);


// Buddy system -----------------------------------------------------

typedef struct __free_frame_node{
  int index;  // index to the frame array
  struct __free_frame_node *next;
} freeframe_node;


void alloc_page_init(uint64_t heap_start, uint64_t heap_end);
int alloc_page(int page_cnt);

#ifdef __cplusplus
}
#endif
#endif  // __DIY_MALLOC_H_
