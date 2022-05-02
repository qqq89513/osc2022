
#ifndef __SYS_REG_H_
#define __SYS_REG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define read_sysreg(r) ({                           \
    unsigned long __val;                            \
    asm volatile("mrs %0, " #r : "=r" (__val));     \
    __val;                                          \
})

#define write_sysreg(r, __val) ({                   \
  asm volatile("msr " #r ", %0" :: "r" (__val));    \
})

#define read_gen_reg(r)({                           \
    unsigned long __val;                            \
    asm volatile("mov %0, " #r : "=r" (__val));     \
    __val;                                          \
})

#define write_gen_reg(r, __val) ({                  \
  asm volatile("mov " #r ", %0" :: "r" (__val));    \
})

// Trap frame for exeception handling, refer: save_all, check vect_table_and_execption_handler.S
typedef struct trap_frame {
  uint64_t x0;  uint64_t x1;
  uint64_t x2;  uint64_t x3;
  uint64_t x4;  uint64_t x5;
  uint64_t x6;  uint64_t x7;
  uint64_t x8;  uint64_t x9;
  uint64_t x10; uint64_t x11;
  uint64_t x12; uint64_t x13;
  uint64_t x14; uint64_t x15;
  uint64_t x16; uint64_t x17;
  uint64_t x18; uint64_t x19;
  uint64_t x20; uint64_t x21;
  uint64_t x22; uint64_t x23;
  uint64_t x24; uint64_t x25;
  uint64_t x26; uint64_t x27;
  uint64_t x28; uint64_t fp;
  uint64_t lr; uint64_t spsr_el1;
  uint64_t elr_el1;  uint64_t esr_el1;
  uint64_t sp_el0;   uint64_t padding0;
  uint64_t padding1; uint64_t padding2;
} trap_frame;


#ifdef __cplusplus
}
#endif
#endif  // __SYS_REG_H_