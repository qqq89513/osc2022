
#ifndef __THREAD_H_
#define __THREAD_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DEFAULT_THREAD_SIZE 4096 // 4kB, this includes the size of a stack and the thread's TCB

enum task_state {
  RUNNNING=1,
  WAIT_TO_RUN,
  EXITED,   // zombie, waiting to be cleaned
  CLEANED   
};
enum task_exeception_level {
  USER=0,
  KERNEL=1
};

typedef struct thread_t {
  uint64_t x19;           // keep by switch_to()
  uint64_t x20;           // keep by switch_to()
  uint64_t x21;           // keep by switch_to()
  uint64_t x22;           // keep by switch_to()
  uint64_t x23;           // keep by switch_to()
  uint64_t x24;           // keep by switch_to()
  uint64_t x25;           // keep by switch_to()
  uint64_t x26;           // keep by switch_to()
  uint64_t x27;           // keep by switch_to()
  uint64_t x28;           // keep by switch_to()
  uint64_t fp;  // x29    // keep by switch_to()
  uint64_t lr;  // x30    // keep by switch_to()
  uint64_t sp;            // keep by switch_to()
  uint64_t spsr_el1;      // for debug purpose, probally works without it
  uint64_t elr_el1;       // for debug purpose, probally works without it
  uint64_t esr_el1;       // for debug purpose, probally works without it
  void *allocated_addr;   // the address returned from diy_malloc(), passed to diy_free()
  void *user_sp;          // for .state=USER
  void *user_space;       // for .state=USER
  int ppid;               // parent pid
  int pid;
  enum task_state state;
  enum task_exeception_level mode;
  void *target_func;
  struct thread_t *next;
} thread_t;

void idle();
void thread_init();
thread_t *thread_get_current();
thread_t *thread_create(void *func, enum task_exeception_level mode);
void start_scheduling();
void schedule();
void r_q_dump();
void exited_ll_dump();
void exit_call_by_syscall_only();
int kill_call_by_syscall_only(int pid);


#ifdef __cplusplus
}
#endif
#endif  // __THREAD_H_
