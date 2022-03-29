
#ifndef __SYS_REG_H_
#define __SYS_REG_H_

#ifdef __cplusplus
extern "C" {
#endif


#define read_sysreg(r) ({                       \
    unsigned long __val;                        \
    asm volatile("mrs %0, " #r : "=r" (__val)); \
    __val;                                      \
})

#define write_sysreg(r, __val) ({                \
	asm volatile("msr " #r ", %0" :: "r" (__val)); \
})



#ifdef __cplusplus
}
#endif
#endif  // __SYS_REG_H_