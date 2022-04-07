
#include "timer.h"

#include <stddef.h>
#include "diy_malloc.h"
#include "diy_string.h"
#include "sys_reg.h"
#include "uart.h"

static t_queue_ll *queue_head = NULL;

void timer_add(timer_callback *callback, uint64_t callback_arg, char *msg, uint64_t after){

  // Get CPU freq and current ticks
  uint64_t ticks_now = read_sysreg(cntpct_el0);
  uint64_t freq = read_sysreg(cntfrq_el0);

  t_queue_ll *task_to_insert = simple_malloc(sizeof(t_queue_ll));
  task_to_insert->func = callback;
  task_to_insert->arg = callback_arg;
  strcpy_(task_to_insert->msg, msg);
  task_to_insert->sch_at = ticks_now + after*freq;  // times freq to translate from seconds to ticks
  task_to_insert->next = NULL;

  // Insert task_to_insert to linked list
  if(queue_head == NULL){
    queue_head = task_to_insert;
  }
  else{
    t_queue_ll *task_cur = queue_head;  // current node in linked list traversal
    t_queue_ll *task_pre = NULL;        // previous node visited
    while(task_cur != NULL){
      // Insert if task_to_insert should be schduled earlier than task_cur
      if(task_to_insert->sch_at < task_cur->sch_at){
        // Insert before task_cur
        task_pre->next = task_to_insert;
        task_to_insert->next = task_cur;
        if(task_cur == queue_head){  // insert before head, i.e. task_to_insert becomes new head
          queue_head = task_to_insert;
        }
        break;
      }

      // Advance task_cur
      else{
        task_pre = task_cur;
        task_cur = task_cur->next;
      }

    }
    // Insert task_to_insert at the end
    if(task_cur == NULL){
      task_pre->next = task_to_insert;
      task_to_insert->next = NULL;
    }
  }

  // Set timer to the future time that queue_head is scheduled at
  const uint64_t ticks_after_now = queue_head->sch_at - ticks_now;
  write_sysreg(cntp_tval_el0, ticks_after_now);
  core_timer_state(1);
}

/** Turn on or off core timer interrupt
  @param state (x0): 0 for disable, 1 for enable
*/
void core_timer_state(uint64_t state){
  write_sysreg(cntp_ctl_el0, state);
  asm volatile("mov x0,     2");
  asm volatile("ldr x1,     =0x40000040");  // 0x40000040 is CORE0_TIMER_IRQ_CTRL, Address: 0x4000_0040 Core 0 Timers interrupt control, ref: https://datasheets.raspberrypi.com/bcm2836/bcm2836-peripherals.pdf
  asm volatile("str w0,            [x1]");  // unmask timer interrupt, i.e. enable core timer interrupt
}

void timer_dequeue(){

  if(queue_head != NULL){
    // Execute callback
    uart_printf("timer_dequeue: callback=%p, msg=%s, sch_at=%ld\r\n", queue_head->func, queue_head->msg, queue_head->sch_at);
    if(queue_head->func != NULL)
      (*(queue_head->func)) (queue_head->arg);

    // Update timer for next task
    queue_head = queue_head->next; // remove head from queue, should free head, but i'm lazy
    if(queue_head != NULL){
      // Set timer to the future time that queue_head is scheduled at
      uint64_t ticks_now = read_sysreg(cntpct_el0);
      const uint64_t ticks_after_now = queue_head->sch_at - ticks_now;
      write_sysreg(cntp_tval_el0, ticks_after_now);
    }
    else{   // no remaining tasks in queue
      queue_head = NULL;
      core_timer_state(0);
      return;
    }
  }
  else{
    uart_printf("Error, should not get here. in timer_dequeue(), line %d.\r\n", __LINE__);
    core_timer_state(0);
  }
}

void timer_queue_traversal(){
  t_queue_ll *task_cur = queue_head;  // current node in linked list traversal
  while(task_cur != NULL){
    uart_printf("sch_at=%ld, msg=%s\r\n", task_cur->sch_at, task_cur->msg);
    // Advance task_cur
    task_cur = task_cur->next;
  }
}

