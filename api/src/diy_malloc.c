
#include "diy_malloc.h"
#include <stddef.h>
#include "uart.h"

// Simple memory allocation -----------------------------------------
void* simple_malloc(size_t size){
  static char pool[SIMPLE_MALLOC_POOL_SIZE];
  static char *ptr = pool;
  char *temp = ptr;
  if(size < 1){
    uart_printf("Error, not enough of SIMPLE_MALLOC_POOL_SIZE=%d, in simple_malloc()\r\n", SIMPLE_MALLOC_POOL_SIZE);
    return NULL;
  }
  ptr += size;
  return (void*) temp;
}


// Buddy system (page allocation) -----------------------------------
// Ref: https://oscapstone.github.io/labs/overview.html, https://grasslab.github.io/NYCU_Operating_System_Capstone/labs/lab3.html
#define ITEM_COUNT(arr) (sizeof(arr) / sizeof(*arr))
#define PAGE_SIZE 4096 // 4kB

#define MAX_CONTI_ALLOCATION_EXPO 6 // i.g. =6 means max allocation size is 4kB * 2^6
#define FRAME_ARRAY_F -1            // The idx’th frame is free, but it belongs to a larger contiguous memory block. Hence, buddy system doesn’t directly allocate it.
#define FRAME_ARRAY_X -2            // The idx’th frame is already allocated, hence not allocatable.
#define FRAME_ARRAY_P -3            // The idx’th frame is preserved, not allocatable
static int *the_frame_array;        // Has the size of (heap_size/PAGE_SIZE), i.e., total_pages
static size_t total_pages = 0;      // = heal_size / PAGE_SIZE

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

int alloc_page(int page_cnt){
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

  dump_the_frame_array();
  dupmp_frame_freelist_arr();
  return page_allocated;
}

void alloc_page_init(uint64_t heap_start, uint64_t heap_end){
  freeframe_node *freeframe_node_pool = NULL;
  const size_t heap_size = heap_end - heap_start;
  total_pages = (heap_size / PAGE_SIZE);
  uart_printf("total_pages=%ld, heap_size=%ld bytes\r\n", total_pages, heap_size);

  // Init: allocate space 
  the_frame_array = (int*) simple_malloc(sizeof(int) * total_pages);
  freeframe_node_pool = (freeframe_node  *) simple_malloc(sizeof(freeframe_node) * total_pages);
  freeframe_node_fifo = (freeframe_node **) simple_malloc(sizeof(freeframe_node) * total_pages + 1); // +1 to ease edge condition
  fifo_size = total_pages + 1;
  for(int i=0; i<total_pages; i++){
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

  // Dump frame array
  dump_the_frame_array();
  dupmp_frame_freelist_arr();
}

