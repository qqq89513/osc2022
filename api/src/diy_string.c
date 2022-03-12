#include "diy_string.h"

unsigned int strlen_(const char* src){
  unsigned int len = 0;
  while(*src++)
  len++;
  return len;
}

char* strcat_(char* dest,char* src){
  char *tmp = dest;
  while(*dest++);
  dest--;
  while((*dest++ = *src++));
  return tmp;
}

char* strcpy_(char* dest,const char* src){
  char *tmp = dest;
  while(*src)
  *dest++=*src++;

  *dest='\0';
  return tmp;
}

int strcmp_(const char* src1,const char* src2){
  int x=0;
  while(!(x = *src1-*src2) && *src1){
    src1++;
    src2++;
  }
  if(x>0)  x = 1;
  if(x<0)  x = -1;
  return x;
}

// Credits to: https://stackoverflow.com/questions/28931379/implementation-of-strtok-function, Shirley V
char *strtok_(char *str, char *delimiter){
  static char *temp_ptr = NULL;
  char *final_ptr = NULL;
  /* Flag has been defined as static to avoid the parent function loop
   * runs infinitely after reaching end of string.
   */ 
  static int flag = 0;
  int i, j;

  if (delimiter == NULL) {
    flag = 0;
    temp_ptr = NULL;
    return NULL;
  }

  /* If flag is 1, it will denote the end of the string */
  if (flag == 1) {
    flag = 0;
    temp_ptr = NULL;
    return NULL;
  }

  /* The below condition is to avoid temp_ptr is getting assigned 
   * with NULL string from the parent function main. Without
   * the below condition, NULL will be assigned to temp_ptr 
   * and final_ptr, so accessing these pointers will lead to
   * segmentation fault.
   */
  if (str != NULL) { 
    temp_ptr = str; 
  }

  /* Before function call ends, temp_ptr should move to the next char,
   * so we can't return the temp_ptr. Hence, we introduced a new pointer
   * final_ptr which points to temp_ptr.
   */
  final_ptr = temp_ptr;

  for (i = 0; i <= strlen_(temp_ptr); i++)
  {
    for (j = 0; j < strlen_(delimiter); j++) {

      if (temp_ptr[i] == '\0') {
        /* If the flag is not set, both the temp_ptr and flag_ptr 
         * will be holding string "Jasmine" which will make parent 
         * to call this function strtok_ infinitely. 
         */
        flag = 1;
        if(final_ptr == NULL){
          flag = 0;
          temp_ptr = NULL;
        }
        return final_ptr;
      }

      if ((temp_ptr[i] == delimiter[j])) {
        /* NULL character is assigned, so that final_ptr will return 
         * til NULL character. Here, final_ptr points to temp_ptr.
         */
        temp_ptr[i] = '\0';
        temp_ptr += i+1;
        if(final_ptr == NULL){
          flag = 0;
          temp_ptr = NULL;
        }
        return final_ptr;
      }
    }
  }
  /* You will somehow end up here if for loop condition fails.
   * If this function doesn't return any char pointer, it will 
   * lead to segmentation fault in the parent function.
   */
  flag = 0;
  temp_ptr = NULL;
  return NULL;
}

void memcpy_(void* dest, const void* src, size_t len){
  char* dest_ = (char*)dest;
  char* src_ = (char*)src;
  for(size_t i=0; i<len; i++)
    *(dest_++) = *(src_++);
    // dest_[i] = src_[i];
}

void *memset_(void *str, int c, size_t len){
  char* ptr = (void*) str;
  for(int i=0; i<len; i++)
    *++ptr = c;
  return str;
}
