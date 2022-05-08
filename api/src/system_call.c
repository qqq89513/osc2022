
#include "system_call.h"
#include "diy_malloc.h"
#include "cpio.h"
#include "sys_reg.h"
#include "thread.h"
#include "mbox.h"
#include "uart.h"
#include "general.h"

// Lab5, basic 2 description: The system call numbers given below would be stored in x8
#define SYSCALL_NUM_GETPID     0
#define SYSCALL_NUM_UART_READ  1
#define SYSCALL_NUM_UART_WRITE 2
#define SYSCALL_NUM_EXEC       3
#define SYSCALL_NUM_FORK       4
#define SYSCALL_NUM_EXIT       5
#define SYSCALL_NUM_MBOX_CALL  6
#define SYSCALL_NUM_KILL       7

extern void kid_thread_return_fork();   // defined in vect_table_and_execption_handler.S

// Private functions, priv stands for private
static int    priv_getpid();
static size_t priv_uart_read(char buf[], size_t size);
static size_t priv_uart_write(const char buf[], size_t size);
static int    priv_exec(const char *name, char *const argv[], trap_frame *tf);
static int    priv_fork(trap_frame *tf_mom);
static void   priv_exit(int status);
static int    priv_mbox_call(unsigned char ch, unsigned int *mbox);
static int    priv_kill(int pid);


/**
 * How to invoke a system call:
 *  1. Call public function, let's say sysc_kill()
 *  2. In sysc_kill(pid), we let x8=SYSCALL_NUM_KILL and x0=pid, and than call "svc 0"
 *  3. Exception triggered by "svc 0", exeception handler save current registers into stack as a trap frame
 *  4. Pass the trap frame pointer to exeception handler in c domain
 *  5. C domain exeception handler recognizes exeception triggered by "svc 0", pass the trap frame pointer to system_call()
 *  6. In system_call(), get system call number from x8 in trap frame, and then execute the corresponding private system call
 *  7. x0..7 in trap frame can be read as parameters passing to system call, x0 is the return value
 *  8. In this case, sysc_kill() has 1 passed in parameter, pid in x0, and 1 return value
*/
void system_call(trap_frame *tf){
  uint64_t num = tf->x8;
  thread_t *thd = NULL;
  switch(num){
    case SYSCALL_NUM_GETPID:
      tf->x0 = priv_getpid();
      break;
    case SYSCALL_NUM_UART_READ:
      tf->x0 = priv_uart_read((char*)tf->x0, (size_t)tf->x1);
      break;
    case SYSCALL_NUM_UART_WRITE:
      tf->x0 = priv_uart_write((char*)tf->x0, (size_t)tf->x1);
      break;
    case SYSCALL_NUM_EXEC:
      tf->x0 = priv_exec((char*)tf->x0, (char **)tf->x1, tf);
      break;
    case SYSCALL_NUM_FORK:
      tf->x0 = priv_fork(tf);
      break;
    case SYSCALL_NUM_EXIT:
      priv_exit((int)tf->x0);
      break;
    case SYSCALL_NUM_MBOX_CALL:
      tf->x0 = priv_mbox_call((unsigned char)tf->x0, (unsigned int*)tf->x1);
      break;
    case SYSCALL_NUM_KILL:
      tf->x0 = priv_kill(tf->x0);
      break;
    default:
      thd = thread_get_current();
      uart_printf("Exeception, unimplemented system call, num=%lu, pid=%d, elr_el1=%lX\r\n", num, thd->pid, thd->elr_el1);
  }
}

int           sysc_getpid(){
  write_gen_reg(x8, SYSCALL_NUM_GETPID);
  asm volatile("svc 0");
  // trap frame in stack is modified in system_call(),
  // and than restored by load_all.
  // by exeception handled, following is executed explicitly
  // ret_val = priv_getpid();
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_getpid(){
  thread_t *thd_now = thread_get_current();
  if(thd_now != NULL)
    return thd_now->pid;
  else
    return -1;
}

size_t        sysc_uart_read(char buf[], size_t size){
  write_gen_reg(x8, SYSCALL_NUM_UART_READ);
  write_gen_reg(x1, size);  // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, buf);   // so write to x0 should be the last one performed, maybe should change to register int var asm("x0");
  asm volatile("svc 0");
  size_t ret_val = read_gen_reg(x0);
  return ret_val;
}
static size_t priv_uart_read(char buf[], size_t size){
  const size_t s = size;
  EL1_ARM_INTERRUPT_ENABLE(); // since uart_read_byte() blocks is no input
  while(size != 0){
    *buf++ = (char)uart_read_byte();
    size--;
  }
  EL1_ARM_INTERRUPT_DISABLE();
  return s;
}

size_t        sysc_uart_write(const char buf[], size_t size){
  write_gen_reg(x8, SYSCALL_NUM_UART_WRITE);
  write_gen_reg(x1, size);  // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, buf);   // so write to x0 should be the last one performed
  asm volatile("svc 0");
  size_t ret_val = read_gen_reg(x0);
  return ret_val;
}
static size_t priv_uart_write(const char buf[], size_t size){
  const size_t s = size;
  while(size != 0){
    uart_send(*buf++);
    size--;
  }
  return s;
}

int           sysc_exec(const char *name, char *const argv[]){
  write_gen_reg(x8, SYSCALL_NUM_EXEC);
  write_gen_reg(x1, argv);    // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, name);    // so write to x0 should be the last one performed
  asm volatile("svc 0");      // should not return
  return 0;
}
static int    priv_exec(const char *name, char *const argv[], trap_frame *tf){
  thread_t *thd = thread_get_current();
  uint8_t *load_addr = NULL; // 64 pages
  if(thd->mode == KERNEL){
    uart_printf("sysc_exec() failed, current thread pid=%d, sysc_exec() for KERNEL thread is now unimplemented\r\n", thd->pid);
    return -1;
  }

  // Copy image to a dynamic allocated space
  load_addr = diy_malloc(64*4096);
  if(cpio_copy((char*)name, load_addr) != 0){
    uart_printf("sysc_exec() failed, failed to locate file %s.\r\n", name);
    return -1;
  }

  // Modify trap frame, 
  // so when returning from execeptio, eret sets sp to sp_el0 and jumps to elr_el12
  tf->elr_el1 = (uint64_t)load_addr;
  tf->sp_el0 = (uint64_t)thd->user_sp;
  return 0;
  // should call eret here, exec the program direcctly
  // should also clean the original thread
}

int           sysc_fork(){
  write_gen_reg(x8, SYSCALL_NUM_FORK);
  asm volatile("svc 0");
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_fork(trap_frame *tf_mom){
  thread_t *thd_mom = thread_get_current();
  thread_t *thd_kid = thread_create((void*)tf_mom->elr_el1, thd_mom->mode);
  thread_t thd_backup;  // backup for kid
  const uint8_t mom_higher = thd_mom > thd_kid;
  uint64_t offset = (uint64_t)( mom_higher ? ((uint64_t)thd_mom - (uint64_t)thd_kid) : ((uint64_t)thd_kid - (uint64_t)thd_mom) );
  uint64_t offset_tf = (uint64_t)tf_mom - (uint64_t)thd_mom;
  trap_frame *tf_kid = (trap_frame*)((uint64_t)thd_kid + offset_tf);
  uint8_t *copy_src = NULL, *copy_dest = NULL;

  /* When kid thread is scheduled, it 
      1. sp updated by switch_to, and than jumps to kid_thread_return_fork() by setting lr=kid_thread_return_fork() 
      2. In kid_thread_return_fork(), recovery context from sp. Note that sp now is the sp of kid thread
      3. eret to elr_el1, set sp to sp_el0 if returning back to el0
      4. Back to the instruction right after "svc 0"
      5. Get return value from system call, which is 0
  */

  // Copy mother's stack to kid's stack
  {
    // Backup kid
    thd_backup.allocated_addr = thd_kid->allocated_addr;
    thd_backup.user_space     = thd_kid->user_space;
    thd_backup.user_sp        = thd_kid->user_sp;
    thd_backup.pid            = thd_kid->pid;
    thd_backup.ppid           = thd_mom->pid;
    thd_backup.state          = thd_kid->state;
    thd_backup.next           = thd_kid->next;

    // Copy momther thread's entire stack and thread info
    copy_src  = (uint8_t*)thd_mom->allocated_addr;
    copy_dest = (uint8_t*)thd_kid->allocated_addr;
    for(size_t i=0; i<DEFAULT_THREAD_SIZE; i++) copy_dest[i] = copy_src[i];

    // Recover kid
    thd_kid->allocated_addr = thd_backup.allocated_addr;
    thd_kid->user_space     = thd_backup.user_space;
    thd_kid->user_sp        = thd_backup.user_sp;
    thd_kid->pid            = thd_backup.pid;
    thd_kid->ppid           = thd_backup.ppid;
    thd_kid->state          = thd_backup.state;
    thd_kid->next           = thd_backup.next;

    // Copy mother thread's user stack if it's a user thread
    if(thd_kid->mode == USER){
      copy_src  = (uint8_t*)thd_mom->user_space;
      copy_dest = (uint8_t*)thd_kid->user_space;
      for(size_t i=0; i<DEFAULT_THREAD_SIZE; i++) copy_dest[i] = copy_src[i];
    }
  }

  // Set kid thread sp and lr for switch_to
  thd_kid->sp = (uint64_t)tf_kid;   // the trap frame is stored on the stack, load_all recover context from stack
  thd_kid->lr = (uint64_t)kid_thread_return_fork; // jumps to kid_thread_return_fork() when it's first time scheduled

  // Copy mother's trap frame to kid's trap frame
  copy_src  = (uint8_t*)tf_mom;
  copy_dest = (uint8_t*)tf_kid;
  for(size_t i=0; i<sizeof(trap_frame); i++) copy_dest[i] = copy_src[i];

  // Set trap frame values which are different from mother thread's trap frame
  tf_kid->x0 = 0; // return value of fork() of kid thread is 0
  tf_kid->fp = mom_higher ? (tf_mom->fp - offset) : (tf_mom->fp + offset);
  tf_kid->lr = tf_mom->lr;
  if(tf_mom->sp_el0 == 0)
    tf_kid->sp_el0 = 0;
  else{
    const uint64_t offset_sp_el0 = tf_mom->sp_el0 - (uint64_t)thd_mom->user_space;
    tf_kid->sp_el0 = (uint64_t)thd_kid->user_space + offset_sp_el0;
  }

  return thd_kid->pid;
}

// Thread self terminate, status unimplmented
void          sysc_exit(int status){
  write_gen_reg(x8, SYSCALL_NUM_EXIT);
  write_gen_reg(x0, status); // pass parameter to priv_exit()
  asm volatile("svc 0");     // by exception handled, priv_exit(status) is called
  return;
}
static void   priv_exit(int status){
  exit_call_by_syscall_only();
}

int           sysc_mbox_call(unsigned char ch, unsigned int *mbox){
  write_gen_reg(x8, SYSCALL_NUM_MBOX_CALL);
  write_gen_reg(x1, mbox);  // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, ch);    // so write to x0 should be the last one performed
  asm volatile("svc 0");
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_mbox_call(unsigned char ch, unsigned int *mbox){
  return mbox_call_user_buffer(ch, mbox);
}

int           sysc_kill(int pid){
  write_gen_reg(x8, SYSCALL_NUM_KILL);
  write_gen_reg(x0, pid);
  asm volatile("svc 0");
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_kill(int pid){
  return kill_call_by_syscall_only(pid);
}
