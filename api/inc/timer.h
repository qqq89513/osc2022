#ifndef __TIMER_H_
#define __TIMER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <general.h>
#include <stdint.h>
#define CORE0_TIMER_IRQ_CTRL (VM_KERNEL_PREFIX | 0x40000040) // Address: 0x4000_0040 Core 0 Timers interrupt control, ref: https://datasheets.raspberrypi.com/bcm2836/bcm2836-peripherals.pdf

// User can declare function as int callback1 (uint64_t);
typedef int (timer_callback) (uint64_t); // function of ( int timer_callback(uint64_t data); )

// Timer ready queue
typedef struct __t_queue{
  timer_callback *func;     // function to execute
  uint64_t arg;             // argument to *func, call it like (*(t_queue_ll.func)) (arg)
  char msg[32];             // messages to print when it's schduled
  uint64_t sch_at;          // schduled at the time ticks
  struct __t_queue* next;
} t_queue_ll; // ll for linked list

void timer_add(timer_callback *callback, uint64_t callback_arg, char *msg, uint64_t after);
void core_timer_state(uint64_t state);
void timer_dequeue();
void timer_queue_traversal();

#ifdef __cplusplus
}
#endif
#endif  // __TIMER_H_