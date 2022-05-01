/*
 * Copyright (C) 2018 bzt (bztsrc@github)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

.section ".text.boot"

.global _start
.global from_el1_to_el0
.global from_el1_to_el0_remote
.global switch_to
.global go_to_thread

_start:
    // read cpu id, stop slave cores
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbz     x1, 2f
    // cpu id > 0, stop
1:  wfe
    b       1b
2:  // cpu id == 0

    // DTB load address is kept in x0 by GPU. Save it somewhere else
    mov     x28, x0

    // set top of stack just before our code (stack grows to a lower address per AAPCS64)
    ldr     x1, =_start
    mov     sp, x1

    // Set privilege from el2 to el1
    bl      from_el2_to_el1                 // set exeception level from el2 to el1. Next instruction executes in el1
    bl      set_exception_vector_table_el1  // set look up vector table when exception raised, defined in vect_talbe_and_exeception_handler.S

    // clear bss
    ldr     x1, =_bss_start
    ldr     w2, =_bss_size
3:  cbz     w2, 4f
    str     xzr, [x1], #8
    sub     w2, w2, #1
    cbnz    w2, 3b

jump_to_main:
    // jump to C code, should not return
4:  mov     x0, x28    // Put dtb address back as a argument of main
    bl      main
    // for failsafe, halt this core too
    b       1b


/* 
  Set exception level from EL2 to EL1
  In default, rpi3 cpu runs in el2.
  Call this function to set exeception level from el2 to el1.
  1. Set hcr_el2 (Hypervisor Configuration Register)
      
  2. Set spsr_el2
        (D,A,I,F) at bit# (9,8,7,6)，EL1h = 0b101, result in 1111000101 = 0x3c5
        ref: https://kaiiiz.github.io/notes/nctu/osdi/lab3/exception-level-switch/
        ref: http://www.lujun.org.cn/?p=1676
  3. Set elr_el2 (Exception Link Register)
        This is the address after exception handled.

  4. Call eret
        eret does 2 things: (mov cpsr, spsr_el2) and jumps to elr_el2
*/
from_el2_to_el1:
    // make el0, el1 can use Floating point and Advanced SIMD
    // this fixes printf (printf use stdargs, which access register q0~q7, and those are SIMD registers)
    // ref: https://kaiiiz.github.io/notes/nctu/osdi/lab3/exception-level-switch/
    // set cpacr_el1.FPEN = 0b11, where FPEN is at bits [21:20]
    mov x1,        (0b11 << 20)
    msr cpacr_el1, x1
    
    mov x1,        (1 << 31)    // EL1 & EL0 uses aarch64
    msr hcr_el2,   x1
    mov x1,        0x3c5        // EL1h (SPSel = 1) with interrupt disabled
    msr spsr_el2,  x1           
    msr elr_el2,   lr
    mov x1,        sp
    msr sp_el1,    x1           // set stack of el1
    eret                        // return to EL1

from_el1_to_el0:
    mov x1,        0x000        // EL0t, all interrupts enabled, 0x3c0 for interrupts disabled
    msr spsr_el1,  x1
    // set elr_el1 to the user program’s address
    msr elr_el1,   lr           // return address to the caller
    // set the user program’s stack pointer to a proper position by setting sp_el0.
    mov x1,        sp
    msr sp_el0,    x1           // set stack of el0 as orinary stack pointer
    eret                        // jump to user code (jump to elr_el1)

/*  Change from el1 to el0 with destination spcified
    void from_el1_to_el0_remote(uint64_t args, uint64_t addr, uint64_t u_sp);
    @param args (x0): Value pass to remote user program
    @param addr (x1): address to the remote user program
    @param u_sp (x2): stack pointer value of remote user program
 */ 
from_el1_to_el0_remote:
    // set spsr_el1 to 0x3c0
    mov x3,        0x000        // EL0t, interrupt disabled
    msr spsr_el1,  x3
    // set elr_el1 to the user program’s address
    msr elr_el1,   x1           // return address to the caller
    // set the user program’s stack pointer to a proper position by setting sp_el0.
    msr sp_el0,    x2           // set stack of el0 as orinary stack pointer
    eret                        // jump to user code (jump to elr_el1)

switch_to:
  /** Context switch from curr thread to next thread. 
   * Save curr context to curr's own stack, update the sp to next's, and than jump to next->lr.
   * Since this is a function call, x0~x15 is saved (should be saved) by caller
   * This function is only available in el1. Invoking in el0 raises synchornous exeception.
   * void switch_to(thread_t *curr, thread_t *next);
   * @param curr (x0): 
   * @param next (x1): 
  */
  stp x19, x20, [x0, 16 * 0]  // curr->x19=x19, curr->x20=x20
  stp x21, x22, [x0, 16 * 1]
  stp x23, x24, [x0, 16 * 2]
  stp x25, x26, [x0, 16 * 3]
  stp x27, x28, [x0, 16 * 4]
  stp fp,  lr,  [x0, 16 * 5]
  mov x9,  sp                 // x9 is corruptible register, caller saved
  str x9,       [x0, 16 * 6]  // curr->sp = sp
  mrs x9,       spsr_el1            // for debug purpose
  str x9,       [x0, 16 * 6 + 8]    // for debug purpose
  mrs x9,       elr_el1             // for debug purpose
  str x9,       [x0, 16 * 7]        // for debug purpose
  mrs x9,       esr_el1             // for debug purpose
  str x9,       [x0, 16 * 7 + 8]    // for debug purpose

  ldp x19, x20, [x1, 16 * 0]  // x19=next->x19=, x20=next->x20
  ldp x21, x22, [x1, 16 * 1]
  ldp x23, x24, [x1, 16 * 2]
  ldp x25, x26, [x1, 16 * 3]
  ldp x27, x28, [x1, 16 * 4]
  ldp fp,  lr,  [x1, 16 * 5]
  ldr x9,       [x1, 16 * 6]  // sp = next->sp
  mov sp,  x9
  ldr x9,       [x1, 16 * 6 + 8]    // for debug purpose
  msr spsr_el1, x9                  // for debug purpose
  ldr x9,       [x1, 16 * 7]        // for debug purpose
  msr elr_el1,  x9                  // for debug purpose
  ldr x9,       [x1, 16 * 7 + 8]    // for debug purpose
  msr esr_el1,    x9
  msr tpidr_el1, x1           // tpidr_el1 = (void*) next, update current thread
  ret

go_to_thread:
  ldp x19, x20, [x0, 16 * 0]
  ldp x21, x22, [x0, 16 * 1]
  ldp x23, x24, [x0, 16 * 2]
  ldp x25, x26, [x0, 16 * 3]
  ldp x27, x28, [x0, 16 * 4]
  ldp fp,  lr,  [x0, 16 * 5]
  ldr x9,       [x0, 16 * 6]
  mov sp,  x9
  msr tpidr_el1, x0
  ret
