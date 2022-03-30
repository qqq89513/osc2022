#include "general.h"
#include "uart.h"
#include "diy_printf.h"

/* Auxilary mini UART registers */
#define AUX_ENABLE    ((volatile unsigned int*)(MMIO_BASE+0x00215004))
#define AUX_MU_IO     ((volatile unsigned int*)(MMIO_BASE+0x00215040))
#define AUX_MU_IER    ((volatile unsigned int*)(MMIO_BASE+0x00215044))
#define AUX_MU_IIR    ((volatile unsigned int*)(MMIO_BASE+0x00215048))
#define AUX_MU_LCR    ((volatile unsigned int*)(MMIO_BASE+0x0021504C))
#define AUX_MU_MCR    ((volatile unsigned int*)(MMIO_BASE+0x00215050))
#define AUX_MU_LSR    ((volatile unsigned int*)(MMIO_BASE+0x00215054))
#define AUX_MU_MSR    ((volatile unsigned int*)(MMIO_BASE+0x00215058))
#define AUX_MU_SCRATCH  ((volatile unsigned int*)(MMIO_BASE+0x0021505C))
#define AUX_MU_CNTL   ((volatile unsigned int*)(MMIO_BASE+0x00215060))
#define AUX_MU_STAT   ((volatile unsigned int*)(MMIO_BASE+0x00215064))
#define AUX_MU_BAUD   ((volatile unsigned int*)(MMIO_BASE+0x00215068))

#define AUX_MU_IIR_TX_MASK    ((volatile unsigned int)(1 << 1)) // Transmit holding register empty   | ref: page 13
#define AUX_MU_IIR_RX_MASK    ((volatile unsigned int)(1 << 2)) // Receiver holds valid byte         | https://cs140e.sergio.bz/docs/BCM2837-ARM-Peripherals.pdf 

// Circular buffer for rx and tx
#define RX_TX_BUF_SIZE 1024
char rx_buf[RX_TX_BUF_SIZE];
char tx_buf[RX_TX_BUF_SIZE];
int rx_buf_out=0, rx_buf_in=0;
int tx_buf_out=0, tx_buf_in=0;

/**
 * Set baud rate and characteristics (115200 8N1) and map to GPIO
 */
void uart_init()
{
  register unsigned int r;

  // Set GPIO14, 15 to no pull up/down. Follow the steps of p.101, BCM2837 datasheet
  *GPPUD = GPPUD_PULL_UP;   // step 1. set pull up, down or none
  WAIT_TICKS(r, 150);       // step 2. wait 150 ticks for signal to set-up
  *GPPUDCLK0 = (1<<14) | (1<<15); // step 3. select clock for GPIO14, 15
  WAIT_TICKS(r, 150);       // step 4. wait 150 ticks for signal to hold
  *GPPUD = 0x00;          // step 5.
  *GPPUDCLK0 = 0;         // step 6. flush GPIO setup

  // Map UART1 to GPIO pins
  r  = *GPFSEL1;
  r &= ~((7<<12) | (7<<15));  // clear GPIO14, 15's state. and leave other's alone
  r |=   (2<<12) | (2<<15);   // 2 is alt5, 12~14th bit for GPIO14, 15th~17 for GPIO15
  *GPFSEL1 = r;

  // Initialize UART
  *AUX_ENABLE |= 1;    // enable UART1, AUX mini uart
  *AUX_MU_CNTL = 0;
  *AUX_MU_LCR  = 3;    // 8 bits
  *AUX_MU_MCR  = 0;
  *AUX_MU_IER  = 0;    // Disable interrupt
  *AUX_MU_IIR  = 0x06; // clear Tx and RX FIFO
  *AUX_MU_BAUD = 270;  // 115200 baud
  *AUX_MU_CNTL = 3;    // enable Tx, Rx
}

/* Wrapping snprintf and HAL_UART_Transmit, just use this as printf
  Note that this function does no buffering like printf.
  The maximum characters to print at once is 128,
  you can change buf size for that.
*/
void uart_printf(const char *fmt, ...){
  int len = 0;
  char buf[128];
  __builtin_va_list args;
  __builtin_va_start(args, fmt);
  len = vsnprintf_(buf, sizeof(buf), fmt, args);
  if(len > 0)
    uart_puts(buf);
  __builtin_va_end(args);
}

/**
 * Send a character
 */
void uart_send(unsigned int c) {
  // wait until we can send
  while(!(*AUX_MU_LSR&0x20));
  // write the character to the buffer
  *AUX_MU_IO = c;
}

/**
 * Receive a character
 */
char uart_getc() {
  char r;
  // wait until something is in the buffer
  while(!(*AUX_MU_LSR&0x01));

  // read it and return
  r = (char)(*AUX_MU_IO);
  
  // convert carrige return to newline
  r = (r=='\r' ? '\n' : r);
  return r;
}

uint8_t uart_read_byte(){
  uint8_t r;
  // wait until something is in the buffer
  while(!(*AUX_MU_LSR&0x01));

  // read it and return
  r = (char)(*AUX_MU_IO);
  return r;
}

/**
 * Display a string
 */
void uart_puts(char *s) {
  while(*s) {
    // convert newline to carrige return + newline
    if(*s=='\n')
      uart_send('\r');

    uart_send(*s++);
  }
}

/** Get string
 * @param n: maximum length of str[]
 * @param str: output of string
 * @param echo: print character received or not
 * @return Length of str (not tested yet)
*/
int uart_gets_n(int n, char *str, int echo){
  char temp;
  int index = 0;
  while(1){
    
    // Check for maximum length
    if(index >= n){
      str[n-1] = '\0';
      return n-1;
    }

    // Read a char and try to parse
    temp = uart_getc();
    if(temp == '\b' && index > 0){           // backspace implementation
        uart_printf("\b \b");
        index--;
        str[index] = '\0';
      }
    else if(temp != '\b' && temp != '\n'){
      if(echo) uart_send(temp); // echos
      str[index] = temp;
      index++;
    }
    else if(temp == '\n'){
      str[index] = '\0';
      uart_printf("\r\n");
      return index+1;
    }
  }
}

void _putchar(char character){
  uart_send(character);
}

void uart_rx_tx_handler() {
  int RX = (*AUX_MU_IIR & AUX_MU_IIR_RX_MASK);
  int TX = (*AUX_MU_IIR & AUX_MU_IIR_TX_MASK);

  // RX interrupt
  if(RX){
    // Should also check if the FIFO is full by checking (rx_buf_in+1) % RX_TX_BUF_SIZE == rx_buf_out
    // But I'm lazy
    // FIFO insert
    rx_buf[rx_buf_in++] = (char)(*AUX_MU_IO); // read 1 byte from peripheral
    if(rx_buf_in >= RX_TX_BUF_SIZE)
      rx_buf_in = 0;
  }
  // TX interrupt
  else if(TX){
    // Transmit completes, FIFO empty
    if(tx_buf_out == tx_buf_in){
      // Do not raise an interrupt when tx is ready
      _disable_tx_interrupt();
      return;
    }
    else{
      // Write to hardware peripheral buffer
      *AUX_MU_IO = tx_buf[tx_buf_out++];
      if(tx_buf_out >= RX_TX_BUF_SIZE)
        tx_buf_out = 0;

      // Tell cpu to raise an interrupt once it's ready to send a byte, i.e., no pending byte to be send
      _enable_tx_interrupt();
    }
  }
  else{
    uart_puts("uart_rx_tx_handler: should not got here.\r\n");
    while (1);
  }
}

/** Get string asynchronously, return -1 if no newline received yet.
 * You should polling this function in while(1)
 * @param n: maximum length of str[]
 * @param str: output of string
 * @param echo: print character received or not
 * @return Length of str, -1 if no newline received yet.
*/
int uart_gets_n_async(int n, char *str, int echo){
  static int index = 0;
  int temp;
  // Check for maximum length
  if(index >= n){
    str[n-1] = '\0';
    index = 0;
    return n-1;
  }

  // Read a char and try to parse
  temp = uart_getc_async();

  if(temp < 0)
    return -1;
  else if(temp == '\b' && index > 0){           // backspace implementation
    uart_printf("\b \b");
    index--;
    str[index] = '\0';
  }
  else if(temp != '\b' && temp != '\n' && temp != '\r'){
    if(echo) uart_send(temp); // echos
    str[index] = temp;
    index++;
  }
  else if(temp == '\n' || temp == '\r'){
    str[index] = '\0';
    uart_printf("\r\n");
    int index_copy = index;
    index = 0;
    return index_copy+1;
  }

  return -1;
}

// Get a char from FIFO, return -1 if FIFO is empty
int uart_getc_async(){
  _enable_rx_interrupt();
  // Return if FIFO is empty
  if(rx_buf_in == rx_buf_out)
    return -1;

  _disable_rx_interrupt(); // prevent modification on rx_buf[], critical section
  char c = rx_buf[rx_buf_out++]; // read from FIFO
  if(rx_buf_out >= RX_TX_BUF_SIZE)
    rx_buf_out = 0;
  _enable_rx_interrupt();
  return c;
}

void uart_puts_async(char *str) {

  // Blocking wait if FIFO is full
  while( (tx_buf_in+1) % RX_TX_BUF_SIZE == tx_buf_out)
    _enable_tx_interrupt();

  _disable_tx_interrupt(); // prevent modification on tx_buf[], critical section
  // Insert the string into tx FIFO buffer
  for(int i = 0; str[i]; i++) {
    tx_buf[tx_buf_in++] = str[i];
    if(tx_buf_in == RX_TX_BUF_SIZE)
      tx_buf_in = 0;
  }
  // Tell cpu to raise an interrupt once it's ready to send a byte
  _enable_tx_interrupt();
}
