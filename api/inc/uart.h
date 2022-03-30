#ifndef __UART_H
#define __UART_H

#include <stdint.h>


#define IRQS1_PENDING ((volatile int*) (MMIO_BASE+0x0000b204))
#define IRQS1_ENABLE  ((volatile int*) (MMIO_BASE+0x0000b210))
#define IRQS1_DISABLE ((volatile int*) (MMIO_BASE+0x0000b21c))
#define AUX_INT       (0x01 << 29) // ref: p.113, https://cs140e.sergio.bz/docs/BCM2837-ARM-Peripherals.pdf
#define _enable_uart_interrupt()    (*IRQS1_ENABLE  = AUX_INT)
#define _disable_uart_interrupt()   (*IRQS1_DISABLE = AUX_INT)
#define _enable_tx_interrupt()      ( *AUX_MU_IER |=   0x2 )
#define _disable_tx_interrupt()     ( *AUX_MU_IER &= ~(0x2))
#define _enable_rx_interrupt()      ( *AUX_MU_IER |=   0x1 )
#define _disable_rx_interrupt()     ( *AUX_MU_IER &= ~(0x1))

void uart_init();
void uart_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void uart_send(unsigned int c);
char uart_getc();
uint8_t uart_read_byte();
void uart_puts(char *s);
int uart_gets_n(int n, char *str, int echo);
void uart_rx_tx_handler();
int uart_getc_async();
int uart_gets_n_async(int n, char *str, int echo);
void uart_puts_async(char *str);

#endif /* __UART_H */
