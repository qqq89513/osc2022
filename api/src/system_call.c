
#include "system_call.h"
#include "sys_reg.h"
#include "thread.h"
#include "uart.h"

// Lab5, basic 2 description: The system call numbers given below would be stored in x8
#define SYSCALL_NUM_GETPID     0
#define SYSCALL_NUM_UART_READ  1
#define SYSCALL_NUM_UART_WRITE 2
#define SYSCALL_NUM_EXEC       3
#define SYSCALL_NUM_FORK       4
#define SYSCALL_NUM_EXIT       5
#define SYSCALL_NUM_MBOX_CALL  6
#define SYSCALL_NUM_KILL       7

// Private functions, priv stands for private
static int    priv_getpid();
static size_t priv_uart_read(char buf[], size_t size);
static size_t priv_uart_write(const char buf[], size_t size);
static int    priv_exec(const char *name, char *const argv[]);
static int    priv_fork();
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
      tf->x0 = priv_exec((char*)tf->x0, (char **)tf->x1);
      break;
    case SYSCALL_NUM_FORK:
      tf->x0 = priv_fork();
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
      uart_printf("Exeception, unimplemented system call, num=%lu\r\n", num);
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
  return 0;
}
static size_t priv_uart_read(char buf[], size_t size){
  return 0;
}

size_t        sysc_uart_write(const char buf[], size_t size){
  return 0;
}
static size_t priv_uart_write(const char buf[], size_t size){
  return 0;
}

int           sysc_exec(const char *name, char *const argv[]){
  return 0;
}
static int    priv_exec(const char *name, char *const argv[]){
  return 0;
}

int           sysc_fork(){
  return 0;
}
static int    priv_fork(){
  return 0;
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
  return 0;
}
static int    priv_mbox_call(unsigned char ch, unsigned int *mbox){
  return 0;
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
