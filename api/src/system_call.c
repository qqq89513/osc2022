
#include "system_call.h"
#include "sys_reg.h"
#include "thread.h"
#include "uart.h"

#define SYSCALL_NUM_GETPID     0
#define SYSCALL_NUM_UART_READ  1
#define SYSCALL_NUM_UART_WRITE 2
#define SYSCALL_NUM_EXEC       3
#define SYSCALL_NUM_FORK       4
#define SYSCALL_NUM_EXIT       5
#define SYSCALL_NUM_MBOX_CALL  6
#define SYSCALL_NUM_KILL       7

static int    getpid_private();
static size_t uart_read_private(char buf[], size_t size);
static size_t uart_write_private(const char buf[], size_t size);
static int    exec_private(const char *name, char *const argv[]);
static int    fork_private();
static void   exit_private(int status);
static int    mbox_call_private(unsigned char ch, unsigned int *mbox);
static void   kill_private(int pid);


// Lab5, basic 2 description: The system call numbers given below would be stored in x8
void system_call(trap_frame *tf){
  uint64_t num = tf->x8;
  switch(num){
    case SYSCALL_NUM_GETPID:
      tf->x0 = getpid_private();
      break;
    case SYSCALL_NUM_UART_READ:
      tf->x0 = uart_read_private((char*)tf->x0, (size_t)tf->x1);
      break;
    case SYSCALL_NUM_UART_WRITE:
      tf->x0 = uart_write_private((char*)tf->x0, (size_t)tf->x1);
      break;
    case SYSCALL_NUM_EXEC:
      tf->x0 = exec_private((char*)tf->x0, (char **)tf->x1);
      break;
    case SYSCALL_NUM_FORK:
      tf->x0 = fork_private();
    case SYSCALL_NUM_EXIT:
      exit_private((int)tf->x0);
      break;
    case SYSCALL_NUM_MBOX_CALL:
      tf->x0 = mbox_call_private((unsigned char)tf->x0, (unsigned int*)tf->x1);
    case SYSCALL_NUM_KILL:
      kill_private(tf->x0);
    default:
      uart_printf("Exeception, unimplemented system call, num=%lu\r\n", num);
  }
}

int           getpid(){
  write_gen_reg(x8, SYSCALL_NUM_GETPID);
  asm volatile("svc 0");
  // trap frame in stack is modified in system_call(),
  // and than restored by load_all.
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    getpid_private(){
  thread_t *thd_now = thread_get_current();
  if(thd_now != NULL)
    return thd_now->pid;
  else
    return -1;
}

size_t        uart_read(char buf[], size_t size){
  return 0;
}
static size_t uart_read_private(char buf[], size_t size){
  return 0;
}

size_t        uart_write(const char buf[], size_t size){
  return 0;
}
static size_t uart_write_private(const char buf[], size_t size){
  return 0;
}

int           exec(const char *name, char *const argv[]){
  return 0;
}
static int    exec_private(const char *name, char *const argv[]){
  return 0;
}

int           fork(){
  return 0;
}
static int    fork_private(){
  return 0;
}

void          exit_(int status){

}
static void   exit_private(int status){

}

int           mbox_call_(unsigned char ch, unsigned int *mbox){
  return 0;
}
static int    mbox_call_private(unsigned char ch, unsigned int *mbox){
  return 0;
}

void          kill_(int pid){

}
static void   kill_private(int pid){

}
