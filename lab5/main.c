
#include "uart.h"
#include "mbox.h"
#include "general.h"
#include "diy_sscanf.h"
#include "diy_string.h"
#include "cpio.h"
#include "diy_malloc.h"
#include "fdtb.h"
#include "thread.h"
#include "timer.h"
#include "sys_reg.h"
#include "system_call.h"
#include <stdint.h>

#define MACHINE_NAME "rpi5-baremetal-lab5$ "
#define CMD_HELP     "help"
#define CMD_HELLO    "hello"
#define CMD_REBOOT   "reboot"
#define CMD_LS       "ls"
#define CMD_CAT      "cat"
#define CMD_LSDEV    "lsdev"
#define CMD_ALLOCATE_PAGE "ap"
#define CMD_FREE_PAGE     "fp"
#define CMD_MALLOC        "m"
#define CMD_FREE          "f"
#define CMD_DUMP_PAGE     "dump_page"
#define CMD_DUMP_RQ       "dump_rq"
#define CMD_EXEC          "exec"

#define ADDR_IMAGE_START 0x80000

void general_exception_handler(uint64_t cause, trap_frame *tf);

static void sys_init(void *dtb_addr);
static int spilt_strings(char** str_arr, char* str, char* deli);
static void fork_test();
static void foo();
static void shell();
static void irq_handler();
static void mailbox_test();
extern uint64_t __image_start, __image_end;
extern uint64_t __stack_start, __stack_end;
void main(void *dtb_addr)
{

  sys_init(dtb_addr);
  EL1_ARM_INTERRUPT_ENABLE();

  // say hello
  uart_printf("\r\n\r\n");
  uart_printf("Welcome------------------------ lab 5\r\n");

  uart_printf("dtb_addr=0x%p, __image_start=%p, __image_end=%p\r\n", dtb_addr, &__image_start, &__image_end);

  thread_init();
  thread_create(shell, USER);
  thread_create(foo, USER);
  r_q_dump();
  start_scheduling();
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

static void sys_init(void *dtb_addr){
  
  // IO init
  uart_init();

  // Device tree parse
  const long int dtb_size = fdtb_parse(dtb_addr, 0, cpio_parse);

  // Memory init
  uint32_t *mem_start_addr = 0;
  uint32_t mem_size = 0;
  mbox_arm_mem_info(&mem_start_addr, &mem_size);
  alloc_page_preinit((uint64_t)mem_start_addr, (uint64_t)mem_start_addr + mem_size);
  mem_reserve(0x0, 0x1000);                                       // spin tables for multicore boot
  mem_reserve((uint64_t)&__image_start, (uint64_t)&__image_end);  // kernel image
  mem_reserve((uint64_t)&__stack_end, (uint64_t)&__stack_start);  // stack, grows downward, so range is from end to start
  mem_reserve(0x8000000, 0x8000000 + 247296);                     // initramfs, hard coded
  mem_reserve((uint64_t)dtb_addr, (uint64_t)dtb_addr + dtb_size); // device tree
  alloc_page_init();

  // Timer init for Lab5, basic 2, Video Player
  uint64_t tmp;
  asm volatile("mrs %0, cntkctl_el1" : "=r"(tmp));
  tmp |= 1;
  asm volatile("msr cntkctl_el1, %0" : : "r"(tmp));
}

void general_exception_handler(uint64_t cause, trap_frame *tf){
  // Enter critical section
  EL1_ARM_INTERRUPT_DISABLE();

  // thread_t *thd = thread_get_current();
  // uart_printf("In exception handler, pid=%d, cause=%lu, elr_el1=0x%lX\r\n", thd->pid, cause, tf->elr_el1);
  switch(cause){
    // synchornous (svc)
    case 5:  case 9:
      system_call(tf);  // do not schedule after system calls
      break;
    
    // IRQ
    case 6:  case 10:
      irq_handler();
      EL1_ARM_INTERRUPT_ENABLE();
      schedule();
      break;
    
    case  1: case  2: case  3: case  4:
    case  7: case  8: case 11: case 12:
    case 13: case 14: case 15: case 16:
    default:
      uart_printf("spsr_el1 = 0x%08lX, elr_el1 = 0x%08lX, esr_el1 = 0x%08lX, cause = %lu\r\n",
        tf->spsr_el1, tf->elr_el1, tf->esr_el1, cause);
      uart_printf("Above exception unhandled\r\n");
  }

  // Exit critical section
  EL1_ARM_INTERRUPT_ENABLE();
  // schedule();
}

static void irq_handler(){
  // uart interrupt fired
  if(*IRQS1_PENDING & AUX_INT){
    uart_printf("Exception, uart interrtupt not enabled but fired\r\n");
  }

  // arm core 0 timer interrupt fired
  else if(*CORE0_IRQ_SOURCE & COREx_IRQ_SOURCE_CNTPNSIRQ_MASK){
    // only available in el1
    // unsigned long cntpct = read_sysreg(cntpct_el0);
	  unsigned long cntfrq = read_sysreg(cntfrq_el0);
    // uart_printf("ticks=%ld, freq=%ld, time elapsed=%ldms\r\n", 
    //   cntpct, cntfrq, (cntpct*1000) / cntfrq);
    write_sysreg(cntp_tval_el0, cntfrq >> 5); // set next tick to 1/32 second, which is, time slice for round robin
  }

  // Unknown interrupt fired
  else{
    uart_printf("Unknown general interrupt fired, IRQS1_PENDING=0x%08X, CORE0_IRQ_SOURCE=0x%08X, in c_irq_el1h_ex_handler().\r\n",
      *IRQS1_PENDING, *CORE0_IRQ_SOURCE);
    uart_printf("Blocking in while(1) now...\r\n");
    while(1);
  }
}

static void foo(){
  int pid = sysc_getpid();
  while(1){
    uint64_t tk;
    uart_printf("foo(), pid=%d, in background", pid);
    sysc_uart_write("\r\n", 2);
    WAIT_TICKS(tk, 1500000000);
  }
  
  if(pid == 7)
    mailbox_test();
  // lr == foo()
  for(int i=0; i<10; ++i) {    
    uart_printf("pid = %d, i=%d\r\n", pid, i);
    uint64_t tk;
    WAIT_TICKS(tk, 50000000);
    // schedule(); // with timer enabled, thread doesn't need to call schedule(), exeception handler forced interrupted thread swap out
  }

  // Test of sysc_kill(): thread 3 kills thread 4 and 5
  if(pid == 3){
    // if timer is enabled, than the order of threading might not be deterministic,
    // so thread 4 or 5 might exited before thread 3, resulting in failing og sysc_kill(4) or sysc_kill(5)
    if(sysc_kill(4) != 0)  uart_printf("Error, no pid %d found in run queue, failed to sysc_kill().\r\n", 4);
    if(sysc_kill(5) != 0)  uart_printf("Error, no pid %d found in run queue, failed to sysc_kill().\r\n", 5);
  }
  uart_printf("pid %d exiting\r\n", pid);  // killed thread will not reach here
  sysc_exit(0);
}

static void mailbox_test(){
  static volatile uint32_t  __attribute__((aligned(16))) mbox_buf[36];
  uint32_t *mem_start_addr = 0;
  uint32_t mem_size = 0;
  int ret = 0;
  int pid = sysc_getpid();
  uart_printf("pid = %d, entering mailbox test\r\n", pid);

  mbox_buf[0] = 8 * 4;                  // buffer size in bytes, 8 for 8 elements to MBOX_END_TAG; 4 for each elements is 4 bytes (u32)
  mbox_buf[1] = 0x00000000;             // MBOX_REQUEST_CODE;      // fixed code
  // tags begin
  mbox_buf[2] = 0x00010005;             // tag identifier
  mbox_buf[3] = 8;                      // response length
  mbox_buf[4] = 0x00000000;             // fixed code
  mbox_buf[5] = 0;                      // output buffer 0, clear it here
  mbox_buf[6] = 0;                      // output buffer 1, clear it here
  // tags end
  mbox_buf[7] = 0x00000000;

  // Send mailbox request via system call
  ret = sysc_mbox_call(MBOX_CH_PROP, (uint32_t*)mbox_buf); // message passing procedure call
  mem_start_addr = (uint32_t*)((uint64_t)mbox_buf[5]); // cast to u64 cuz uint32_t* takes 64 bits
  mem_size = mbox_buf[6];
  uart_printf("pid = %d, mbox ret=%d\r\n", pid, ret);
  uart_printf("pid = %d, mem_start_addr=0x%p\r\n", pid, mem_start_addr);
  uart_printf("pid = %d, mem_size=0x%08X\r\n", pid, mem_size);

  uart_printf("pid = %d, exitting mailbox test\r\n", pid);
}

static void fork_test(){
  uart_printf("\nFork Test, pid %d\n", sysc_getpid());
  uint64_t tk = 0;
  int cnt = 1;
  int ret = 0;
  ret = sysc_fork();
  if(ret == 0) { // child
    uint64_t cur_sp;
    asm volatile("mov %0, sp" : "=r"(cur_sp));
    uart_printf("first child pid=%d, cnt=%d, &cnt=%lX, sp=%lX\n", sysc_getpid(), cnt, (uint64_t)&cnt, cur_sp);
    ++cnt;
    ret = sysc_fork();
    if(ret != 0){
      asm volatile("mov %0, sp" : "=r"(cur_sp));
      uart_printf("first child pid=%d, cnt=%d, &cnt=%lX, sp=%lX\n", sysc_getpid(), cnt, (uint64_t)&cnt, cur_sp);
    }
    else{
      while (cnt < 5) {
        asm volatile("mov %0, sp" : "=r"(cur_sp));
        uart_printf("second child pid=%d, cnt=%d, &cnt=%lX, sp=%lX\n", sysc_getpid(), cnt, (uint64_t)&cnt, cur_sp);
        WAIT_TICKS(tk, 100000000);
        ++cnt;
      }
    }
    sysc_exit(0);
  }
  else {
    uart_printf("parent here, pid %d, child %d\n", sysc_getpid(), ret);
  }

  sysc_exit(0);
}

static void shell(){
  char input_s[64];
  char *args[10];
  int args_cnt = 0;

  while(1) {

    // Read cmd
    uart_printf(MACHINE_NAME);
    uart_gets_n(32, input_s, 1);
    args_cnt = spilt_strings(args, input_s, " ");

    // Execute cmd
    if(strlen_(args[0]) > 0){
      if     (strcmp_(args[0], CMD_HELP) == 0){
        uart_printf(CMD_HELP   "\t\t: print this help menu\r\n");
        uart_printf(CMD_HELLO  "\t\t: print Hello World!\r\n");
        uart_printf(CMD_REBOOT "\t\t: reboot the device\r\n");
        uart_printf(CMD_LS     "\t\t: List files and dirs\r\n");
        uart_printf(CMD_CAT    "\t\t: Print file content\r\n");
        uart_printf(CMD_LSDEV  "\t\t: Print all the nodes and propperties parsed from dtb.\r\n");
        uart_printf(CMD_ALLOCATE_PAGE " <page count>\t: Allocate <page count> from heap.\r\n");
        uart_printf(CMD_FREE_PAGE " <page index>\t: Release <page index>.\r\n");
        uart_printf(CMD_DUMP_PAGE "\t: Dump the frame array and free block lists\r\n");
        uart_printf(CMD_MALLOC " <size>\t: Allocate memory, <size> in bytes\r\n");
        uart_printf(CMD_FREE " <addr>\t: Free memory, <addr> in hex without 0x\r\n");
        uart_printf(CMD_DUMP_RQ "\t\t: Dump run queue\r\n");
        uart_printf(CMD_EXEC " <file> \t: Reallocate the file (img) and jumps to it.\r\n");
      }
      else if(strcmp_(args[0], CMD_HELLO) == 0){
        uart_printf("Hello World!\r\n");
      }
      else if(strcmp_(args[0], CMD_REBOOT) == 0){
        uart_printf("Rebooting...\r\n");
        reset(1000);
        while(1);
      }
      else if(strcmp_(args[0], CMD_LS) == 0){
        cpio_ls();
      }
      else if(strcmp_(args[0], CMD_CAT) == 0){
        if(args_cnt > 1)
          cpio_cat(args[1]);
      }
      else if(strcmp_(args[0], CMD_LSDEV) == 0){
        // fdtb_parse(dtb_addr, 1, NULL);
      }
      else if(strcmp_(args[0], CMD_ALLOCATE_PAGE) == 0){
        if(args_cnt > 1){
          int page_cnt = 0;
          sscanf_(args[1], "%d", &page_cnt);
          const int page_index = alloc_page(page_cnt, 0);
          if(page_index > -1)   uart_printf("Allocated %d at page #%d\r\n", page_cnt, page_index);
          else                  uart_printf("Error, not enough of contiguious space for page count=%d\r\n", page_cnt);
        }
        else
          uart_printf("Usage: " CMD_ALLOCATE_PAGE " <page count>\t: Allocate <page count> from heap.\r\n");
      }
      else if(strcmp_(args[0], CMD_FREE_PAGE) == 0){
        if(args_cnt > 1){
          int page_index = 0;
          sscanf_(args[1], "%d", &page_index);
          free_page(page_index, 1);
        }
        else
          uart_printf("Usage: " CMD_FREE_PAGE " <page index>\t: Release <page index>.\r\n");
      }
      else if(strcmp_(args[0], CMD_DUMP_PAGE) == 0){
        // dump_the_frame_array();
        dump_the_frame_array();
        dupmp_frame_freelist_arr();
      }
      else if(strcmp_(args[0], CMD_MALLOC) == 0){
        if(args_cnt > 1){
          int size = 0;
          sscanf_(args[1], "%d", &size);
          const void* addr = diy_malloc(size);
          if(addr != NULL)   uart_printf("Allocated %d bytes at addr %p\r\n", size, addr);
          else               uart_printf("Failed to malloc %d bytes\r\n", size);
        }
        else
          uart_printf("Usage: " CMD_MALLOC " <size>: Allocate memory, <size> in bytes\r\n");
      }
      else if(strcmp_(args[0], CMD_FREE) == 0){
        if(args_cnt > 1){
          void *addr = NULL;
          sscanf_(args[1], "%p", &addr);
          diy_free(addr);
        }
        else
          uart_printf("Usage: " CMD_FREE " <addr>\t: Free memory, <addr> in hex without 0x\r\n");
      }
      else if(strcmp_(args[0], CMD_DUMP_RQ) == 0){
        uart_printf("Shell dump run queue:\r\n");
        r_q_dump();
      }
      else if(strcmp_(args[0], CMD_EXEC) == 0){
        if(args_cnt > 1){
          sysc_exec(args[1], NULL);
          // Should not get here
          uart_printf("sys_exec() failed, in shell(), should not get here.\r\n");
        }
        else
          uart_printf("Usage: " CMD_EXEC " <file>\r\n");
      }
      else
        uart_printf("Unknown cmd \"%s\".\r\n", input_s);
    }
  }
}
