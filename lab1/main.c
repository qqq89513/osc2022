
#include "uart.h"
#include <string.h>

#define MACHINE_NAME "rpi3-baremetal$ "
#define CMD_HELP    "help"
#define CMD_HELLO   "hello"
#define CMD_REBOOT  "reboot"
#define CMD_LSHW    "lshw"

void main()
{
  char input_s[32];
  int index = 0;

  // set up serial console
  uart_init();

  // say hello
  uart_printf("Welcome------------------------\r\n");
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
      uart_send('\n');

      if     (strcmp(input_s, CMD_HELP) == 0){
        uart_printf(CMD_HELP   "\t: print this help menu\r\n");
        uart_printf(CMD_HELLO  "\t: print Hello World!\r\n");
        uart_printf(CMD_REBOOT "\t: reboot the device\r\n");
        uart_printf(CMD_LSHW   "\t: printf hardware info acquired from mailbox\r\n");
      }
      else if(strcmp(input_s, CMD_HELLO) == 0){
        uart_printf("Hello World!\r\n");
      }
      else if(strcmp(input_s, CMD_REBOOT) == 0){
      }
      else if(strcmp(input_s, CMD_LSHW) == 0){
      }
      else
        uart_printf("Unknown cmd \"%s\".\r\n", input_s);

      // Ready for next cmd
      index = 0;
      uart_printf(MACHINE_NAME);
    }
  }
}
