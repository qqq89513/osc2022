#include "thread.h"
#include "diy_malloc.h"
#include "sys_reg.h"
#include "uart.h"

#define DEFAULT_THREAD_SIZE 4096 // 4kB, this includes the size of a stack and the thread's TCB
#define PID_KERNEL_MAIN 0
#define PID_IDLE        1

extern void switch_to(thread_t *curr, thread_t *next); // defined in start.S

static int pid_count = PID_KERNEL_MAIN;        // 0 for main() from kernel, who has no parent thread
static thread_t *queue_head = NULL;
static thread_t *queue_tail = NULL;

static void Rqueue_insert_tail(thread_t *thd){
  if(thd == queue_tail){
    uart_printf("Exception, in Rqueue_insert_tail(), thd == queue_tail, pid=%d\r\n", thd->pid);
    return;
  }
  // First thread in queue
  if(queue_head == NULL && queue_tail == NULL){
    queue_head = thd;
    queue_tail = thd;
  }
  // Insert thread in tail of queue
  else{
    thd->next = NULL;
    queue_tail->next = thd;
    queue_tail = thd; // update tail
  }
}
static thread_t *Rqueue_pop_head(){
  thread_t *thd_return = NULL;

  if(queue_head == NULL){
    thd_return = NULL;
  }
  else{
    thd_return = queue_head;
    queue_head = queue_head->next;
    if(queue_head == NULL)
      queue_tail = NULL;
  }
  return thd_return;  
}

void idle(){
  thread_t *thd_now = NULL;
  // Error check, early returns
  thd_now = thread_get_current();
  if(thd_now != NULL){                  // idle() should be only called from kernel main
    uart_printf("Exeception, in idle(), idle() is not invoked from kernel main, ");
    uart_printf("schedule() might be called before idle() is called. pid=%d\r\n", thd_now->pid);
    return;
  }
  else if(queue_head == NULL){          // maybe thread_init() is not called
    uart_printf("Exeception, in idle(), queue_head is NULL\r\n");
    return;
  }
  else if(queue_head->pid != PID_IDLE){
    uart_printf("Exeception, in idle(), queue_head->pid=%d, should be PID_IDLE=%d\r\n", queue_head->pid, PID_IDLE);
    return;
  }
  
  thd_now = Rqueue_pop_head();      // make it as if current running thread is idle()
  write_sysreg(tpidr_el1, thd_now);
  while(1){
    // TODO: Kill exited (zombie)
    // TODO: Make shell here
    uart_printf("Entered idle\r\n");
    schedule();
  }
}

void thread_init(){
  pid_count = PID_IDLE;
  thread_create(idle, KERNEL);
}

/** Get the pointer of structure thread_t of current thread.
 * This function is only available in el1. Invoking in el0 raises synchornous exeception.
 * @return pointer of structure thread_t of current thread
 */
thread_t *thread_get_current(){
  uint64_t value = read_sysreg(tpidr_el1);
  return (thread_t*) value;
}

thread_t *thread_create(void *func, enum task_exeception_level mode){
  if(pid_count == PID_KERNEL_MAIN){
    uart_printf("Error, in thread_create(), failed to create thread, please call thread_init() first.\r\n");
    return NULL;
  }

  void *stack_start = NULL;
  void *space_addr = NULL;
  thread_t *thd_new = NULL;
  thread_t *thd_parent = thread_get_current();
  space_addr = diy_malloc(DEFAULT_THREAD_SIZE);  // should check return value of diy_malloc()
  thd_new = space_addr;
  stack_start = space_addr + DEFAULT_THREAD_SIZE - 1;
  stack_start = (thread_t*)(  (uint64_t)stack_start - ((uint64_t)stack_start % 8)  ); // round down to multiple of 8
  
  thd_new->fp = (uint64_t) stack_start;
  thd_new->sp = (uint64_t) stack_start;
  thd_new->lr = (uint64_t) func;  // jump to address stored in lr whenever this task
  thd_new->allocated_addr = space_addr;
  thd_new->target_func = func;
  thd_new->mode = mode;
  thd_new->state = WAIT_TO_RUN;
  thd_new->pid = pid_count;
  if(thd_parent != NULL)   thd_new->ppid = 0;               // Thread created from other thread
  else                     thd_new->ppid = thd_parent->pid; // Thread created from main() from kernel

  Rqueue_insert_tail(thd_new);
  pid_count++;
  return thd_new;
}

void schedule(){

  thread_t *thd_now = thread_get_current();
  thread_t *thd_next = Rqueue_pop_head();

  // Error check, early returns
  if(thd_next == NULL && thd_now->pid != 1){
    uart_printf("Exception, queue empty but current thread is not idle(), current thread pid=%d\r\n", thd_now->pid);
    return;
  }
  else if(thd_now == thd_next){
    uart_printf("Exception, thd_now == thd_next, pid=%d. Rqueue_dump():\r\n", thd_now->pid);
    Rqueue_dump();
    return;
  }

  // The queue is empty, and the current thread is idle(), return directly
  if(thd_next == NULL && thd_now->pid == 1)
    return;

  // TODO: Check state to determine to insert thd_now back to queue or not
  Rqueue_insert_tail(thd_now);
  switch_to(thd_now, thd_next);
}

void Rqueue_dump(){
  thread_t *thd = queue_head;
  while(thd != NULL){
    uart_printf("ppid=%d, pid=%d, state=%d, mode=%d, target_func=%p, allocated_addr=%p\r\n",
      thd->ppid, thd->pid, thd->state, thd->mode, thd->target_func, thd->allocated_addr);
    thd = thd->next;
  }
}
