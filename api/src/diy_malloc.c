
#include "diy_malloc.h"
#include <stddef.h>


void* simple_malloc(size_t size){
  static char pool[MALLOC_POLL_SIZE];
  static char *ptr = pool;
  char *temp = ptr;
  if(size < 1)
    return NULL; 
  ptr += size;
  return (void*) temp;
}

