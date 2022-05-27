
#ifndef __MMU_H_
#define __MMU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Should be reserved by mem_reserve_kernel_vm() if virtual memory is used
#define PAGE_TABLE_STATICS_START_ADDR     0x1000
#define PAGE_TABLE_STATICS_END_ADDR       (PAGE_TABLE_STATICS_START_ADDR + (0x1000*3))

#define DEFAULT_THREAD_VA_CODE_START  0x0000
#define DEFAULT_THREAD_VA_STACK_START 0xFFFFFFFFB000

#define ENTRY_GET_ATTRS(num)  ((num) & 0xFFFF000000000FFF)
#define CLEAR_LOW_12bit(num)  ((num) & 0xFFFFFFFFFFFFF000)
#define KERNEL_VA_TO_PA(addr) (((uint64_t)(addr)) & 0x0000FFFFFFFFFFFF)
#define KERNEL_PA_TO_VA(addr) (((uint64_t)(addr)) | 0xFFFF000000000000)

uint64_t *new_page_table();
void map_pages(uint64_t *pgd, uint64_t va_start, uint64_t pa_start, int num);
void dump_page_table(uint64_t *pgd);
void copy_page_table(uint64_t *from, uint64_t *to);
void *virtual_mem_translate(void *virtual_addr);

#ifdef __cplusplus
}
#endif
#endif  // __MMU_H_
