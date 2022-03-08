
#include "uart.h"
#include "mbox.h"
#include "general.h"
#include <string.h>
#include <stdint.h>

#define MACHINE_NAME "rpi3-baremetal-lab1$ "
#define CMD_HELP    "help"
#define CMD_HELLO   "hello"
#define CMD_REBOOT  "reboot"
#define CMD_LSHW    "lshw"

static void show_hardware_info();

void main()
{
  char input_s[32];
  int index = 0;

  // set up serial console
  uart_init();

  // say hello
  uart_printf("\r\n\r\n");
  uart_printf("Welcome------------------------ lab 1\r\n");
  uart_printf(MACHINE_NAME);

  while(1) {
    char temp = uart_getc();

    // Read cmd
    if(temp == '\b' && index > 0){           // backspace implementation
      uart_printf("\b \b");
      index--;
      input_s[index] = '\0';
    }
    else if(temp != '\b' && temp != '\n'){
      uart_send(temp); // echos
      input_s[index] = temp;
      index++;
    }
    // Execute cmd
    else if(temp == '\n'){
      input_s[index] = '\0';
      uart_printf("\r\n");

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

      // Ready for next cmd
      index = 0;
      uart_printf(MACHINE_NAME);
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
