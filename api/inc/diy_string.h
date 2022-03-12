
#ifndef __DIY_STRING_H_
#define __DIY_STRING_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

unsigned int strlen_(const char* src);
char* strcat_(char* dest,char* src);
char* strcpy_(char* dest,const char* src);
int strcmp_(const char* src1,const char* src2);
char *strtok_(char *str, char *delimiter);
void memcpy_(void* dest, const void* src, size_t len);
void *memset_(void *str, int c, size_t len);


#ifdef __cplusplus
}
#endif


#endif  // __DIY_STRING_H_
