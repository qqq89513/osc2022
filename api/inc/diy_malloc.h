#ifndef __DIY_MALLOC_H_
#define __DIY_MALLOC_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

#define MALLOC_POLL_SIZE 32768  // 400 kB

void* simple_malloc(size_t size);



#ifdef __cplusplus
}
#endif
#endif  // __DIY_MALLOC_H_
