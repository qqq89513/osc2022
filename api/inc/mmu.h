
#ifndef __MMU_H_
#define __MMU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Should be reserved by mem_reserve_kernel_vm() if virtual memory is used
#define PAGE_TABLE_STATICS_START_ADDR     0x1000
#define PAGE_TABLE_STATICS_END_ADDR       (PAGE_TABLE_STATICS_START_ADDR + (0x1000*3))

uint64_t *new_page_table();
void map_pages(uint64_t *pgd, uint64_t va_start, uint64_t pa_start, int num);
void dump_page_table(uint64_t *pgd);

#ifdef __cplusplus
}
#endif
#endif  // __MMU_H_
