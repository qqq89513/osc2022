#ifndef __DIY_MALLOC_H_
#define __DIY_MALLOC_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <stdint.h>

// Simple memory allocation -----------------------------------------
void* simple_malloc(size_t size);


// Buddy system -----------------------------------------------------

typedef struct __free_frame_node{
  int index;  // index to the frame array
  struct __free_frame_node *next;
} freeframe_node;

typedef struct __memblock_node{
  uint64_t start_addr;
  uint64_t end_addr;
  struct __memblock_node *next;
} memblock_node;

typedef struct __chunk_header{
  uint8_t used : 1;
  uint64_t size : 63; // has the size of whole chunk, including header and free to use block
  // unsigned long long size : 63; // has the size of whole chunk, including header and free to use block
} chunk_header;

void alloc_page_init(uint64_t heap_start, uint64_t heap_end);
int alloc_page(int page_cnt, int verbose);
int free_page(int page_index, int verbose);
void mem_reserve(uint64_t start, uint64_t end);

// Dump functions
void dump_the_frame_array();
void dupmp_frame_freelist_arr();

void *diy_malloc(size_t size);

#ifdef __cplusplus
}
#endif
#endif  // __DIY_MALLOC_H_
