
// This is a independent source file, should be executed in el0.
// It raised an exeception.
// Named .asm to prevent makefile confused with the main .S files

.section ".text"
.global _start
_start:
    mov x1, 0
    mov x2, 0xdead
1:
    add x1, x1, 1
    svc 0           // exeception raised here, jumpping to el1 exeception_handler in vect_table_and_execption_handler.S
                    // it saves pc+4 to elr_el1. So that after exeception is handled in el1, eret can jump back to here
    cmp x1, 5
    blt 1b
1:
    b 1b

