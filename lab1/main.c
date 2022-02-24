
#include "uart.h"

void main()
{

  // set up serial console
  uart_init();

  // say hello
  uart_puts("Hello yo~\n");

  // Echo everything back
  while(1) {
    uart_send(uart_getc());
  }
}
