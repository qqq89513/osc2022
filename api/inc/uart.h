#ifndef __UART_H
#define __UART_H

#include <stdint.h>

void uart_init();
void uart_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void uart_send(unsigned int c);
char uart_getc();
uint8_t uart_read_byte();
void uart_puts(char *s);
int uart_gets_n(int n, char *str, int echo);

#endif /* __UART_H */
