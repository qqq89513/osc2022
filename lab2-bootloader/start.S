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

_start:
    // read cpu id, stop slave cores
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbz     x1, 2f
    // cpu id > 0, stop
1:  wfe
    b       1b
2:  // cpu id == 0

    // clear bss
    ldr     x1, =_bss_start
    ldr     w2, =_bss_size
3:  cbz     w2, 4f
    str     xzr, [x1], #8
    sub     w2, w2, #1
    cbnz    w2, 3b

    // DTB load address is kept in x0 by GPU. Save it somewhere else
    mov     x28, x0

    // set top of stack to exec address
    ldr     x1, =0x60000
    mov     sp, x1
    bl      self_relocate               // __start__ = 0x80000

    // jump to relocated main()
    mov     x0, x28                     // Put dtb address back as a argument of main
4:  bl      (main - 0x80000 + 0x60000)  // __start__ = 0x60000
    // for failsafe, halt this core too
    b       1b

// This function copies 0x80000 to 0x60000, length=0x10000 bytes
self_relocate:
    /* Compiled from https://godbolt.org/:
        void self_relocate(){
          // uart_printf("Before relocation: __start__=0x%p\r\n", &__start__); // prints __start__=0x80000
          const u64 size = 0x10000 >> 3; // >>3 to be devided by 8
          u64 *load_addr = (u64*) 0x80000;
          u64 *exec_addr = (u64*) 0x60000;
          for(u64 i=0; i<size; i++)
            *exec_addr++ = *load_addr++;
          return;
        }
     */
  sub sp, sp, #32
  mov x0, (0x10000 >> 3)
  str x0, [sp]       // const u64 size = 0x10000 >> 3;
  mov x0, 0x80000
  str x0, [sp, 24]   // u64 *load_addr = (u64*) 0x80000;
  mov x0, 0x60000
  str x0, [sp, 16]   // u64 *exec_addr = (u64*) 0x60000;
  str xzr, [sp, 8]   // u64 i=0;
.L3:
  ldr x1, [sp, 8]    // x1 = i;
  mov x0, (0x10000>>3 - 1)  // x0 = 8191
  cmp x1, x0         // 
  bgt .L5            // branch greater than, x1 > x0
  ldr x1, [sp, 24]   // x1 = load_addr;
  add x0, x1, 8      // x0 = x1 + 8;
  str x0, [sp, 24]   // load_addr = x1;
  ldr x0, [sp, 16]   // x0 = exec_addr;
  add x2, x0, 8      // x2 = x0 + 8;
  str x2, [sp, 16]   // exec_addr = x2;
  ldr x1, [x1]       // x1 = *x1
  str x1, [x0]       // *x0 = x1;
  ldr x0, [sp, 8]    // x0 = i;
  add x0, x0, 1      // x0 = x0 + 1;
  str x0, [sp, 8]    // i = x0;
  b .L3
.L5:
  nop
  add sp, sp, 32     // destory local vars
  ret
