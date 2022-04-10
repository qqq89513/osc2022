
#include "diy_malloc.h"
#include <stddef.h>
#include "uart.h"

// Simple memory allocation -----------------------------------------
extern char __simple_malloc_start;
extern char __simple_malloc_end;
void* simple_malloc(size_t size){
  static char *ptr = &__simple_malloc_start;
  char *temp = ptr;
  ptr += size;
  if(ptr > &__simple_malloc_end){
    uart_printf("Error, not enough of simple allocator size, in simple_malloc(). size=%ld\r\n", size);
    uart_printf("__simple_malloc_start=%p, __simple_malloc_end=%p, ptr=%p\r\n", &__simple_malloc_start, &__simple_malloc_end, ptr);
    uart_printf("simple malloc usage = %lu/%lu\r\n", (uint64_t)(ptr-&__simple_malloc_start), (uint64_t)(&__simple_malloc_end-&__simple_malloc_start));
    return NULL;
  }
  return (void*) temp;
}


// Buddy system (page allocation) -----------------------------------
// Ref: https://oscapstone.github.io/labs/overview.html, https://grasslab.github.io/NYCU_Operating_System_Capstone/labs/lab3.html
#define ITEM_COUNT(arr) (sizeof(arr) / sizeof(*arr))
#define PAGE_SIZE 4096 // 4kB
#define GET_PAGE_NUM(addr)        ( ((addr)-heap_start_addr)/PAGE_SIZE )
#define GET_PAGE_ADDR(page_num)   ( heap_start_addr+(page_num)*PAGE_SIZE )

#define MAX_CONTI_ALLOCATION_EXPO 16 // i.g. =6 means max allocation size is 4kB * 2^6
#define FRAME_ARRAY_F -1            // The idx’th frame is free, but it belongs to a larger contiguous memory block. Hence, buddy system doesn’t directly allocate it.
#define FRAME_ARRAY_X -2            // The idx’th frame is already allocated, hence not allocatable.
#define FRAME_ARRAY_P -3            // The idx’th frame is preserved, not allocatable
static int *the_frame_array;        // Has the size of (heap_size/PAGE_SIZE), i.e., total_pages
static size_t total_pages = 0;      // = heal_size / PAGE_SIZE
static uint64_t heap_start_addr = 0;// start address of heap

static int *malloc_page_usage;      // used for malloc, malloc_page_usage[i] == k means that k bytes in page #i are allocated through malloc

// Preserved memory block linked list
memblock_node *preserved_memblocks_head = NULL;

// frame_freelist_arr[i] points to the head of the linked list of free 4kB*(2^i) pages
freeframe_node *frame_freelist_arr[MAX_CONTI_ALLOCATION_EXPO + 1] = {NULL};

// freeframe_node fifo, since
static freeframe_node **freeframe_node_fifo;  // +1 to ease edge condition
static int fifo_in = 0, fifo_out = 0;         // _in == _out: empty
static int fifo_size = 0;
// Return a node from freeframe_node_fifo
static freeframe_node *malloc_freeframe_node(){
  freeframe_node *temp;
  if(fifo_in == fifo_out){
    uart_printf("Exception, no enough space, fifo empty, fifo_in=fifo_out=%d, in malloc_freeframe_node()\r\n", fifo_in);
    return NULL;
  }
  temp = freeframe_node_fifo[fifo_out++];
  if(fifo_out >= fifo_size)
    fifo_out = 0;
  return temp;
}
// Insert a node into freeframe_node_fifo
static void free_freeframe_node(freeframe_node *node){
  freeframe_node_fifo[fifo_in++] = node;
  if(fifo_in >= fifo_size)
    fifo_in = 0;
}

static int log2_floor(uint64_t x){
  int expo = 0;
  while(x != 0x01 && x != 0x00){
    x = x >> 1;
    expo++;
  }
  if(x == 0x01)
    return expo;
  else // x == 0x00
    return -1;
}

// Dump funcs
void dump_the_frame_array(){
  uart_printf("the_frame_array[] = ");
  for(int i=0; i<total_pages; i++)  uart_printf("%2d ", the_frame_array[i]);
  uart_printf("\r\n");
}
void dupmp_frame_freelist_arr(){
  uart_printf("frame_freelist_arr[] = \r\n");
  for(int i=MAX_CONTI_ALLOCATION_EXPO; i>=0; i--){
    freeframe_node *node = frame_freelist_arr[i];
    uart_printf("\t4kB *%3d: ", 1 << i);
    while(node != NULL){
      uart_printf("%2d-->", node->index);
      node = node->next;
    }
    uart_printf("NULL\r\n");
  }
}
static void dump_chunk(){
  chunk_header *header = NULL;
  // Dump the pages' chunk if they are allocated by diy_malloc()
  for(int i=0; i<total_pages; i++){
    header = (chunk_header*) GET_PAGE_ADDR(i); // header->size is the size of allocated block
    if(malloc_page_usage[i] >= 0){
      uart_printf("Page %d, usage = %d bytes\r\n", i, malloc_page_usage[i]);
      // Traverse all chunks in page #i
      while((uint64_t)header < GET_PAGE_ADDR(i+1)){ // limit at current page
        uart_printf("\tusable addr=%p, used=%d, size=%lu\r\n", &header[1], header->used, (uint64_t)header->size);
        header = (chunk_header*) ( (uint64_t)header + header->size );
      }
    }
  }
  uart_printf("\r\n");
}

int alloc_page(int page_cnt, int verbose){
  int curr_page_cnt = 0;
  int page_allocated = -1;
  freeframe_node *temp_node = NULL;
  freeframe_node **head = NULL;
  // Return if page_cnt is too big
  if(page_cnt > (1 << MAX_CONTI_ALLOCATION_EXPO)){
    uart_printf("Error, cannot allocate contiguous page_cnt=%d, maximum contiguous page=%d\r\n", page_cnt, (1 << MAX_CONTI_ALLOCATION_EXPO));
    return -1;
  }
  // Roud up page_cnt to 2, 4, 8, 16...
  for(int i=0; i<=MAX_CONTI_ALLOCATION_EXPO; i++){
    if(page_cnt <= (1 << i)){
      page_cnt = 1 << i;
      break;
    }
  }

  // Search for free block that fits page_cnt in frame_freelist_arr
  for(int i=0; i<=MAX_CONTI_ALLOCATION_EXPO; i++){
    head = &frame_freelist_arr[i];  // head points to the linked list of sets of same # free pages
    curr_page_cnt = 1 << i;         // # of free pages now head points to
    if(page_cnt <= curr_page_cnt && (*head) != NULL){
      break;
    }
  }

  // Free block found
  if(page_cnt <= curr_page_cnt && (*head) != NULL) {
    page_allocated = (*head)->index;   // Required page#
    // Remove the block found from (*head)
    temp_node = (*head);
    (*head) = (*head)->next;
    free_freeframe_node(temp_node);

    // Mark part of the free block allocated in the_frame_array
    the_frame_array[page_allocated] = page_cnt;
    const int end_idx = page_allocated + page_cnt;
    const int start_idx = page_allocated + 1;
    for(int k=start_idx; k<end_idx; k++)  the_frame_array[k] = FRAME_ARRAY_X;

    // Re-assign the rest of the free block to new buddies
    if(page_cnt < curr_page_cnt) {
      int pages_remain = curr_page_cnt - page_cnt; // pages count remained in the buddy
      int nfblock_size = page_cnt;  // new frame block size, unit=page
      int nfblock_start = end_idx;  // new frame block starting index
      while(pages_remain > 0){
        int fflist_arr_idx = log2_floor(nfblock_size);
        // Insert 1 node into the linked list
        head = &frame_freelist_arr[fflist_arr_idx];  // head points to the linked list of sets of same # free pages
        temp_node = malloc_freeframe_node();
        temp_node->index = nfblock_start;
        temp_node->next = (*head);
        (*head) = temp_node;

        // Mark buddy begining
        the_frame_array[nfblock_start] = nfblock_size;
        nfblock_start += nfblock_size;

        // Calculate the remaining un-reassigned pages
        pages_remain = pages_remain - nfblock_size;
        nfblock_size = nfblock_size << 1;
      }
    }
  }
  // Free block not found
  else {
    uart_printf("Error, not enough of pages. Required %d contiguous pages\r\n", page_cnt);
  }
  if(verbose){
    if(total_pages < 200) dump_the_frame_array();
    dupmp_frame_freelist_arr();
  }
  return page_allocated;
}

/** Merge frame_freelist_arr[fflists_idx] and it's buddy into frame_freelist_arr[fflists_idx+1]. 
 * So before calling this function, a free node should be inserted to the head (frame_freelist_arr[fflists_idx]). Then 
 * call this function. This function search head's buddy. If buddy found, merge them into a larger block, i.e., insert 
 * it(2 nodes become 1) into the head of the linked list of larger block size. Finally return fflists_idx+1.
 * @param fflists_idx: The index of frame_freelist_arr, indicating which block size to merge
 * @return fflists_idx+1 if buddy found. 1 << (fflists_idx+1) is the block size that merged into. 
 *  Return -1 if no buddy to merge.
 *  Return -2 if exception.
*/
static int free_page_merge(int fflists_idx){
  // Return if no head in the linked list
  if(frame_freelist_arr[fflists_idx] == NULL){
    uart_printf("Exception, frame_freelist_arr[%d] is NULL. @line=%d, file:%s\r\n", fflists_idx, __LINE__, __FILE__);
    return -2;
  }
  
  int freeing_page = frame_freelist_arr[fflists_idx]->index;
  int block_size = the_frame_array[freeing_page];   // How many contiguous pages to free
  int buddy_LR = (freeing_page / block_size % 2);   // 0 for left(lower) buddy, 1 for right(higher) buddy
  // Compute the index of the neighbor to merge
  int buddy_page = freeing_page + (buddy_LR ? -block_size : +block_size);
  if(buddy_LR != 0 && buddy_LR != 1){  // Exception
    uart_printf("Exception, shouldn't get here. buddy_LR=%d, @line=%d, file:%s\r\n", buddy_LR, __LINE__, __FILE__);
    return -2;
  }
  
  // Merge head and it's buddy in the linked list
  freeframe_node **head = &frame_freelist_arr[fflists_idx];
  freeframe_node *node = *head;
  freeframe_node *node_prev = NULL;
  fflists_idx++;
  if(fflists_idx < ITEM_COUNT(frame_freelist_arr)){  // Check if larger block is acceptable
    // Search for head's buddy to merge
    while(node != NULL){
      // Merge-able neighbor found, move the node to the linked list of a greater block size
      if(node->index == buddy_page){
        // Remove the node from the linked list
        if(node_prev != NULL){  // Point previous node's next to current node's next, and remove and free head
          node_prev->next = node->next;
          // Remove and free head
          freeframe_node *node_temp = *head;
          *head = (*head)->next;
          free_freeframe_node(node_temp);
        }
        else{                   // No previous node, that is, head is the buddy
          uart_printf("Exception, should not get here, in free_page_merge(), @line=%d, file:%s\r\n", __LINE__, __FILE__);
        }
        
        // Insert node to the linked list of a greater block size
        node->index = (freeing_page < buddy_page) ? freeing_page : buddy_page; // = min(freeing_page, buddy_page), buddy's head is the smaller one
        node->next = frame_freelist_arr[fflists_idx];
        frame_freelist_arr[fflists_idx] = node;

        // Update the frame array
        block_size = block_size << 1;
        const int start_idx = node->index + 1;
        const int end_idx = node->index + block_size;
        the_frame_array[node->index] = block_size;
        for(int i=start_idx; i < end_idx; i++) the_frame_array[i] = FRAME_ARRAY_F;
        
        // uart_printf("Merging %d and %d into %d. merged block_size=%d, start_idx=%d, end_idx=%d\r\n", 
        //   buddy_page, freeing_page, node->index, block_size, start_idx, end_idx);

        return fflists_idx;
      }
      // Advance node
      node_prev = node;
      node = node->next;
    }
  }

  // We are here because either:
  //  1. Cannot merge into a bigger block because it's already biggest. OR
  //  2. No buddy block found in the linked list
  return -1;
}

/** Free a page allocated from alloc_page()
 * @return 0 on success. -1 on error.
*/
int free_page(int page_index, int verbose){
  int block_size = the_frame_array[page_index]; // How many contiguous pages to free
  int fflists_idx = log2_floor(block_size);

  // Check if ok to free, return -1 if not ok to free
  if(block_size < 0){
    uart_printf("Error, freeing wrong page. the_frame_array[%d]=%d, the page belongs to a block\r\n", page_index, block_size);
    return -1;
  }
  else if(block_size == 1){
    freeframe_node *head = frame_freelist_arr[fflists_idx];
    // Search for page_index in the linked list
    while(head != NULL){
      if(head->index == page_index){
        uart_printf("Error, freeing wrong page. Page %d is already in free lists. block_size=%d\r\n", page_index, block_size);
        return -1;
      }
      head = head->next;
    }
  }
  else{ // block_size > 1
    if(the_frame_array[page_index + 1] != FRAME_ARRAY_X){
      uart_printf("Error, freeing wrong page. Page %d is already in free lists. block_size=%d\r\n", page_index, block_size);
      return -1;
    }
  }

  // Insert a free node in to frame_freelist_arr[fflists_idx]
  freeframe_node *node = malloc_freeframe_node();
  node->index = page_index;
  node->next = frame_freelist_arr[fflists_idx];
  frame_freelist_arr[fflists_idx] = node;
  // Update the frame array
  const int start_idx = page_index + 1;
  const int end_idx = page_index + block_size;
  for(int i=start_idx; i < end_idx; i++) the_frame_array[i] = FRAME_ARRAY_F;
  
  // Merge iterativly
  int fflists_merge = fflists_idx;
  while(fflists_merge >= 0){
    if(verbose) dupmp_frame_freelist_arr();
    fflists_merge = free_page_merge(fflists_merge);
  }
  if(verbose){
    if(total_pages < 200) dump_the_frame_array();
    dupmp_frame_freelist_arr();
  }
  return 0;
}

void alloc_page_init(uint64_t heap_start, uint64_t heap_end){
  freeframe_node *freeframe_node_pool = NULL;
  const size_t heap_size = heap_end - heap_start;
  total_pages = (heap_size / PAGE_SIZE);
  heap_start_addr = heap_start;
  uart_printf("total_pages=%ld, heap_size=%ld bytes\r\n", total_pages, heap_size);

  // Init: allocate space
  malloc_page_usage = (int*) simple_malloc(sizeof(int) * total_pages);
  the_frame_array = (int*) simple_malloc(sizeof(int) * total_pages);
  freeframe_node_pool = (freeframe_node  *) simple_malloc(sizeof(freeframe_node) * total_pages);
  freeframe_node_fifo = (freeframe_node **) simple_malloc(sizeof(freeframe_node) * (total_pages + 1)); // +1 to ease edge condition
  fifo_size = total_pages + 1;
  for(int i=0; i<total_pages; i++){
    malloc_page_usage[i] = -1;
    free_freeframe_node(&freeframe_node_pool[i]);
  }

  // Buddy system init
  int left_pages = total_pages;
  int buddy_page_cnt = 1 << MAX_CONTI_ALLOCATION_EXPO;
  int i = MAX_CONTI_ALLOCATION_EXPO;
  int frame_arr_idx = 0;
  while(left_pages > 0 && buddy_page_cnt != 0){
    if(left_pages >= buddy_page_cnt){
      
      // Insert a freeframe_node into the linked list frame_freelist_arr[i]
      freeframe_node *node = malloc_freeframe_node();
      node->next = frame_freelist_arr[i];
      node->index = frame_arr_idx;
      frame_freelist_arr[i] = node;
      
      // Fill the frame array
      the_frame_array[frame_arr_idx] = buddy_page_cnt;
      for(int k=frame_arr_idx+1; k<(frame_arr_idx+buddy_page_cnt); k++){      // Fill value <F> for buddy of frame_arr_idx'th page
        the_frame_array[k] = FRAME_ARRAY_F;
      }
      frame_arr_idx += buddy_page_cnt;

      // Substract left pages
      left_pages -= buddy_page_cnt;
    }
    else{
      buddy_page_cnt = buddy_page_cnt >> 1;
      i--;
    }
  }

  // Preserve pages
  //  1. allocate all pages, 1 page at a time
  //  2. mark preserved pages in the the_frame_array[]
  //  3. free the pages that's not mark as preserved
  memblock_node *node = preserved_memblocks_head;
  if(node != NULL){
    // Allocate all pages
    for(int i=0; i<total_pages; i++)  alloc_page(1, 0);
    // Mark preserved pages
    while(node != NULL){
      const int start_page = GET_PAGE_NUM(node->start_addr);
      const int end_page = GET_PAGE_NUM(node->end_addr);
      uart_printf("prevserved from page %d to %d\r\n", start_page, end_page);
      if(start_page >= 0 && end_page <= total_pages)
        for(int i=start_page; i<=end_page; i++)
          the_frame_array[i] = FRAME_ARRAY_P;
      node = node->next;
    }
    // Free not preserved pages
    for(int i=0; i<total_pages; i++){
    if(the_frame_array[i] != FRAME_ARRAY_P)
      free_page(i, 0);
  }
  }
}

void mem_reserve(uint64_t start, uint64_t end){
  // Insert a node to the head of preserved memblocks linked list
  memblock_node *node = (memblock_node*) simple_malloc(sizeof(memblock_node));
  node->start_addr = start;
  node->end_addr = end;
  node->next = preserved_memblocks_head;
  preserved_memblocks_head = node;
}

// diy_malloc, diy_free for small memory ---------------------------
void *diy_malloc(size_t desire_size){
  // TODO: Handle allocation for desire_size > (PAGE_SIZE-sizeof(chunk_header))
  // TODO: Handle the case that unused chunks available, but new page needs to be allocated
  //       Then the unused chunks is never reachable untill the page they belong to is freed.
  
  static int curr_page = -1; // current page allocation from

  // Allocating a new page
  if(curr_page == -1){
    curr_page = alloc_page(1, 0);
    if(curr_page < 0){ 
      uart_printf("In diy_malloc(), failed to allocate a page.\r\n");
      return NULL;
    }
    uart_printf("New page %d allocated by diy_malloc()\r\n", curr_page);
    malloc_page_usage[curr_page] = 0;
    size_t *page = (size_t *) GET_PAGE_ADDR(curr_page);
    // Zero out the content of the page that just allocated
    for(int i=0; i<PAGE_SIZE/sizeof(size_t); i++)
      page[i] = 0;
    chunk_header *header = (chunk_header*) page;
    header->size = PAGE_SIZE;
    header->used = 0;
    dump_chunk();
  }

  // Current page is freed in diy_free(), so allocate a new one
  if(malloc_page_usage[curr_page] == -1){
    curr_page = -1;
    return diy_malloc(desire_size);
  }


  /** An allocated block(chunk), allocated by diy_malloc, looks like this
   * | --8 byte header-- | ----free to use range---- |
   *                     ^
   *                     |--> here is the address that diy_malloc() return
   * 8 byte header: header.size indicates the length of the entire chunk, header.used indicates if the chunk is allocated
   * free to use range: caller of diy_malloc() can use this range of memory, has lenght of (head.size - sizeof(header))
  */

  // First fit: find the first fittable hole
  chunk_header *header = (chunk_header*) GET_PAGE_ADDR(curr_page); // header->size is the size of allocated block
  void *ret = NULL;
  desire_size += sizeof(chunk_header);
  // Skip chunks that are used(allocated) or not big enough
  while((header->used == 1 || header->size < desire_size) && (uint64_t)header < GET_PAGE_ADDR(curr_page+1)){
    header = (chunk_header*) ( (uint64_t)header + header->size );
  }

  // Free chunk found
  if((uint64_t)header < GET_PAGE_ADDR(curr_page+1)){
    const int64_t left_over = header->size - desire_size;
    // Chunk with exact same size found
    if(left_over == 0){
      header->size = desire_size;
      header->used = 1;
      malloc_page_usage[curr_page] += header->size;
      ret = &header[1]; // return the address right after the header
    }
    // Chunk with larger size that can left a usable hole
    else if(left_over >= (sizeof(chunk_header) + 1)){
      header->size = desire_size;
      header->used = 1;
      malloc_page_usage[curr_page] += header->size;
      ret = &header[1];
      // Cut the original chunk, setup the leftover as a new chunk
      header = (chunk_header*) ( (uint64_t)header + desire_size );
      header->size = left_over;
      header->used = 0;
    }
    // Chunk with larger size but left a unusabl hole
    else{  // left_over < (sizeof(chunk_header) + 1)
      header->size = header->size;
      header->used = 1;
      malloc_page_usage[curr_page] += header->size;
      ret = &header[1];
    }

    dump_chunk();
    return ret;
  }

  // No free chunks, allocate a new page
  curr_page = -1;
  return diy_malloc(desire_size - sizeof(chunk_header)); // substract sizeof(chunk_header) because it was added for fir fit operation
}

void diy_free(void *addr){
  chunk_header *header = addr - sizeof(chunk_header);
  int page_num = GET_PAGE_NUM((uint64_t)addr);

  // Check if freeing wron address
  if(header->used == 0 || header->size < (sizeof(chunk_header)+1)){
    uart_printf("Error, failed to free addr=%p, .used=%d, .size=%lu\r\n", addr, header->used, (uint64_t)header->size);
    return;
  }

  // Mark this chunk unused and substract usage
  header->used = 0;
  malloc_page_usage[page_num] -= header->size;

  // Free the page if all chunks are unsued
  if(malloc_page_usage[page_num] == 0){
    uart_printf("In diy_free(), all chunks in page %d is unused, freeing this page.\r\n", page_num);
    malloc_page_usage[page_num] = -1;
    free_page(page_num, 0);
  }
  // Merge neighbor unsed chunks
  else if(malloc_page_usage[page_num] >= 0){
    chunk_header *neighbor_chunk = NULL;
    chunk_header *traverse_chunk = NULL;

    // Merge forward
    neighbor_chunk = (chunk_header*) ( (uint64_t)header + header->size ); // next chunk
    if((uint64_t)neighbor_chunk < GET_PAGE_ADDR(page_num+1) && neighbor_chunk->used == 0){
      header->size += neighbor_chunk->size;
      neighbor_chunk->size = 0;
    }

    // Merge backward
    // Traverse all chunks in page #page_num to get previous chunk
    traverse_chunk = (chunk_header*) GET_PAGE_ADDR(page_num);
    neighbor_chunk = traverse_chunk;  // chunk before traverse_chunk
    while((uint64_t)traverse_chunk < GET_PAGE_ADDR(page_num+1)){ // limit at current page
      // Next chunk
      neighbor_chunk = traverse_chunk;
      traverse_chunk = (chunk_header*) ( (uint64_t)traverse_chunk + traverse_chunk->size );
      // Previous chunk found, 
      // i.e., traverse_chunk is header, than header's previous chunk will be neighbor_chunk
      if(traverse_chunk == header){
        if(neighbor_chunk->used == 0){
          neighbor_chunk->size += header->size;
          header->size = 0;
        }
        break;
      }
    }
  }
  // Error
  else if(malloc_page_usage[page_num] < 0){
    uart_printf("Error, malloc_page_usage[%d]=%d, which should not be smaller than 0.\r\n", page_num, malloc_page_usage[page_num]);
    return;
  }

  dump_chunk();
}

