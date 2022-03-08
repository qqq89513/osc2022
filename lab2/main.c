
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

static void show_hardware_info();
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
