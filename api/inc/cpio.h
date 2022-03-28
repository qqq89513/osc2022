#ifndef __DIY_CPIO_H_
#define __DIY_CPIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CPIO_ADDR ((void*)0x8000000) // 0x20000000 for bare-metal rpi, 0x8000000 for qemu
#define CPIO_MAX_FILES 32
#define CPIO_MAGIC_NUM 0x070701
#define CPIO_END_RECORD "TRAILER!!!"

// For more details: https://www.freebsd.org/cgi/man.cgi?query=cpio&sektion=5

// The "new" ASCII format uses 8-byte hexadecimal fields for all numbers
typedef struct __cpio_newc_header{
  uint8_t c_magic[6];       // The string "070701".
  uint8_t c_ino[8];
  uint8_t c_mode[8];
  uint8_t c_uid[8];
  uint8_t c_gid[8];
  uint8_t c_nlink[8];
  uint8_t c_mtime[8];
  uint8_t c_filesize[8];    // The size of the file. Note that this archive format is limited to four gigabyte file sizes.
  uint8_t c_devmajor[8];
  uint8_t c_devminor[8];
  uint8_t c_rdevmajor[8];
  uint8_t c_rdevminor[8];
  uint8_t c_namesize[8];    // The number of bytes in the pathname that follows the header. This count includes the trailing NUL byte.
  uint8_t c_check[8];
} cpio_newc_header_t;

typedef struct __cpio_file{
  uint8_t *pathname;       // string
  uint32_t file_size;
  uint8_t *data_ptr;
  struct __cpio_file* next;
} cpio_file_ll; // ll for linked list

typedef int (cpio_parse_func) (void*); // function of ( int cpio_parse(void *addr); )
int cpio_parse(void *addr);
void cpio_ls();
int cpio_copy(char *file_name, uint8_t *destination);
int cpio_cat(char *file_name);

#ifdef __cplusplus
}
#endif
#endif  // __DIY_CPIO_H_