
#include "diy_malloc.h"
#include <stddef.h>
#include "uart.h"

// Simple memory allocation -----------------------------------------
extern char __simple_malloc_start;
extern char __simple_malloc_end_no_paging;
static char *__simple_malloc_end = 0x00;
void* simple_malloc(size_t size){
  static char *ptr = &__simple_malloc_start;
  char *temp = ptr;
  ptr += size;
  if(__simple_malloc_end == 0){
    uart_printf("Simple malloc init, no paging, ");
    uart_printf("__simple_malloc_start=%p, __simple_malloc_end_no_paging=%p, size=%lu\r\n",
      &__simple_malloc_start, &__simple_malloc_end_no_paging, (uint64_t)&__simple_malloc_end_no_paging - (uint64_t)&__simple_malloc_start);

    __simple_malloc_end = &__simple_malloc_end_no_paging;
  }
  if(ptr > __simple_malloc_end){
    uart_printf("Error, not enough of simple allocator size, in simple_malloc(). size=%ld\r\n", size);
    uart_printf("__simple_malloc_start=%p, __simple_malloc_end=%p, ptr=%p\r\n", &__simple_malloc_start, __simple_malloc_end, ptr);
    uart_printf("simple malloc usage = %lu/%lu\r\n", (uint64_t)(ptr-&__simple_malloc_start), (uint64_t)(__simple_malloc_end-&__simple_malloc_start));
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
#define GET_PAGE_BUDDY_NODE(page_num) ( (buddynode*) (GET_PAGE_ADDR((page_num)) + 8) ) // +8 to provide offset to prevent confusion of NULL and 0x0000

#define MAX_CONTI_ALLOCATION_EXPO 16 // i.g. =6 means max allocation size is 4kB * 2^6
#define FRAME_ARRAY_F -1            // The idx’th frame is free, but it belongs to a larger contiguous memory block. Hence, buddy system doesn’t directly allocate it.
#define FRAME_ARRAY_X -2            // The idx’th frame is already allocated, hence not allocatable.
#define FRAME_ARRAY_P -3            // The idx’th frame is preserved, not allocatable
static buddy_status *the_frame_array;        // Has (heap_size/PAGE_SIZE) elements, i.e., total_pages elements
static size_t total_pages = 0;      // = heal_size / PAGE_SIZE
static uint64_t heap_start_addr = 0;// start address of heap

#define MALLOC_WHOLE_PAGE (PAGE_SIZE + 1)
static int *malloc_page_usage;      // used for malloc, malloc_page_usage[i] == k means that k bytes in page #i are allocated through malloc
                                    // malloc_page_usage[i] == MALLOC_WHOLE_PAGE means that the page #i is allocated as single or multiple page for large diy_malloc() request

// frame_freelist_arr[i] points to the head of the linked list of free 4kB*(2^i) pages
buddynode *frame_freelist_arr[MAX_CONTI_ALLOCATION_EXPO + 1] = {NULL};

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
  uart_printf("Unused buddies: (index, the_frame_array[index].val) = ");
  for(int i=0; i<total_pages; i++){
    if(the_frame_array[i].used == 0 && the_frame_array[i].val > 0)
      uart_printf("(%d,%ld) ", i, (uint64_t)the_frame_array[i].val);
  }
  uart_printf("\r\n");

  uart_printf("Used buddies:   (index, the_frame_array[index].val) = ");
  for(int i=0; i<total_pages; i++){
    if(the_frame_array[i].used == 1 && the_frame_array[i].val > 0)
      uart_printf("(%d,%ld) ", i, (uint64_t)the_frame_array[i].val);
  }
  uart_printf("\r\n");
}
void dupmp_frame_freelist_arr(){
  uart_printf("frame_freelist_arr[] = \r\n");
  for(int i=MAX_CONTI_ALLOCATION_EXPO; i>=0; i--){
    buddynode *node = frame_freelist_arr[i];
    uart_printf("\t4kB *%3d: ", 1 << i);
    while(node != NULL){
      uart_printf("%2ld-->", GET_PAGE_NUM((uint64_t)node));
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
      if(malloc_page_usage[i] == MALLOC_WHOLE_PAGE){
        uart_printf("Page %d, entire page allocated at once.\r\n", i);
      }
      else{
        uart_printf("Page %d, usage = %d bytes\r\n", i, malloc_page_usage[i]);
        // Traverse all chunks in page #i
        while((uint64_t)header < GET_PAGE_ADDR(i+1)){ // limit at current page
          uart_printf("\tusable addr=%p, used=%d, size=%lu\r\n", &header[1], header->used, (uint64_t)header->size);
          header = (chunk_header*) ( (uint64_t)header + header->size );
        }
      }
    }
  }
  uart_printf("\r\n");
}

// buddynode linked list opertation
static void buddynode_remove(buddynode *node){
  if(node == NULL){
    uart_printf("Error, in buddynode_remove(), node=NULL\r\n");
    return;
  }
  if(node->prev != NULL)  node->prev->next = node->next;
  if(node->next != NULL)  node->next->prev = node->prev;
}
static void buddynode_insert_a_before_b(buddynode *a, buddynode *b){
  if(a == NULL){
    uart_printf("Error, in buddynode_insert_a_before_b(), a=NULL\r\n");
    return;
  }
  else if(b == NULL){
    a->prev = NULL;
    a->next = NULL;
  }
  else{ // a!=NULL && b!=NULL
    a->prev = b->prev;
    a->next = b;
    b->prev = a;
    if(a->prev != NULL) a->prev->next = a;
  }
}

int alloc_page(int page_cnt, int verbose){
  int curr_page_cnt = 0;
  int page_allocated = -1;
  buddynode *temp_node = NULL;
  buddynode **head = NULL;
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

  // Search for free buddy that fits page_cnt in frame_freelist_arr
  for(int i=0; i<=MAX_CONTI_ALLOCATION_EXPO; i++){
    head = &frame_freelist_arr[i];  // head points to the linked list of sets of same # free pages
    curr_page_cnt = 1 << i;         // # of free pages now head points to
    if(page_cnt <= curr_page_cnt && (*head) != NULL){
      break;
    }
  }

  // Free buddy found
  if(page_cnt <= curr_page_cnt && (*head) != NULL) {
    page_allocated = GET_PAGE_NUM((uint64_t)*head);   // Required page#
    // Remove the buddy found from (*head)
    buddynode_remove(*head);
    (*head) = (*head)->next;

    // Mark part of the free buddy allocated in the_frame_array
    the_frame_array[page_allocated].val = page_cnt;
    the_frame_array[page_allocated].used = 1;
    const int end_idx = page_allocated + page_cnt;
    const int start_idx = page_allocated + 1;
    for(int k=start_idx; k<end_idx; k++)  the_frame_array[k].val = FRAME_ARRAY_X;

    // Re-assign the rest of the free block to new buddies
    if(page_cnt < curr_page_cnt) {
      int pages_remain = curr_page_cnt - page_cnt; // pages count remained in the buddy
      int nfblock_size = page_cnt;  // new frame block size, unit=page
      int nfblock_start = end_idx;  // new frame block starting index
      while(pages_remain > 0){
        int fflist_arr_idx = log2_floor(nfblock_size);
        // Insert 1 node into the linked list
        head = &frame_freelist_arr[fflist_arr_idx];  // head points to the linked list of sets of same # free pages
        temp_node = GET_PAGE_BUDDY_NODE(nfblock_start);
        buddynode_insert_a_before_b(temp_node, *head);
        (*head) = temp_node; // update frame_freelist_arr[fflist_arr_idx]

        // Mark buddy begining
        the_frame_array[nfblock_start].val = nfblock_size;
        the_frame_array[nfblock_start].used = 0;
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
  return -1;
}

/** Merge frame_freelist_arr[fflists_idx] and it's buddy into frame_freelist_arr[fflists_idx+1]. 
 * So before calling this function, a free node should be inserted to the head (frame_freelist_arr[fflists_idx]). Then 
 * call this function. This function locates head's buddy in the_frame_array. 
 * If buddy is free and has the same size, merge them into a larger block, 
 * i.e., insert it(2 nodes become 1) into the head of the linked list of larger block size. Finally return fflists_idx+1.
 * @param fflists_idx: The index of frame_freelist_arr, indicating which block size to merge
 * @return fflists_idx+1 if buddy is free and has the same size. 1 << (fflists_idx+1) is the block size that merged into. 
 *  Return -1 if cannot merge.
 *  Return -2 if exception.
*/
static int free_page_merge(int fflists_idx){
  // Return if no head in the linked list
  if(frame_freelist_arr[fflists_idx] == NULL){
    uart_printf("Exception, frame_freelist_arr[%d] is NULL. @line=%d, file:%s\r\n", fflists_idx, __LINE__, __FILE__);
    return -2;
  }
  
  int freeing_page = GET_PAGE_NUM((uint64_t)frame_freelist_arr[fflists_idx]);
  int block_size = the_frame_array[freeing_page].val;   // How many contiguous pages to free
  int buddy_LR = (freeing_page / block_size % 2);       // 0 for left(lower) buddy, 1 for right(higher) buddy
  // Compute the index of the neighbor to merge
  int buddy_page = freeing_page + (buddy_LR ? -block_size : +block_size);
  if(buddy_LR != 0 && buddy_LR != 1){  // Exception
    uart_printf("Exception, shouldn't get here. buddy_LR=%d, @line=%d, file:%s\r\n", buddy_LR, __LINE__, __FILE__);
    return -2;
  }
  
  // Check if merging into greater buddy is acceptable
  if((fflists_idx+1) >= ITEM_COUNT(frame_freelist_arr)){
    return -1;
  }

  // Merge head and it's buddy in the linked list
  buddynode **head = &frame_freelist_arr[fflists_idx];
  buddynode *buddy_node = GET_PAGE_BUDDY_NODE(buddy_page);
  fflists_idx++;
  // Check if the buddy of buddy_page has the same size and is free
  if(the_frame_array[buddy_page].val == block_size && the_frame_array[buddy_page].used == 0){
    // Remove head and the buddy node from the linked list
    buddynode_remove(buddy_node);
    buddynode_remove(*head);
    (*head) = (*head)->next;      // update head of linked list of smaller size one

    // Insert node to the head of the linked list of a greater block size
    const int merged_page = (freeing_page < buddy_page) ? freeing_page : buddy_page; // = min(freeing_page, buddy_page), block's head is the smaller one
    buddynode *merged_node = GET_PAGE_BUDDY_NODE(merged_page);
    buddynode_insert_a_before_b(merged_node, frame_freelist_arr[fflists_idx]);
    frame_freelist_arr[fflists_idx] = merged_node;

    // Update the frame array
    block_size = block_size << 1;
    const int start_idx = merged_page + 1;
    const int end_idx = merged_page + block_size;
    the_frame_array[merged_page].val = block_size;
    the_frame_array[merged_page].used = 0;
    for(int i=start_idx; i < end_idx; i++) the_frame_array[i].val = FRAME_ARRAY_F;

    // uart_printf("Merging %d and %d into %d. merged block_size=%d, start_idx=%d, end_idx=%d\r\n", 
    //   buddy_page, freeing_page, merged_page, block_size, start_idx, end_idx);

    return fflists_idx;
  }

  // We are here because buddy block is either allocated or has different size than freeing_page
  return -1;
}

/** Free a page allocated from alloc_page()
 * @return 0 on success. -1 on error.
*/
int free_page(int page_index, int verbose){
  int block_size = the_frame_array[page_index].val; // How many contiguous pages to free
  int fflists_idx = log2_floor(block_size);

  // Check if ok to free, return -1 if not ok to free
  if(block_size < 0){
    uart_printf("Error, freeing wrong page. the_frame_array[%d]=%d, the page belongs to a block\r\n", page_index, block_size);
    return -1;
  }
  else if(block_size == 1){
    buddynode *node = frame_freelist_arr[fflists_idx];
    // Search for page_index in the linked list
    while(node != NULL){
      if(GET_PAGE_NUM((uint64_t)node) == page_index){
        uart_printf("Error, freeing wrong page. Page %d is already in free lists. block_size=%d\r\n", page_index, block_size);
        return -1;
      }
      node = node->next;
    }
  }
  else{ // block_size > 1
    if(the_frame_array[page_index + 1].val != FRAME_ARRAY_X){
      uart_printf("Error, freeing wrong page. Page %d is already in free lists. block_size=%d\r\n", page_index, block_size);
      return -1;
    }
  }
  if(the_frame_array[page_index].used != 1){
    uart_printf("Error, freeing the page %d, it is not allocated yet.", page_index);
    return -1;
  }

  // Insert a free node into frame_freelist_arr[fflists_idx]
  buddynode *node = GET_PAGE_BUDDY_NODE(page_index);
  buddynode_insert_a_before_b(node, frame_freelist_arr[fflists_idx]);
  frame_freelist_arr[fflists_idx] = node;
  // Update the frame array
  the_frame_array[page_index].used = 0;
  const int start_idx = page_index + 1;
  const int end_idx = page_index + block_size;
  for(int i=start_idx; i < end_idx; i++) the_frame_array[i].val = FRAME_ARRAY_F;
  
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

void alloc_page_preinit(uint64_t heap_start, uint64_t heap_end){
  const size_t heap_size = heap_end - heap_start;
  total_pages = (heap_size / PAGE_SIZE);
  heap_start_addr = heap_start;
  uart_printf("total_pages=%ld, heap_size=%ld bytes\r\n", total_pages, heap_size);

  // Calculate __simple_malloc_end
  uint64_t simple_malloc_last_byte = (uint64_t) &__simple_malloc_start;
  simple_malloc_last_byte += sizeof(int) * total_pages;           // for malloc_page_usage
  simple_malloc_last_byte += sizeof(buddy_status) * total_pages;  // for the_frame_array
  simple_malloc_last_byte += (16 - (simple_malloc_last_byte%16)); // round to multiple of 16
  __simple_malloc_end = (char*)( simple_malloc_last_byte + 1024); // add 1024 for other purpose
  uart_printf("__simple_malloc_start=%p, __simple_malloc_end=%p\r\n", &__simple_malloc_start, __simple_malloc_end);
  // Init: allocate space
  malloc_page_usage = (int*) simple_malloc(sizeof(int) * total_pages);
  the_frame_array = (buddy_status*) simple_malloc(sizeof(buddy_status) * total_pages);
  for(int i=0; i<total_pages; i++){
    malloc_page_usage[i] = -1;
    the_frame_array[i].val = FRAME_ARRAY_X;
    the_frame_array[i].used = 1;
  }

  // Preserve space for simple_malloc()
  mem_reserve((uint64_t)&__simple_malloc_start, (uint64_t)__simple_malloc_end);
}

void alloc_page_init(uint64_t heap_start, uint64_t heap_end){
  if(total_pages == 0){
    uart_printf("Error, please call alloc_page_preinit() first. in alloc_page_init()\r\n");
    return;
  }

  // New efficient way
  for(int p=0; p<total_pages; p++){
    // Skip preserved pages
    for(p=p; the_frame_array[p].val == FRAME_ARRAY_P && p<total_pages; p++){};

    // Page number is odd, block size is definitely 1
    if(p%2 == 1){
      int block_size = 1;
      the_frame_array[p].val = 1;
      the_frame_array[p].used = 0;
      buddynode *node = GET_PAGE_BUDDY_NODE(p);
      buddynode_insert_a_before_b(node, frame_freelist_arr[log2_floor(block_size)]);
      frame_freelist_arr[log2_floor(block_size)] = node; // update head
      continue;
    }

    // Determine max block size starting from page p
    int expo;
    for(expo=MAX_CONTI_ALLOCATION_EXPO; expo>=0; expo--){
      // p mod 2^expo == 0
      if((p % (1<<expo)) == 0)
        break;
    }

    // Adjust block_size if necessary, and than insert a free node into the linked list
    if(expo < 0)
      uart_printf("Error, expo<0, expo=%d, p=%d, should not get here. in alloc_page_init()\r\n", expo, p);
    else{
      // Check if from p ~ p+block_size are all preserved
      int block_size = 1 << expo;
      int k = 0;
      for(k=p; k<(p+block_size) && the_frame_array[k].val!=FRAME_ARRAY_P && k<total_pages; k++);
      // Shrink block size due to either:
      //    some page in the block is reserved, or,
      //    amount of remaining pages cannot satisfy the block size
      if(the_frame_array[k].val == FRAME_ARRAY_P || k >= total_pages){
        block_size = 1 << log2_floor(k-p);  // shrink block size
        // uart_printf("Debug: p=%d, k=%d, expect_size=%d, block_size=%d\r\n", p, k, 1<<expo, block_size);
      }

      // Insert a free node into the linked list
      the_frame_array[p].val = block_size;
      the_frame_array[p].used = 0;
      // free_page(p, 0);
      buddynode *node = GET_PAGE_BUDDY_NODE(p);
      buddynode_insert_a_before_b(node, frame_freelist_arr[log2_floor(block_size)]);
      frame_freelist_arr[log2_floor(block_size)] = node; // update head
      p = p + block_size - 1;
    }
  }
}

void mem_reserve(uint64_t start, uint64_t end){
  const int start_page = GET_PAGE_NUM(start);
  const int end_page = GET_PAGE_NUM(end);
  if(start_page < 0 || end_page < 0 || start_page >= total_pages || end_page >= total_pages || start_page > end_page){
    uart_printf("Error, wrong memory reserve range, start=0x%lX, end=0x%lX, start_page=%d, end_page=%d\r\n",
      start, end, start_page, end_page);
    return;
  }
  uart_printf("prevserved from page %d to %d\r\n", start_page, end_page);
  if(start_page >= 0 && end_page <= total_pages)
    for(int i=start_page; i<=end_page; i++){
      the_frame_array[i].val = FRAME_ARRAY_P;
      the_frame_array[i].used = 1;
    }
}

// diy_malloc, diy_free for small memory ---------------------------
void *diy_malloc(size_t size){
  // TODO: Handle allocation for size > (PAGE_SIZE-sizeof(chunk_header))
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
    return diy_malloc(size);
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
  size_t desire_size = size + sizeof(chunk_header);
  desire_size += (8-(desire_size % 8)); // round up to 8

  // Large request, allocate pages for the request
  if(desire_size >= PAGE_SIZE){
    // Calculate how many pages need to be allocate
    const int pages = size / PAGE_SIZE + ((size%PAGE_SIZE) != 0);   // ceil(size / PAGE_SIZE)
    // Allocate pages
    const int allocated_page = alloc_page(pages, 0);
    for(int i=allocated_page; i<allocated_page+pages; i++)
      malloc_page_usage[i] = MALLOC_WHOLE_PAGE;

    return (void*) GET_PAGE_ADDR(allocated_page);
  }
  // Small request, find free chunks in  curr_page
  else{
    // First fit: find the first fittable hole
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
}

void diy_free(void *addr){
  chunk_header *header = addr - sizeof(chunk_header);
  int page_num = GET_PAGE_NUM((uint64_t)addr);

  // Free pages
  if((((uint64_t)addr - heap_start_addr) % PAGE_SIZE) == 0){
    const int page_size = the_frame_array[page_num].val;
    free_page(page_num, 0);
    for(int i=page_num; i<page_num+page_size; i++)
      malloc_page_usage[i] = -1;

    return;
  }
  // Free chunk
  else{
    // Check if freeing wrong address
    if(header->used == 0 || header->size < (sizeof(chunk_header)+1)){
      uart_printf("Error, failed to free addr=%p, .used=%d, .size=%lu\r\n", addr, header->used, (uint64_t)header->size);
      return;
    }

    // Mark this chunk unused and substract usage
    header->used = 0;
    malloc_page_usage[page_num] -= header->size;
  }

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
        else; // found neighbor chunk but it's in use, do nothing
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

