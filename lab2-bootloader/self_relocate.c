#include <stdint.h>
#include "uart.h"

extern uint64_t __start__;    // defined in link.ld
extern void main();

void self_relocate(){
  uart_init();
  uart_printf("Before relocation: __start__=0x%p, main=0x%p\r\n", &__start__, main);
  
  const uint64_t size = 0x10000 >> 3; // >>3 to be devided by 8
  uint64_t *load_addr = (uint64_t*) 0x80000;
  uint64_t *exec_addr = (uint64_t*) 0x60000;
  
  for(uint64_t i=0; i<size; i++)
    *exec_addr++ = *load_addr++;
  return;
}
