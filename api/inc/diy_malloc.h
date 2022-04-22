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

// Considered as a header in an unallocated buddy, pointing next and previous buddy's header with the same buddy size
typedef struct buddynode{
  struct buddynode *prev; // Null for head
  struct buddynode *next; // Null for end
} buddynode;

typedef struct buddy_status{
  uint8_t used : 1;    // is the buddy is allocated, is used or not
  int64_t val : 63;  // >1 for buddy size, or FRAME_ARRAY_F, FRAME_ARRAY_X, FRAME_ARRAY_P
} buddy_status;

typedef struct __chunk_header{
  uint8_t used : 1;
  uint64_t size : 63; // has the size of whole chunk, including header and free to use block
  // unsigned long long size : 63; // has the size of whole chunk, including header and free to use block
} chunk_header; // sizeof(chunk_header) should be muliple of 8

void alloc_page_preinit(uint64_t heap_start, uint64_t heap_end);
void alloc_page_init();
int alloc_page(int page_cnt, int verbose);
int free_page(int page_index, int verbose);
void mem_reserve(uint64_t start, uint64_t end);

// Dump functions
void dump_the_frame_array();
void dupmp_frame_freelist_arr();

void *diy_malloc(size_t size);
void diy_free(void *addr);

#ifdef __cplusplus
}
#endif
#endif  // __DIY_MALLOC_H_
