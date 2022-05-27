#include "mmu.h"
#include "sys_reg.h"
#include "diy_malloc.h"
#include "diy_string.h"
#include "uart.h"

#define TCR_CONFIG_REGION_48bit (((64 - 48) << 16) | ((64 - 48) << 0)) // t1sz, t0sz, (64-48) bits should be all 1 or 0, for virtual address
#define TCR_CONFIG_4KB          ((0b10 << 30) | (0b00 << 14))          // tg1, tg0, set granule 4kB and 4kB
#define TCR_CONFIG_DEFAULT      (TCR_CONFIG_REGION_48bit | TCR_CONFIG_4KB)

#define MAIR_DEVICE_nGnRnE      0b00000000
#define MAIR_NORMAL_NOCACHE     0b01000100  // high 0100:Normal memory, Outer Non-cacheable; low 0100:Normal memory, Inner Non-cacheable
#define MAIR_IDX_DEVICE_nGnRnE  0           // set for Attr0
#define MAIR_IDX_NORMAL_NOCACHE 1           // set for Attr1
#define MAIR_SHIFT              2

#define PD_TABLE 0b11 // for table L0~2
#define PD_PAGE  0b11 // for table L3
#define PD_BLOCK 0b01
#define PD_ACCESS (1 << 10)
#define PD_USER_KERNEL_ACCESS (1 << 6)

#define KERNEL_VM_TO_PM_MASK 0x0000FFFFFFFFFFFF // for kernel, virtual mem addr to physical mem addr

uint64_t *new_page_table(){
  uint64_t *table_addr = diy_malloc(PAGE_SIZE);
  memset_(table_addr, 0, PAGE_SIZE);
  for(size_t i=0; i<(PAGE_SIZE/sizeof(uint64_t)); i++)
    table_addr[i] = 0;
  return table_addr;
}

void map_pages(uint64_t *pgd, uint64_t va_start, uint64_t pa_start, int num){
  if (pgd == NULL || pa_start == 0){
    uart_printf("Error, in map_pages(), pgd=0x%p, pa_start=0x%p\r\n", pgd, (void*)pa_start);
    return;
  }

  int index[4]; // index of each table for L0~3
  uint64_t va = 0;
  uint64_t *table = NULL;
  uint64_t entry = 0;
  pa_start = KERNEL_VA_TO_PA(pa_start);
  for (int n = 0; n < num; ++n) {
    
    // Get index of each level
    va = (uint64_t)(va_start + n*PAGE_SIZE);
    va >>= 12;   index[3] = va & 0x1ff;
    va >>= 9;    index[2] = va & 0x1ff;
    va >>= 9;    index[1] = va & 0x1ff;
    va >>= 9;    index[0] = va & 0x1ff;
    table = (uint64_t*) KERNEL_PA_TO_VA((uint64_t)pgd);
    entry = 0;

    // Find the address of level3 table
    for(int lv=0; lv<3; lv++) {  // map lv0~2
      
      // Allocate a table that table[index[lv]] can point to
      if(table[index[lv]] == 0){
        table[index[lv]] = KERNEL_VA_TO_PA(new_page_table()) | PD_TABLE;
      }

      // Remove attributes at low 12 bits
      entry = CLEAR_LOW_12bit(table[index[lv]]);

      // Next level
      table = (uint64_t*)KERNEL_PA_TO_VA(entry); // address of the first entry of next level table
    }

    // leve3, aka PTE
    if(table[index[3]] != 0)
      uart_printf("Warning, in map_pages(), PTE[%d]=%lx alread mapped\r\n", index[3], table[index[3]]);
    table[index[3]] = (pa_start + n*PAGE_SIZE) | PD_ACCESS | PD_USER_KERNEL_ACCESS | (MAIR_IDX_NORMAL_NOCACHE << MAIR_SHIFT) | PD_PAGE;
  }
}

void mmu_init(){
  write_sysreg(tcr_el1, TCR_CONFIG_DEFAULT);

  write_sysreg(mair_el1, 
    (MAIR_DEVICE_nGnRnE  << (MAIR_IDX_DEVICE_nGnRnE * 8)) |   // set MAIR attr0
    (MAIR_NORMAL_NOCACHE << (MAIR_IDX_NORMAL_NOCACHE * 8)) ); // set MAIR attr1

  uint64_t *PGD = (uint64_t*)( PAGE_TABLE_STATICS_START_ADDR & KERNEL_VM_TO_PM_MASK ); // pg_dir is from link.ld, remove 0xFFFF000000000000 to access phy address
  uint64_t *PUD = (uint64_t*)((uint64_t)PGD + 0x1000);  // L1 table, entry points to L2 table or 1GB block
  uint64_t *PMD = (uint64_t*)((uint64_t)PGD + 0x2000);  // L2 table, entry points to L3 table or 2MB block

  // Set Identity Paging
  // 0x00000000 ~ 0x3f000000: Normal  // PUD[0]
  // 0x3f000000 ~ 0x40000000: Device  // PUD[0]
  // 0x40000000 ~ 0x80000000: Device  // PUD[1]
  // PGD[0] = (uint64_t)PUD | (1LL<<63) | (1LL<<60) | PD_TABLE;  // 1st entry points to a L1 table ??
  PGD[0] = (uint64_t)PUD | PD_TABLE;  // 1st entry points to a L1 table
  PUD[0] = (uint64_t)PMD | PD_TABLE;  // 1st 1GB mapped by L3 table, where L3 table pointed by 1st entry of PUD
  PUD[1] = 0x40000000 | (PD_ACCESS | (MAIR_IDX_DEVICE_nGnRnE << MAIR_SHIFT) | PD_BLOCK);     // 2nd 1GB mapped by the 2nd entry of PUD

  // Note for following for:
  //    <<21 because each entry of PMD is 2MB=1<<21
  //    504 = 0x3f000000 / 2MB
  //    512 = 0x40000000 / 2MB

  // 0x00000000 ~ 0x3f000000: Normal
  for(uint64_t i=0; i<504; i++)
    PMD[i] = (i << 21) | PD_ACCESS | (MAIR_IDX_NORMAL_NOCACHE<<MAIR_SHIFT) | PD_BLOCK;

  // 0x3f000000 ~ 0x40000000: Device
  for(uint64_t i=504; i<512; i++)
    PMD[i] = (i << 21) | PD_ACCESS | (MAIR_IDX_DEVICE_nGnRnE<<MAIR_SHIFT) | PD_BLOCK;

  write_sysreg(ttbr0_el1, PGD);         // load PGD to the bottom translation-based register.
  write_sysreg(ttbr1_el1, PGD);         // also load PGD to the upper translation based register.

  uint64_t temp = read_sysreg(sctlr_el1);
  temp |= 1;  // enable MMU
  write_sysreg(sctlr_el1, temp);
}

void dump_page_table(uint64_t *pgd){
  uint64_t *table_L0 = pgd;
  uint64_t *table_L1, *table_L2, *table_L3;
  // i0, i1, i2, i3 are index of tables of lv0, lv1, lv2, lv3
  // L0, aka PGD
  for(int i0=0; i0<(PAGE_SIZE/8); i0++){
    if(table_L0[i0] == 0) continue; // skip empty entry

    uart_printf("L0[%d]=0x%lx\r\n", i0, table_L0[i0]);
    table_L1 = (uint64_t*) CLEAR_LOW_12bit(table_L0[i0]);
    table_L1 = (uint64_t*) KERNEL_PA_TO_VA(table_L1);

    // L1, aka PUD
    for(int i1=0; i1<(PAGE_SIZE/8); i1++){
      if(table_L1[i1] == 0) continue; // skip empty entry

      uart_printf("  L1[%d]=0x%lx\r\n", i1, table_L1[i1]);
      table_L2 = (uint64_t*) CLEAR_LOW_12bit(table_L1[i1]);
      table_L2 = (uint64_t*) KERNEL_PA_TO_VA(table_L2);

      // L2, aka PMD
      for(int i2=0; i2<(PAGE_SIZE/8); i2++){
        if(table_L2[i2] == 0) continue; // skip empty entry

        uart_printf("    L2[%d]=0x%lx\r\n", i2, table_L2[i2]);
        table_L3 = (uint64_t*) CLEAR_LOW_12bit(table_L2[i2]);
        table_L3 = (uint64_t*) KERNEL_PA_TO_VA(table_L3);

        // L3, aka PTE
        for(int i3=0; i3<(PAGE_SIZE/8); i3++){
          if(table_L3[i3] == 0) continue; // skip empty entry
          uart_printf("      L3[%d]=0x%lx\r\n", i3, table_L3[i3]);
        }
      }
    }
  }
}

void *virtual_mem_translate(void *virtual_addr){
  asm volatile("mov x4,    %0\n"::"r"(virtual_addr));
  asm volatile("at  s1e0r, x4\n");
  uint64_t frame_addr = (uint64_t)read_sysreg(par_el1) & 0xFFFFFFFFF000; // physical frame address
  uint64_t pa = frame_addr | ((uint64_t)virtual_addr & 0xFFF);                // combine 12bits offset
  if ((read_sysreg(par_el1) & 0x1) == 1)
    uart_printf("Error, virtual_mem_translate() failed\r\n");
  return (void*)pa;
}
