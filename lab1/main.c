
#include "uart.h"

void main()
{
  // set up serial console
  uart_init();

  // say hello
  uart_puts("uart_puts() prints this line.\n");
  uart_printf("uart_printf() prints ths line.\r\n");

  while(1) {
    uart_send(uart_getc());
  }
}
