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
.global core_timer_state

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


/*  Turn on and off core timer interrupt
    void core_timer_state(uint64_t state);
    @param state (x0): 0 for disable, 1 for enable
 */
core_timer_state:
    msr cntp_ctl_el0,  x0                     // set on or off
    mov x0,            2
    ldr x1,            =0x40000040            // 0x40000040 is CORE0_TIMER_IRQ_CTRL, Address: 0x4000_0040 Core 0 Timers interrupt control, ref: https://datasheets.raspberrypi.com/bcm2836/bcm2836-peripherals.pdf
    str w0,            [x1]                   // unmask timer interrupt, i.e. enable core timer interrupt
    ret

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
    mov x3,        0x3c0        // EL0t, interrupt disabled
    msr spsr_el1,  x3
    // set elr_el1 to the user program’s address
    msr elr_el1,   x1           // return address to the caller
    // set the user program’s stack pointer to a proper position by setting sp_el0.
    msr sp_el0,    x2           // set stack of el0 as orinary stack pointer
    eret                        // jump to user code (jump to elr_el1)
