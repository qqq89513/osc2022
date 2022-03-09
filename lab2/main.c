
#include "uart.h"
#include "mbox.h"
#include "general.h"
#include <string.h>
#include <stdint.h>

#define MACHINE_NAME "rpi3-baremetal-lab2$ "
#define CMD_HELP    "help"
#define CMD_HELLO   "hello"
#define CMD_REBOOT  "reboot"
#define CMD_LSHW    "lshw"
#define CMD_LKR_UART "lkr_uart"  // load kernel from uart

#define ADDR_IMAGE_START 0x80000

static void show_hardware_info();
static void load_kernel_uart();
extern int sscanf_(const char *ibuf, const char *fmt, ...);

void main()
{
  char input_s[32];

  // set up serial console
  uart_init();

  // sscanf test
  int temp=0, hum=0, neg=0;
  int n = 0;
  n = sscanf_("temp=221, hum=+108, neg=-10, string=yoyo", "temp=%d, hum=%d, neg=%d, string=%s", &temp, &hum, &neg, input_s);
  uart_printf("sscanf_: n=%d temp=%d, hum=%d, neg=%d, string=%s\r\n", n, temp, hum, neg, input_s);

  // say hello
  uart_printf("\r\n\r\n");
  uart_printf("Welcome------------------------ lab 2\r\n");

  while(1) {

    // Read cmd
    uart_printf(MACHINE_NAME);
    uart_gets_n(sizeof(input_s), input_s, 1);

    // Execute cmd
    if(strlen(input_s) > 0){
      if     (strcmp(input_s, CMD_HELP) == 0){
        uart_printf(CMD_HELP   "\t: print this help menu\r\n");
        uart_printf(CMD_HELLO  "\t: print Hello World!\r\n");
        uart_printf(CMD_REBOOT "\t: reboot the device\r\n");
        uart_printf(CMD_LSHW   "\t: print hardware info acquired from mailbox\r\n");
      }
      else if(strcmp(input_s, CMD_HELLO) == 0){
        uart_printf("Hello World!\r\n");
      }
      else if(strcmp(input_s, CMD_REBOOT) == 0){
        uart_printf("Rebooting...\r\n");
        reset(1000);
        while(1);
      }
      else if(strcmp(input_s, CMD_LSHW) == 0){
        show_hardware_info();
      }
      else if(strcmp(input_s, CMD_LKR_UART) == 0){
        load_kernel_uart();
      }
      else
        uart_printf("Unknown cmd \"%s\".\r\n", input_s);
    }
  }
}

static void show_hardware_info(){
  uint32_t board_rev=0;
  uint32_t *mem_start_addr = 0;
  uint32_t mem_size = 0;
  mbox_board_rev(&board_rev);
  mbox_arm_mem_info(&mem_start_addr, &mem_size);
  uart_printf("board_rev=0x%08X\r\n", board_rev);
  uart_printf("mem_start_addr=0x%08X\r\n", mem_start_addr);
  uart_printf("mem_size=0x%08X\r\n", mem_size);
}

static void load_kernel_uart(){
  char input_s[32];
  int bytes_to_print = 0;
  uint8_t* addr_kernel = (uint8_t*)ADDR_IMAGE_START;
  uint64_t image_size = 0;

  // Read size
  uart_printf("Image size=");
  uart_gets_n(sizeof(input_s), input_s, 1);
  sscanf_(input_s, "%lu", &image_size);

  // Check size
  if(image_size == 0){
    uart_printf("Error, image size cannot be 0\r\n");
    return;
  }
  // Read binary from uart
  else{
    uart_printf("Receiving %lu bytes...\n", image_size);
    for(uint64_t i=0; i<image_size; i++){
      addr_kernel[i] = uart_getc();
    }
    bytes_to_print = image_size > 20 ? 20 : image_size;
    uart_printf("%lu of bytes received from uart. ", image_size);
    uart_printf("First %d bytes received (HEX): ", bytes_to_print);
    for(uint64_t i=0; i<bytes_to_print; i++)
      uart_printf("%02X ", addr_kernel[i]);
    uart_printf("\r\n");

    // Jump to new kernel
    volatile void (*jump_to_new_kernel) (void) = (void (*) (void))  (addr_kernel);
    jump_to_new_kernel();
    while(1);
  }
}
