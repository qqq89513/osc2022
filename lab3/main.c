
#include "uart.h"
#include "mbox.h"
#include "general.h"
#include "diy_sscanf.h"
#include "diy_string.h"
#include "cpio.h"
#include "diy_malloc.h"
#include "fdtb.h"
#include "sys_reg.h"
#include "timer.h"
#include <stdint.h>

#define MACHINE_NAME "rpi3-baremetal-lab3$ "
#define CMD_HELP     "help"
#define CMD_HELLO    "hello"
#define CMD_REBOOT   "reboot"
#define CMD_LSHW     "lshw"
#define CMD_LS       "ls"
#define CMD_CAT      "cat"
#define CMD_LSDEV    "lsdev"
#define CMD_EL0      "el0"
#define CMD_EXEC     "exec"
#define CMD_PTIMER   "ptimer"
#define CMD_setTimeout  "setTimeout"
#define CMD_ASYNC_PRINT "async_print"


// Externs
extern uint64_t _start;
extern void from_el1_to_el0(); // defined in start.S
extern void from_el1_to_el0_remote(uint64_t args, uint64_t addr, uint64_t u_sp); // defined in start.S

// Globals defined here
void dump_3_regs(uint64_t spsr_el1, uint64_t elr_el1, uint64_t esr_el1);
void c_irq_handler_timer(uint64_t ticks, uint64_t freq);
void c_irq_el1h_ex_handler();

// Locals
static void show_hardware_info();
static int spilt_strings(char** str_arr, char* str, char* deli);
static int exeception_level = 0;
static int periodic_print(uint64_t sec);

void main(void *dtb_addr)
{
  char *input_s;
  char **args;
  int args_cnt = 0;
  int gets_ret = -1;

  exeception_level = 1;

  input_s = simple_malloc(sizeof(char) * 32);
  args = simple_malloc(sizeof(char*) * 10);

  // Enable interrupt in el1
  EL1_ARM_INTERRUPT_ENABLE();

  // set up serial console
  uart_init();
  _enable_uart_interrupt(); // this only enables uart interrupt, you need to call _enable_tx_interrupt() or _enable_rx_interrupt

  // say hello
  uart_printf("\r\n\r\n");
  uart_printf("Welcome------------------------ lab 3\r\n");

  uart_printf("_start=0x%p, dtb_addr=0x%p\r\n", &_start, dtb_addr);

  fdtb_parse(dtb_addr, 0, cpio_parse);

  uart_puts_async(MACHINE_NAME);
  while(1) {

    // Read string asynchronously
    gets_ret = uart_gets_n_async(32, input_s, 1);

    // When a command with enter is received
    if(gets_ret != -1){
      // Spilt string into string array
      args_cnt = spilt_strings(args, input_s, " ");

      // Execute cmd
      if(strlen_(args[0]) > 0){
        if     (strcmp_(args[0], CMD_HELP) == 0){
          uart_printf(CMD_HELP   "\t\t: print this help menu\r\n");
          uart_printf(CMD_HELLO  "\t\t: print Hello World!\r\n");
          uart_printf(CMD_REBOOT "\t\t: reboot the device\r\n");
          uart_printf(CMD_LSHW   "\t\t: print hardware info acquired from mailbox\r\n");
          uart_printf(CMD_LS     "\t\t: List files and dirs\r\n");
          uart_printf(CMD_CAT    "\t\t: Print file content\r\n");
          uart_printf(CMD_LSDEV  "\t\t: Print all the nodes and propperties parsed from dtb.\r\n");
          uart_printf(CMD_EL0    "\t\t: Switch from exception level el1 to el0 (user mode)\r\n");
          uart_printf(CMD_EXEC " <file> \t: Switch to el0, reallocate the file (img) and jumps to it.\r\n");
          uart_printf(CMD_PTIMER  " <SECONDS>: Print ticks every SECONDS.\r\n");
          uart_printf(CMD_setTimeout " <MESSAGE> <SECONDS>: Print MESSAGE after SECOND.\r\n");
          uart_printf(CMD_ASYNC_PRINT "\t: Test printing asynchronously with tx interrupt.\r\n");
        }
        else if(strcmp_(args[0], CMD_HELLO) == 0){
          uart_printf("Hello World!\r\n");
        }
        else if(strcmp_(args[0], CMD_REBOOT) == 0){
          uart_printf("Rebooting...\r\n");
          reset(1000);
          while(1);
        }
        else if(strcmp_(args[0], CMD_LSHW) == 0){
          show_hardware_info();
        }
        else if(strcmp_(args[0], CMD_LS) == 0){
          cpio_ls();
        }
        else if(strcmp_(args[0], CMD_CAT) == 0){
          if(args_cnt > 1)
            cpio_cat(args[1]);
        }
        else if(strcmp_(args[0], CMD_LSDEV) == 0){
          fdtb_parse(dtb_addr, 1, NULL);
        }
        else if(strcmp_(args[0], CMD_EL0) == 0){
          from_el1_to_el0();
          exeception_level = 0;
          uart_printf("Now in el0 user mode.\r\n");
        }
        else if(strcmp_(args[0], CMD_EXEC) == 0){
          if(args_cnt > 1){
            // Reallocate file to 0x50000
            if(cpio_copy(args[1], (uint8_t*) 0x50000) == 0)
              from_el1_to_el0_remote((uint64_t)dtb_addr, 0x50000, 0x50000); // switch to el0 and jumps to 0x50000
          }
          else
            uart_printf("Usage: " CMD_EXEC " <file>\r\n");
        }
        else if(strcmp_(args[0], CMD_PTIMER) == 0){
          if(args_cnt == 2){
            uint64_t period = 0;
            sscanf_(args[1], "%d", &period);
            timer_add(periodic_print, period, "Periodic print", period);
            // from_el1_to_el0();
            // exeception_level = 0;
            // while(1);         // print ticks every 2 seconds
          }
          else
            uart_printf("Usage: " CMD_PTIMER " SECONDS\r\n");
        }
        else if(strcmp_(args[0], CMD_setTimeout) == 0){
          if(args_cnt == 3){
            char *msg = args[1];
            uint64_t time_after = 0;
            sscanf_(args[2], "%d", &time_after);
            timer_add(NULL, 0, msg, time_after);
          }
          else{
            uart_printf("Waiting queue:\r\n");
            timer_queue_traversal();
            uart_printf("Usage: " CMD_setTimeout " MESSAGE SECONDS\r\n");
          }
        }
        else if(strcmp_(args[0], CMD_ASYNC_PRINT) == 0){
          uart_puts_async("async send 1\r\n");
          uart_puts_async("async send 2\r\n");
          uart_puts_async("async send 3\r\n");
        }
        else
          uart_printf("Unknown cmd \"%s\".\r\n", input_s);
      }

      // Ready to take next command
      uart_puts_async(MACHINE_NAME);
    }
  }
}

void dump_3_regs(uint64_t spsr_el1, uint64_t elr_el1, uint64_t esr_el1){
  uart_printf("spsr_el1 = 0x%08lX, elr_el1 = 0x%08lX, esr_el1 = 0x%08lX\r\n",
    spsr_el1, elr_el1, esr_el1);
}

void c_irq_handler_timer(uint64_t ticks, uint64_t freq){
  uart_printf("ticks=%ld, freq=%ld, time elapsed=%ldms\r\n", 
    ticks, freq, (ticks*1000) / freq);
}

static int spilt_strings(char** str_arr, char* str, char* deli){
  int count = 0;
  // Spilt str by specified delimeter
  str_arr[0] = strtok_(str, deli);
  count = 0;
  while(str_arr[count] != NULL){
    count++;
    str_arr[count] = strtok_(NULL, deli);
  }
  return count;
}

static void show_hardware_info(){
  uint32_t board_rev=0;
  uint32_t *mem_start_addr = 0;
  uint32_t mem_size = 0;
  mbox_board_rev(&board_rev);
  mbox_arm_mem_info(&mem_start_addr, &mem_size);
  uart_printf("board_rev=0x%08X\r\n", board_rev);
  uart_printf("mem_start_addr=0x%p\r\n", mem_start_addr);
  uart_printf("mem_size=0x%08X\r\n", mem_size);
}

// Timer callback example
static int periodic_print(uint64_t sec){
  timer_add(periodic_print, sec, "Periodic print", sec);
  uart_printf("This is periodic_print callback.\r\n");
  return 0;
}

// General interrupt fired in el1
void c_irq_el1h_ex_handler(){
  // Enter critical section
  // EL1_ARM_INTERRUPT_DISABLE();  // this doesn't work with uart_puts_async(), don't know why

  // uart interrupt fired
  if(*IRQS1_PENDING & AUX_INT){
    uart_rx_tx_handler();
  }

  // arm core 0 timer interrupt fired
  else if(*CORE0_IRQ_SOURCE & COREx_IRQ_SOURCE_CNTPNSIRQ_MASK){
    // only available in el1
    // Dump time ticks
    unsigned long cntpct = read_sysreg(cntpct_el0);
	  unsigned long cntfrq = read_sysreg(cntfrq_el0);
    c_irq_handler_timer(cntpct, cntfrq);
    timer_dequeue();  // execute task in the head of the queue
  }

  // Unknown interrupt fired
  else{
    uart_printf("Unknown general interrupt fired, IRQS1_PENDING=0x%08X, CORE0_IRQ_SOURCE=0x%08X, in c_irq_el1h_ex_handler().\r\n",
      *IRQS1_PENDING, *CORE0_IRQ_SOURCE);
    uart_printf("Blocking in while(1) now...\r\n");
    while(1);
  }

  // Exit critical section
  // EL1_ARM_INTERRUPT_ENABLE();  // this doesn't work with uart_puts_async(), don't know why
}
