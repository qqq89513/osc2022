
#include "uart.h"
#include "mbox.h"
#include "general.h"
#include "diy_sscanf.h"
#include "diy_string.h"
#include "cpio.h"
#include "diy_malloc.h"
#include "fdtb.h"
#include <stdint.h>

#define MACHINE_NAME "rpi3-baremetal-lab4$ "
#define CMD_HELP     "help"
#define CMD_HELLO    "hello"
#define CMD_REBOOT   "reboot"
#define CMD_LS       "ls"
#define CMD_CAT      "cat"
#define CMD_LSDEV    "lsdev"
#define CMD_ALLOCATE_PAGE "ap"
#define CMD_FREE_PAGE     "fp"
#define CMD_MALLOC        "m"
#define CMD_FREE          "f"
#define CMD_DUMP_PAGE     "dump_page"

#define ADDR_IMAGE_START 0x80000

static void sys_init(void *dtb_addr);
static int spilt_strings(char** str_arr, char* str, char* deli);
extern uint64_t __image_start, __image_end;
extern uint64_t __stack_start, __stack_end;
void main(void *dtb_addr)
{
  char *input_s;
  char **args;
  int args_cnt = 0;

  sys_init(dtb_addr);

  input_s = simple_malloc(sizeof(char) * 32);
  args = simple_malloc(sizeof(char*) * 10);

  // say hello
  uart_printf("\r\n\r\n");
  uart_printf("Welcome------------------------ lab 4\r\n");

  uart_printf("dtb_addr=0x%p, __image_start=%p, __image_end=%p\r\n", dtb_addr, &__image_start, &__image_end);

  while(1) {

    // Read cmd
    uart_printf(MACHINE_NAME);
    uart_gets_n(32, input_s, 1);
    args_cnt = spilt_strings(args, input_s, " ");

    // Execute cmd
    if(strlen_(args[0]) > 0){
      if     (strcmp_(args[0], CMD_HELP) == 0){
        uart_printf(CMD_HELP   "\t\t: print this help menu\r\n");
        uart_printf(CMD_HELLO  "\t\t: print Hello World!\r\n");
        uart_printf(CMD_REBOOT "\t\t: reboot the device\r\n");
        uart_printf(CMD_LS     "\t\t: List files and dirs\r\n");
        uart_printf(CMD_CAT    "\t\t: Print file content\r\n");
        uart_printf(CMD_LSDEV  "\t\t: Print all the nodes and propperties parsed from dtb.\r\n");
        uart_printf(CMD_ALLOCATE_PAGE " <page count>\t: Allocate <page count> from heap.\r\n");
        uart_printf(CMD_FREE_PAGE " <page index>\t: Release <page index>.\r\n");
        uart_printf(CMD_DUMP_PAGE "\t: Dump the frame array and free block lists\r\n");
        uart_printf(CMD_MALLOC " <size>\t: Allocate memory, <size> in bytes\r\n");
        uart_printf(CMD_FREE " <addr>\t: Free memory, <addr> in hex without 0x\r\n");
      }
      else if(strcmp_(args[0], CMD_HELLO) == 0){
        uart_printf("Hello World!\r\n");
      }
      else if(strcmp_(args[0], CMD_REBOOT) == 0){
        uart_printf("Rebooting...\r\n");
        reset(1000);
        while(1);
      }
      else if(strcmp_(args[0], CMD_LS) == 0){
        cpio_ls();
      }
      else if(strcmp_(args[0], CMD_CAT) == 0){
        if(args_cnt > 1)
          cpio_cat(args[1]);
      }
      else if(strcmp_(args[0], CMD_LSDEV) == 0){
        fdtb_parse(dtb_addr, 1, NULL);
      }
      else if(strcmp_(args[0], CMD_ALLOCATE_PAGE) == 0){
        if(args_cnt > 1){
          int page_cnt = 0;
          sscanf_(args[1], "%d", &page_cnt);
          const int page_index = alloc_page(page_cnt, 0);
          if(page_index > -1)   uart_printf("Allocated %d at page #%d\r\n", page_cnt, page_index);
          else                  uart_printf("Error, not enough of contiguious space for page count=%d\r\n", page_cnt);
        }
        else
          uart_printf("Usage: " CMD_ALLOCATE_PAGE " <page count>\t: Allocate <page count> from heap.\r\n");
      }
      else if(strcmp_(args[0], CMD_FREE_PAGE) == 0){
        if(args_cnt > 1){
          int page_index = 0;
          sscanf_(args[1], "%d", &page_index);
          free_page(page_index, 1);
        }
        else
          uart_printf("Usage: " CMD_FREE_PAGE " <page index>\t: Release <page index>.\r\n");
      }
      else if(strcmp_(args[0], CMD_DUMP_PAGE) == 0){
        // dump_the_frame_array();
        dump_the_frame_array();
        dupmp_frame_freelist_arr();
      }
      else if(strcmp_(args[0], CMD_MALLOC) == 0){
        if(args_cnt > 1){
          int size = 0;
          sscanf_(args[1], "%d", &size);
          const void* addr = diy_malloc(size);
          if(addr != NULL)   uart_printf("Allocated %d bytes at addr %p\r\n", size, addr);
          else               uart_printf("Failed to malloc %d bytes\r\n", size);
        }
        else
          uart_printf("Usage: " CMD_MALLOC " <size>: Allocate memory, <size> in bytes\r\n");
      }
      else if(strcmp_(args[0], CMD_FREE) == 0){
        if(args_cnt > 1){
          void *addr = NULL;
          sscanf_(args[1], "%p", &addr);
          diy_free(addr);
        }
        else
          uart_printf("Usage: " CMD_FREE " <addr>\t: Free memory, <addr> in hex without 0x\r\n");
      }
      else
        uart_printf("Unknown cmd \"%s\".\r\n", input_s);
    }
  }
}

static int spilt_strings(char** str_arr, char* str, char* deli){
  int count = 0;
  // Spilt str by specified delimeter
  str_arr[0] = strtok_(str, deli);
  count = 0;
  while(str_arr[count] != NULL){
    count++;
    str_arr[count] = strtok_(NULL, deli);
  }
  return count;
}

static void sys_init(void *dtb_addr){
  
  // IO init
  uart_init();

  // Device tree parse
  const long int dtb_size = fdtb_parse(dtb_addr, 0, cpio_parse);

  // Memory init
  uint32_t *mem_start_addr = 0;
  uint32_t mem_size = 0;
  mbox_arm_mem_info(&mem_start_addr, &mem_size);
  alloc_page_preinit((uint64_t)mem_start_addr, (uint64_t)mem_start_addr + mem_size);
  mem_reserve(0x0, 0x1000);                                       // spin tables for multicore boot
  mem_reserve((uint64_t)&__image_start, (uint64_t)&__image_end);  // kernel image
  mem_reserve((uint64_t)&__stack_end, (uint64_t)&__stack_start);  // stack, grows downward, so range is from end to start
  mem_reserve(0x8000000, 0x8000000 + 2560);                       // initramfs, hard coded
  mem_reserve((uint64_t)dtb_addr, (uint64_t)dtb_addr + dtb_size); // device tree
  alloc_page_init();
}
