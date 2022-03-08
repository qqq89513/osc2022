#ifndef __UART_H
#define __UART_H

void uart_init();
void uart_printf(const char *fmt, ...);
void uart_send(unsigned int c);
char uart_getc();
void uart_puts(char *s);
int uart_gets_n(int n, char *str, int echo);

#endif /* __UART_H */
