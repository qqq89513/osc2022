// save general registers to stack, refer to struct trap_frame
.macro save_all
  sub sp, sp, 16 * 19
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

  // For nested interrupt, i.e. exeception triggered during handling exeception
  mrs x0,  spsr_el1
  str x0,  [sp, 16 * 15 + 8]
  mrs x0,  elr_el1
  str x0,  [sp, 16 * 16]
  mrs x0,  esr_el1
  str x0,  [sp, 16 * 16 + 8]
  mrs x0,  sp_el0             // sp_el0 also needs to be saved cuz eret makes "mov sp, sp_el0"
  str x0,  [sp, 16 * 17]
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

  // For nested interrupt, i.e. exeception triggered during handling exeception
  ldr x0,         [sp, 16 * 15 + 8]
  msr spsr_el1,   x0
  ldr x0,         [sp, 16 * 16]
  msr elr_el1,    x0
  ldr x0,         [sp, 16 * 16 + 8]
  msr esr_el1,    x0
  ldr x0,         [sp, 16 * 17]
  msr sp_el0,     x0

  // Restore x0
  ldp x0, x1, [sp ,16 * 0]

  add sp, sp, 16 * 19
.endm

// Align 7 and branch to label
.macro    align7_b    label
    .align    7
    b    \label
.endm

.global exception_vector_table_el1
.global set_exception_vector_table_el1
.global kid_thread_return_fork


.align 11 // vector table should be aligned to 0x800
exception_vector_table_el1:
  // branch to a handler function
  // entry size is 0x80, .align will pad 0 (0x01 << 7 == 0x80)
  // ref: https://developer.arm.com/documentation/102412/0100/The-vector-tables
  // code ref: https://github.com/13579and2468/osc2022/blob/lab3/lab3/src/entry.S

  //Exception from the current EL while using SP_EL0
  align7_b  sync_invalid_el1t_ex_handler        // Synchronous EL1t         | cause: 1
  align7_b  irq_invalid_el1t_ex_handler         // IRQ EL1t                 | cause: 2
  align7_b  fiq_invalid_el1t_ex_handler         // FIQ EL1t                 | cause: 3
  align7_b  error_invalid_el1t_ex_handler       // Error EL1t               | cause: 4
  
  //Exception from the current EL while using SP_ELx
  align7_b  sync_el1h_ex_handler                // Synchronous EL1h         | cause: 5
  align7_b  irq_el1h_ex_handler                 // IRQ EL1h                 | cause: 6
  align7_b  fiq_invalid_el1h_ex_handler         // FIQ EL1h                 | cause: 7
  align7_b  error_invalid_el1h_ex_handler       // Error EL1h               | cause: 8

  //Exception from a lower EL and at least one lower EL is AArch64
  align7_b  sync_el0_64_ex_handler              // Synchronous 64-bit EL0   | cause: 9
  align7_b  irq_el0_64_ex_handler               // IRQ 64-bit EL0           | cause: 10
  align7_b  fiq_invalid_el0_64_ex_handler       // FIQ 64-bit EL0           | cause: 11
  align7_b  error_invalid_el0_64_ex_handler     // Error 64-bit EL0         | cause: 12
      
  //Exception from a lower EL and at least all lower EL are AArch32
  align7_b  sync_invalid_el0_32_ex_handler      // Synchronous 32-bit EL0   | cause: 13
  align7_b  irq_invalid_el0_32_ex_handler       // IRQ 32-bit EL0           | cause: 14
  align7_b  fiq_invalid_el0_32_ex_handler       // FIQ 32-bit EL0           | cause: 15
  align7_b  error_invalid_el0_32_ex_handler     // Error 32-bit EL0         | cause: 16
  .align 7

// Set exception vector table for execeptions raised from el0
// After an exeception is raised, exeception level goes from el0 to el1
// So the exeception raised in el0 is handled in el1
set_exception_vector_table_el1:
  adr x1,       exception_vector_table_el1
  msr vbar_el1, x1
  ret

sync_invalid_el1t_ex_handler:      // cause: 1
  save_all
  // void general_exception_handler(uint64_t cause, trap_frame *tf);
  // defined in main.c
  // mrs a, b: Write system coprocessor register to arm register a
  mov x0, 1
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
irq_invalid_el1t_ex_handler:       // cause: 2
  save_all
  mov x0, 2
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
fiq_invalid_el1t_ex_handler:       // cause: 3
  save_all
  mov x0, 3
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
error_invalid_el1t_ex_handler:     // cause: 4
  save_all
  mov x0, 4
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
sync_el1h_ex_handler:              // cause: 5
  save_all
  mov x0, 5
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
irq_el1h_ex_handler:               // cause: 6
  save_all
  mov x0, 6
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
fiq_invalid_el1h_ex_handler:       // cause: 7
  save_all
  mov x0, 7
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
error_invalid_el1h_ex_handler:     // cause: 8
  save_all
  mov x0, 8
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
sync_el0_64_ex_handler:            // cause: 9
  save_all
  mov x0, 9
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
irq_el0_64_ex_handler:             // cause: 10
  save_all
  mov x0, 10
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
fiq_invalid_el0_64_ex_handler:     // cause: 11
  save_all
  mov x0, 11
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
error_invalid_el0_64_ex_handler:   // cause: 12
  save_all
  mov x0, 12
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
sync_invalid_el0_32_ex_handler:    // cause: 13
  save_all
  mov x0, 13
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
irq_invalid_el0_32_ex_handler:     // cause: 14
  save_all
  mov x0, 14
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
fiq_invalid_el0_32_ex_handler:     // cause: 15
  save_all
  mov x0, 15
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret
error_invalid_el0_32_ex_handler:   // cause: 16
  mov x0, 16
  mov x1, sp
  bl  general_exception_handler
  load_all
  eret

kid_thread_return_fork:
  load_all
  eret
