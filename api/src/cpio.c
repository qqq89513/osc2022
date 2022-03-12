
#include <stdint.h>
#include "diy_string.h"
#include "cpio.h"
#include "diy_sscanf.h"
#include "uart.h"


static cpio_file_ll files_arr[CPIO_MAX_FILES];


static uint32_t header_get_mem(uint8_t* member, size_t size){
  static char str_buf[32];
  uint32_t ret = 0;
  memcpy_(str_buf, member, size);
  str_buf[size] = '\0';
  if(sscanf_(str_buf, "%8X", &ret) != 1)
    return -1;
  return ret;
}

static int pad_to_4(int num){
  int modded = num % 4;
  return modded==0 ? 0 : 4 - modded;
}

int cpio_parse(void *addr){
  cpio_newc_header_t *header = (cpio_newc_header_t*) addr;
  uint32_t magic_num = 0;
  uint32_t file_size = 0;
  uint32_t name_size = 0;
  uint8_t *data_ptr = NULL;
  uint8_t *file_name = NULL;
  cpio_file_ll *file = files_arr;
  while(1){
    // Check for magic num
    magic_num = header_get_mem(header->c_magic, sizeof(header->c_magic));
    if(magic_num != CPIO_MAGIC_NUM){
      uart_printf("[Error] magic_num=%d, should be %d instead.\r\n", magic_num, CPIO_MAGIC_NUM);
      file->next = NULL;
      return -1;
    }

    // Get file size and pathname size
    file_size = header_get_mem(header->c_filesize, sizeof(header->c_filesize));
    name_size = header_get_mem(header->c_namesize, sizeof(header->c_namesize));

    // Get file name (aka pathname)
    file_name = (uint8_t*)(&header[1]); // equivalent to ((u8*)header) + sizeof(header) or (u8*)(header+1)

    // Get file data address
    data_ptr = (uint8_t*)(&header[1]) + name_size + pad_to_4(name_size + sizeof(cpio_newc_header_t));
    
    // Get next address
    file->data_ptr  = data_ptr;
    file->file_size = file_size;
    file->pathname  = file_name;
    if(strcmp_(CPIO_END_RECORD, (const char*)file_name) != 0){
      header = (cpio_newc_header_t*)( data_ptr + file_size + pad_to_4(file_size) );
      file->next = file + 1; // should be malloc() here.
      file = file->next;
    }
    else{
      file->next = NULL;
      return 0;
    }
  }
}

void cpio_ls(){
  cpio_file_ll *file = files_arr;

  // Traverse the linked list
  while(file->next != NULL){
    uart_printf("%s\r\n", file->pathname);
    file = file->next;
  }
}

int cpio_cat(char *file_name){
  cpio_file_ll *file = files_arr;

  // Traverse the linked list
  while(file->next != NULL){
    if(strcmp_(file_name, (char*)file->pathname) == 0){
      // Dump for debug
      for(int i = 0; i < file->file_size; i++) uart_printf("%c", file->data_ptr[i]);
      uart_printf("\r\n");
      return 0;
    }
    file = file->next;
  }
  uart_printf("cat: cannot access '%s': No such file or directory\r\n", file_name);
  return -1;
}
