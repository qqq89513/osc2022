
#ifndef __MMU_H_
#define __MMU_H_

#ifdef __cplusplus
extern "C" {
#endif

// Should be reserved by mem_reserve_kernel_vm() if virtual memory is used
#define PAGE_TABLE_STATICS_START_ADDR     0x1000
#define PAGE_TABLE_STATICS_END_ADDR       (PAGE_TABLE_STATICS_START_ADDR + (0x1000*3))

#ifdef __cplusplus
}
#endif
#endif  // __MMU_H_
