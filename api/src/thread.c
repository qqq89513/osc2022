#include "thread.h"
#include "diy_malloc.h"
#include "sys_reg.h"
#include "uart.h"
#include "timer.h"

#define PID_KERNEL_MAIN 0
#define PID_IDLE        1

// defined in start.S
extern void switch_to(thread_t *curr, thread_t *next);
extern void go_to_thread(thread_t *next);
extern void from_el1_to_el0_remote(uint64_t args, uint64_t addr, uint64_t u_sp);

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
    thd->next = NULL;
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
  const uint64_t stack_grows = (uint64_t)thd->allocated_addr + DEFAULT_THREAD_SIZE - thd->sp;
  while(thd != NULL){
    uart_printf("ppid=%d, pid=%d, state=%d, mode=%d, target_func=%lX, ",
      thd->ppid, thd->pid, thd->state, thd->mode, (uint64_t)thd->target_func);
    uart_printf("allocated_addr=%lX, .sp=%lX, .user_sp=%lX, .stack_gorws=%lX, .elr_el1=%lX\r\n", 
      (uint64_t)thd->allocated_addr, thd->sp, (uint64_t)thd->user_sp, stack_grows, thd->elr_el1);
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
    uart_printf("cleaning pid %d\r\n", temp->pid);
    if(thd->mode == USER){
      if(thd->user_space != NULL)
        diy_free(thd->user_space);
      else
        uart_printf("Exception, in clean_exited(), thd.mode=USER but thd.user_space=NULL\r\n");
    }
    thd = thd->next;
    diy_free(temp);
  }
  exited_ll_head = NULL; // exited list is now empty
}
static void thread_go_to_el0(){
  thread_t *thd = thread_get_current();
  
  uart_printf("pid %d going to el0 now\r\n", thd->pid);
  from_el1_to_el0_remote(0, (uint64_t)thd->target_func, (uint64_t)thd->user_sp);
  uart_printf("Exeception, in thread_go_to_el0(), should not get here.\r\n");
}

void start_scheduling(){
  thread_t *thd = run_q_pop_head();
  
  // Error check, early returns
  if(thd == NULL){
    uart_printf("Exeception, in start_scheduling(), run_q_head is NULL. Maybe thread_init() is not called?\r\n");
    return;
  }
  else if(thd->pid != PID_IDLE){
    uart_printf("Exeception, in start_scheduling(), run_q_head->pid=%d, should be PID_IDLE=%d.\r\n", run_q_head->pid, PID_IDLE);
    return;
  }
  else if(thd->target_func != (void*) idle){
    uart_printf("Exeception, in start_scheduling(), run_q_head->target_func != idle. Maybe thread_create() is called before thread_init()\r\n");
    return;
  }
  
  // Make it as if current running thread is idle()
  thd->state = RUNNNING;

  // Set first timer to 0.5 sec and enable it
  unsigned long cntfrq = read_sysreg(cntfrq_el0);
  write_sysreg(cntp_tval_el0, cntfrq >> 1);
  core_timer_state(1);

  // Jumps to idle(), never return
  go_to_thread(thd);
  uart_printf("Exception, in start_scheduling(), should not get here\r\n");
}

void idle(){
  while(1){
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
  stack_start = (thread_t*)(  (uint64_t)stack_start - ((uint64_t)stack_start % 16)  ); // round down to multiple of 16
  
  thd_new->fp = (uint64_t) stack_start;
  thd_new->sp = (uint64_t) stack_start;
  // Jump to func directly or go to el0 first
  if(mode == KERNEL){
    thd_new->lr = (uint64_t) func;  // jump to address stored in lr whenever this task
    thd_new->user_space = 0;        // unsued
    thd_new->user_sp = 0;           // unsued
  }
  else{
    void *user_space = diy_malloc(DEFAULT_THREAD_SIZE);
    thd_new->lr = (uint64_t) thread_go_to_el0;
    thd_new->user_space = user_space;
    thd_new->user_sp = user_space + DEFAULT_THREAD_SIZE - 1;
    thd_new->user_sp = (void*)(  (uint64_t)thd_new->user_sp - ((uint64_t)thd_new->user_sp % 16)  ); // round down to multiple of 16
  }
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
