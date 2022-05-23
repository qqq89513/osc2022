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

  // Setup page table and enable MMU
  bl  vm_kernel_space
  ldr x2, =boot_rest   // indirect branch, if direct branch, it jumps to 0x80xxx, instead of 0xFFFF0000080xxx.
  br  x2

boot_rest:
  bl clear_bss

jump_to_main:
  // jump to C code, should not return
  mov     x0, x28    // Put dtb address back as a argument of main
  bl      main
  // for failsafe, halt this core too
  b       1b


clear_bss:
  ldr     x1, =_bss_start
  ldr     w2, =_bss_size
3:  
  cbz     w2, 4f
  str     xzr, [x1], #8
  sub     w2, w2, #1
  cbnz    w2, 3b
4:
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

vm_kernel_space:
  #define TCR_CONFIG_REGION_48bit (((64 - 48) << 16) | ((64 - 48) << 0)) // t1sz, t0sz, (64-48) bits should be all 1 or 0, for virtual address
  #define TCR_CONFIG_4KB          ((0b10 << 30) | (0b00 << 14))          // tg1, tg0, set granule 4kB and 4kB
  #define TCR_CONFIG_DEFAULT      (TCR_CONFIG_REGION_48bit | TCR_CONFIG_4KB)
  ldr x1, = TCR_CONFIG_DEFAULT
  msr tcr_el1, x1
  #define MAIR_DEVICE_nGnRnE      0b00000000
  #define MAIR_NORMAL_NOCACHE     0b01000100  // high 0100:Normal memory, Outer Non-cacheable; low 0100:Normal memory, Inner Non-cacheable
  #define MAIR_IDX_DEVICE_nGnRnE  0           // set for Attr0
  #define MAIR_IDX_NORMAL_NOCACHE 1           // set for Attr1
  ldr x1, =( \
    (MAIR_DEVICE_nGnRnE  << (MAIR_IDX_DEVICE_nGnRnE * 8)) | \
    (MAIR_NORMAL_NOCACHE << (MAIR_IDX_NORMAL_NOCACHE * 8)) \
  )
  msr mair_el1, x1

  #define PD_TABLE 0b11
  #define PD_BLOCK 0b01
  #define PD_ACCESS (1 << 10)
  #define BOOT_PGD_ATTR PD_TABLE
  #define BOOT_PUD_ATTR (PD_ACCESS | (MAIR_IDX_DEVICE_nGnRnE << 2) | PD_BLOCK)

  ldr x0, =pg_dir
  and x0, x0, 0x0000ffffffffffff  // pg_dir is from link.ld, remove 0xFFFF000000000000 to access phy address
  add x1, x0, 0x1000

  ldr x2, = BOOT_PGD_ATTR
  orr x2, x1, x2 // combine the physical address of next level page with attribute.
  str x2, [x0]

  ldr x2, = BOOT_PUD_ATTR
  mov x3, 0x00000000
  orr x3, x2, x3
  str x3, [x1]      // 1st 1GB mapped by the 1st entry of PUD
  mov x3, 0x40000000
  orr x3, x2, x3
  str x3, [x1, 8]   // 2nd 1GB mapped by the 2nd entry of PUD

  msr ttbr0_el1, x0 // load PGD to the bottom translation-based register.
  msr ttbr1_el1, x0 // also load PGD to the upper translation based register.

  mrs x2, sctlr_el1 // sctlr_el1 |= 1, enable MMU
  orr x2 , x2, 1
  msr sctlr_el1, x2
  ret