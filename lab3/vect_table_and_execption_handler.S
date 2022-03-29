// save general registers to stack
.macro save_all
  sub sp, sp, 32 * 8
  stp x0, x1, [sp ,16 * 0]
  stp x2, x3, [sp ,16 * 1]
  stp x4, x5, [sp ,16 * 2]
  stp x6, x7, [sp ,16 * 3]
  stp x8, x9, [sp ,16 * 4]
  stp x10, x11, [sp ,16 * 5]
  stp x12, x13, [sp ,16 * 6]
  stp x14, x15, [sp ,16 * 7]
  stp x16, x17, [sp ,16 * 8]
  stp x18, x19, [sp ,16 * 9]
  stp x20, x21, [sp ,16 * 10]
  stp x22, x23, [sp ,16 * 11]
  stp x24, x25, [sp ,16 * 12]
  stp x26, x27, [sp ,16 * 13]
  stp x28, x29, [sp ,16 * 14]
  str x30, [sp, 16 * 15]
.endm

// load general registers from stack
.macro load_all
  ldp x0, x1, [sp ,16 * 0]
  ldp x2, x3, [sp ,16 * 1]
  ldp x4, x5, [sp ,16 * 2]
  ldp x6, x7, [sp ,16 * 3]
  ldp x8, x9, [sp ,16 * 4]
  ldp x10, x11, [sp ,16 * 5]
  ldp x12, x13, [sp ,16 * 6]
  ldp x14, x15, [sp ,16 * 7]
  ldp x16, x17, [sp ,16 * 8]
  ldp x18, x19, [sp ,16 * 9]
  ldp x20, x21, [sp ,16 * 10]
  ldp x22, x23, [sp ,16 * 11]
  ldp x24, x25, [sp ,16 * 12]
  ldp x26, x27, [sp ,16 * 13]
  ldp x28, x29, [sp ,16 * 14]
  ldr x30, [sp, 16 * 15]
  add sp, sp, 32 * 8
.endm

// Align 7 and branch to label
.macro    align7_b    label
    .align    7
    b    \label
.endm

.global exception_vector_table_el1
.global set_exception_vector_table_el1


.align 11 // vector table should be aligned to 0x800
exception_vector_table_el1:
  // branch to a handler function
  // entry size is 0x80, .align will pad 0 (0x01 << 7 == 0x80)
  // ref: https://developer.arm.com/documentation/102412/0100/The-vector-tables
  // code ref: https://github.com/13579and2468/osc2022/blob/lab3/lab3/src/entry.S

  //Exception from the current EL while using SP_EL0
  align7_b  sync_invalid_el1t_ex_handler        // Synchronous EL1t
  align7_b  irq_invalid_el1t_ex_handler         // IRQ EL1t
  align7_b  fiq_invalid_el1t_ex_handler         // FIQ EL1t
  align7_b  error_invalid_el1t_ex_handler       // Error EL1t
  
  //Exception from the current EL while using SP_ELx
  align7_b  sync_el1h_ex_handler                // Synchronous EL1h  
  align7_b  irq_el1h_ex_handler                 // IRQ EL1h  
  align7_b  fiq_invalid_el1h_ex_handler         // FIQ EL1h
  align7_b  error_invalid_el1h_ex_handler       // Error EL1h

  //Exception from a lower EL and at least one lower EL is AArch64
  align7_b  sync_el0_64_ex_handler              // Synchronous 64-bit EL0
  align7_b  irq_el0_64_ex_handler               // IRQ 64-bit EL0
  align7_b  fiq_invalid_el0_64_ex_handler       // FIQ 64-bit EL0
  align7_b  error_invalid_el0_64_ex_handler     // Error 64-bit EL0
      
  //Exception from a lower EL and at least all lower EL are AArch32
  align7_b  sync_invalid_el0_32_ex_handler      // Synchronous 32-bit EL0
  align7_b  irq_invalid_el0_32_ex_handler       // IRQ 32-bit EL0
  align7_b  fiq_invalid_el0_32_ex_handler       // FIQ 32-bit EL0
  align7_b  error_invalid_el0_32_ex_handler     // Error 32-bit EL0

// Set exception vector table for execeptions raised from el0
// After an exeception is raised, exeception level goes from el0 to el1
// So the exeception raised in el0 is handled in el1
set_exception_vector_table_el1:
  adr x1, exception_vector_table_el1
  msr vbar_el1, x1
  ret

unimplemented_ex_handler:
sync_invalid_el1t_ex_handler:
irq_invalid_el1t_ex_handler:
fiq_invalid_el1t_ex_handler:
error_invalid_el1t_ex_handler:
sync_el1h_ex_handler:
irq_el1h_ex_handler:
fiq_invalid_el1h_ex_handler:
error_invalid_el1h_ex_handler:
sync_el0_64_ex_handler:
irq_el0_64_ex_handler:
fiq_invalid_el0_64_ex_handler:
error_invalid_el0_64_ex_handler:
sync_invalid_el0_32_ex_handler:
irq_invalid_el0_32_ex_handler:
fiq_invalid_el0_32_ex_handler:
error_invalid_el0_32_ex_handler:
  save_all
  // TODO: print spsr_el1, elr_el1, esr_el1 instead of infinity loop
1: wfe
    mov x1, 0xdead
    b 1b // infinity loop  
  load_all
  eret

