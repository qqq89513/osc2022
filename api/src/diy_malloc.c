
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


// Buddy system -----------------------------------------------------
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

// Dump funcs
void dump_the_frame_array(){
  uart_printf("the_frame_array index = ");
  for(int i=0; i<total_pages; i++)  uart_printf("%2d ", i);
  uart_printf("\r\nthe_frame_array[]     = ");
  for(int i=0; i<total_pages; i++)  uart_printf("%2d ", the_frame_array[i]);
  uart_printf("\r\n");
}
void dupmp_frame_freelist_arr(){
  uart_printf("frame_freelist_arr[] = \r\n");
  for(int i=MAX_CONTI_ALLOCATION_EXPO; i>=0; i--){
    freeframe_node *node = frame_freelist_arr[i];
    uart_printf("\t4kB * 2^%d: ", i);
    while(node != NULL){
      uart_printf("%2d-->", node->index);
      node = node->next;
    }
    uart_printf("NULL\r\n");
  }
}

void malloc_init(uint64_t heap_start, uint64_t heap_end){
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

