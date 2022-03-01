
#include "uart.h"
#include <stdio.h>

void main()
{
  char temp[128] = {'\0'};
  // set up serial console
  uart_init();

  // say hello
  uart_puts("Hello yo~\n");

  snprintf(temp, sizeof(temp), "haha using snprintf() now. snprintf() located at %p\r\n", snprintf);
  uart_puts(temp);

  // Echo everything back
  while(1) {
    uart_send(uart_getc());
  }
}
