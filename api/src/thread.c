#include "thread.h"
#include "diy_malloc.h"
#include "sys_reg.h"
#include "uart.h"
#include "timer.h"

#define DEFAULT_THREAD_SIZE 4096 // 4kB, this includes the size of a stack and the thread's TCB
#define PID_KERNEL_MAIN 0
#define PID_IDLE        1


static int pid_count = PID_KERNEL_MAIN;        // 0 for main() from kernel, who has no parent thread
static thread_t *run_q_head = NULL;   // run queue, .state = WAIT_TO_RUN
static thread_t *run_q_tail = NULL;
static thread_t *exited_ll_head = NULL;   // exited linked list, .state = EXITED, waiting to be cleaned
static void run_q_insert_tail(thread_t *thd){
  if(thd == run_q_tail){
    uart_printf("Exception, in run_q_insert_tail(), thd == run_q_tail, pid=%d\r\n", thd->pid);
    return;
  }
  // First thread in queue
  if(run_q_head == NULL && run_q_tail == NULL){
    run_q_head = thd;
    run_q_tail = thd;
  }
  // Insert thread in tail of queue
  else{
    thd->next = NULL;
    run_q_tail->next = thd;
    run_q_tail = thd; // update tail
  }
}
static thread_t *run_q_pop_head(){
  thread_t *thd_return = NULL;

  if(run_q_head == NULL){
    thd_return = NULL;
  }
  else{
    thd_return = run_q_head;
    run_q_head = run_q_head->next;
    if(run_q_head == NULL)
      run_q_tail = NULL;
  }
  return thd_return;  
}
static void exited_ll_insert_head(thread_t *thd){
  if(exited_ll_head == NULL){
    exited_ll_head = thd;
    exited_ll_head->next = NULL;
  }
  else{
    thd->next = exited_ll_head;
    exited_ll_head = thd;
  }
}
static void threads_dump(thread_t *head){
  thread_t *thd = head;
  while(thd != NULL){
    uart_printf("ppid=%d, pid=%d, state=%d, mode=%d, target_func=%p, allocated_addr=%p\r\n",
      thd->ppid, thd->pid, thd->state, thd->mode, thd->target_func, thd->allocated_addr);
    thd = thd->next;
  }
}
static void clean_exited(){
  thread_t *thd = exited_ll_head;
  thread_t *temp = exited_ll_head;
  // clean all exited threads
  while(thd != NULL){
    temp = thd;
    thd->state = CLEANED; // redundant, since the space will be freed
    thd = thd->next;
    uart_printf("cleaning pid %d\r\n", temp->pid);
    diy_free(temp);
  }
  exited_ll_head = NULL; // exited list is now empty
}
static void switch_to(thread_t *curr, thread_t *next){
  /** Context switch from curr thread to next thread. 
   * Save curr context to curr's own stack, update the sp to next's, and than jump to next->lr.
   * Since this is a function call, x0~x15 is saved (should be saved) by caller
   * This function is only available in el1. Invoking in el0 raises synchornous exeception.
   * void switch_to(thread_t *curr, thread_t *next);
   * @param curr (x0): 
   * @param next (x1): 
  */ 
  asm volatile("stp x19, x20, [x0, 16 * 0]");  // curr->x19=x19, curr->x20=x20
  asm volatile("stp x21, x22, [x0, 16 * 1]");
  asm volatile("stp x23, x24, [x0, 16 * 2]");
  asm volatile("stp x25, x26, [x0, 16 * 3]");
  asm volatile("stp x27, x28, [x0, 16 * 4]");
  asm volatile("stp fp,  lr,  [x0, 16 * 5]");
  asm volatile("mov x9,  sp");                 // x9 is corruptible register, caller saved
  asm volatile("str x9,       [x0, 16 * 6]");  // curr->sp = sp
  asm volatile("ldp x19, x20, [x1, 16 * 0]");  // next->x19=x19, next->x20=x20
  asm volatile("ldp x21, x22, [x1, 16 * 1]");
  asm volatile("ldp x23, x24, [x1, 16 * 2]");
  asm volatile("ldp x25, x26, [x1, 16 * 3]");
  asm volatile("ldp x27, x28, [x1, 16 * 4]");
  asm volatile("ldp fp,  lr,  [x1, 16 * 5]");
  asm volatile("ldr x9,       [x1, 16 * 6]");  // next->sp = sp
  asm volatile("mov sp,  x9");
  asm volatile("msr tpidr_el1, x1");           // tpidr_el1 = (void*) next, update current thread
  // Implicit asm volatile("ret");             // jumpping to next->lr
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
  else if(run_q_head == NULL){          // maybe thread_init() is not called
    uart_printf("Exeception, in idle(), run_q_head is NULL\r\n");
    return;
  }
  else if(run_q_head->pid != PID_IDLE){
    uart_printf("Exeception, in idle(), run_q_head->pid=%d, should be PID_IDLE=%d\r\n", run_q_head->pid, PID_IDLE);
    return;
  }
  
  thd_now = run_q_pop_head();      // make it as if current running thread is idle()
  thd_now->state = RUNNNING;
  write_sysreg(tpidr_el1, thd_now);
  core_timer_state(1);
  while(1){
    // TODO: Kill exited (zombie)
    // TODO: Make shell here
    clean_exited();
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

  run_q_insert_tail(thd_new);
  pid_count++;
  return thd_new;
}

void schedule(){

  thread_t *thd_now = thread_get_current();
  thread_t *thd_next = run_q_pop_head();

  // Error check, early returns
  if(thd_next == NULL && thd_now->pid != 1){
    uart_printf("Exception, queue empty but current thread is not idle(), current thread pid=%d\r\n", thd_now->pid);
    return;
  }
  else if(thd_now == thd_next){
    uart_printf("Exception, thd_now == thd_next, pid=%d. r_q_dump():\r\n", thd_now->pid);
    r_q_dump();
    return;
  }

  // The queue is empty, and the current thread is idle(), return directly
  if(thd_next == NULL && thd_now->pid == 1)
    return;

  if(thd_now->state == RUNNNING){
    run_q_insert_tail(thd_now);
  }

  thd_now->state = WAIT_TO_RUN;
  thd_next->state = RUNNNING;
  switch_to(thd_now, thd_next);
}

void r_q_dump(){
  threads_dump(run_q_head);
}
void exited_ll_dump(){
  threads_dump(exited_ll_head);
}

void exit_call_by_syscall_only(){
  thread_t *thd = thread_get_current();
  // current thread is not in run queue, so no need to remove it from run queue
  thd->state = EXITED;
  exited_ll_insert_head(thd);
  schedule();
}

int kill_call_by_syscall_only(int pid){
  thread_t *thd = run_q_head;
  thread_t *prev = NULL;
  while(thd != NULL){
    if(thd->pid == pid)
      break;
    prev = thd;
    thd = thd->next;
  }

  if(thd == NULL){
    return -1;
  }

  thd->state = EXITED;
  if(prev != NULL)
    prev->next = thd->next;
  else
    run_q_head = thd->next;

  exited_ll_insert_head(thd);

  return 0;  
}
