
#include "fdtb.h"
#include "uart.h"
#include "cpio.h"
#include "diy_string.h"

#define FDT_MAGIC         0xD00DFEED
#define FDT_VERSION       0x00000011
#define FDT_TK_NULL       0X00000000
#define FDT_TK_BEGIN_NODE 0X00000001
#define FDT_TK_END_NODE   0X00000002
#define FDT_TK_PROP_NODE  0X00000003
#define FDT_TK_NOP        0X00000004
#define FDT_TK_END        0X00000009

#define FDT_CPIO_INITRAMFS_PROPNAME "linux,initrd-start"

static uint64_t pad_to_4(void *num);
static void dump(void *arr, const uint32_t len);
static uint32_t rev32(uint32_t val);
static uint64_t endian_rev(void *input, int dtype_size);


void fdtb_parse(void *dtb_addr, int print, cpio_parse_func *callback){
  fdt_header *header =  (fdt_header*) dtb_addr;
  uint8_t *structure_block_end = (uint8_t*)header + rev32(header->off_dt_struct) + rev32(header->size_dt_struct);
  if(print) uart_printf("[Debug] magic=0x%08X, totalsize=0x%08X, off_dt_struct=0x%08X, off_dt_strings=0x%08X, version=0x%08X\r\n",
  rev32(header->magic), rev32(header->totalsize), rev32(header->off_dt_struct), rev32(header->off_dt_strings), rev32(header->version));

  // Check for magic num and version
  if(rev32(header->magic) != FDT_MAGIC || rev32(header->version) != FDT_VERSION){
    uart_printf("Error, fdtb_parse(), expect magic,version=0x%08X, 0x%08X, got=0x%08X, 0x%08X. ", FDT_MAGIC, FDT_VERSION, rev32(header->magic), rev32(header->version));
    uart_printf("dtb_addr=0x%pX, totalsize=0x%08X\r\n", dtb_addr, rev32(header->totalsize));
    return;
  }

  // Parse tree blob
  int depth = 0;
  uint8_t *cur = (uint8_t*)header + rev32(header->off_dt_struct);  // parser current position
  int finish = 0;
  fdt_node_prop *prop = NULL;
  char *propName = NULL;
  char *propVal = NULL;
  while(!finish){
    const uint32_t token = rev32(*((uint32_t*)cur));
    switch(token){
      case FDT_TK_BEGIN_NODE:
        cur += 4;
        depth++;
        if(print) uart_printf("BEGIN_NODE depth=%d, cur=%p, nodename=%s\r\n", depth, cur, cur);
        cur += strlen_((char*)(cur)) + 1; // +1 for null terminated string
        cur += pad_to_4(cur);
        break;
      
      case FDT_TK_PROP_NODE:
        cur += 4;
        prop = (fdt_node_prop*)cur;
        cur += sizeof(fdt_node_prop);
        propName = (char*)(header) + rev32(header->off_dt_strings) + rev32(prop->nameoff);
        propVal = (char*) cur;
        if(print) uart_printf("PROP_NODE  depth=%d, cur=%p, prop.len=%2d, .nameoff=0x%8X, propName=%s, propVal=%s\r\n", 
                      depth, cur, rev32(prop->len), rev32(prop->nameoff), propName, propVal);
        // TODO: Get linux,initrd-start start address here
        if(!strcmp_(FDT_CPIO_INITRAMFS_PROPNAME, propName) && callback != NULL){
          void *addr = (void*) (uint64_t) rev32( *((uint32_t*)propVal) );
          (*callback) (addr);
        }
        cur += rev32(prop->len);
        cur += pad_to_4(cur);
        break;

      case FDT_TK_END_NODE:{
        cur += 4;
        if(print) uart_printf("END_NODE   depth=%d, cur=%p\r\n", depth, cur);
        depth--;
        break;
      }

      case FDT_TK_NOP:{
        cur += 4; // next token (each token is 4 bytes)
        break;
      }

      case FDT_TK_END:{
        cur += 4;
        if(print) uart_printf("END_____   depth=%d, cur=%p\r\n", depth, cur);
        finish = 1;  // parser reaches the end, break the while
        break;
      }
      
      default:{
        uart_printf("Error, token=0x%08X, cur=0x%p, cur[0..64]=\r\n", token, cur);
        dump(cur, 64); // dump 64 bytes to see what's around
        cur += 4;
        break;
      }
    }
    if(cur > structure_block_end){
      uart_printf("Error, reaching the end of structure block, cur=%p, structure_block_end=%p\r\n", cur, structure_block_end);
      break;
    }
  }

  if(print) uart_printf("Parsing ends here\r\n");
}

static uint64_t pad_to_4(void *num){
  uint64_t modded = ((uint64_t)num) % 4;
  return modded==0 ? 0 : 4 - modded;
}

static void dump(void *arr, const uint32_t len){
  uint8_t *ptr = (uint8_t*) arr;
  for(uint32_t i=0; i<len; i++)
    uart_printf(" %02X", *ptr++);
  uart_printf("\r\n");
}

static uint32_t rev32(uint32_t val){
  return (uint32_t) endian_rev(&val, 4);
}

/** Transform data from litle to big endian, or from big to little endian
 * @param input: Pointer to a value that needs to be transformed
 * @param dtype_size: data type size, size of each item in bytes. Possible value: 1, 2, 4, 8
*/
static uint64_t endian_rev(void *input, int dtype_size){
  const uint8_t *ptr = (uint8_t*)input;
  uint64_t ret = 0;

  switch (dtype_size){
    // int8_t, uint8_t
    case 1: 
      // No need to transform to big endian since the data type size is 1 byte
      break;
    
    // int16_t, uint16_t
    case 2:
      ret = (ptr[0] << 8) | ptr[1];
      break;

    // int32_t, uint32_t
    case 4:
      ret = (ptr[0] << 24) | 
            (ptr[1] << 16) | 
            (ptr[2] <<  8) | 
             ptr[3];
      break;

    // int64_t, uint64_t
    // case 8:
    //   ret = (ptr[0] << 56) |
    //         (ptr[1] << 48) |
    //         (ptr[2] << 40) |
    //         (ptr[3] << 32) |
    //         (ptr[4] << 24) |
    //         (ptr[5] << 16) |
    //         (ptr[6] <<  8) |
    //          ptr[7];
    //   break;

    default:
      uart_printf("[Error] Endian transformation(%d) not implemented. @line %d, file:%s\r\n", dtype_size, __LINE__, __FILE__);
      break;
  }
  return ret;
}
