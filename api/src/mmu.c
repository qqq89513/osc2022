#include "sys_reg.h"

extern uint64_t pg_dir;

#define TCR_CONFIG_REGION_48bit (((64 - 48) << 16) | ((64 - 48) << 0)) // t1sz, t0sz, (64-48) bits should be all 1 or 0, for virtual address
#define TCR_CONFIG_4KB          ((0b10 << 30) | (0b00 << 14))          // tg1, tg0, set granule 4kB and 4kB
#define TCR_CONFIG_DEFAULT      (TCR_CONFIG_REGION_48bit | TCR_CONFIG_4KB)

#define MAIR_DEVICE_nGnRnE      0b00000000
#define MAIR_NORMAL_NOCACHE     0b01000100  // high 0100:Normal memory, Outer Non-cacheable; low 0100:Normal memory, Inner Non-cacheable
#define MAIR_IDX_DEVICE_nGnRnE  0           // set for Attr0
#define MAIR_IDX_NORMAL_NOCACHE 1           // set for Attr1

#define PD_TABLE 0b11
#define PD_BLOCK 0b01
#define PD_ACCESS (1 << 10)
#define BOOT_PGD_ATTR PD_TABLE
#define BOOT_PUD_ATTR (PD_ACCESS | (MAIR_IDX_DEVICE_nGnRnE << 2) | PD_BLOCK) // device mem, points to block

#define KERNEL_VM_TO_PM_MASK 0X0000FFFFFFFFFFFF // for kernel, virtual mem addr to physical mem addr

void mmu_init(){
  write_sysreg(tcr_el1, TCR_CONFIG_DEFAULT);

  write_sysreg(mair_el1, 
    (MAIR_DEVICE_nGnRnE  << (MAIR_IDX_DEVICE_nGnRnE * 8)) | 
    (MAIR_NORMAL_NOCACHE << (MAIR_IDX_NORMAL_NOCACHE * 8)) );

  uint64_t *PGD = (uint64_t*)( (uint64_t)(&pg_dir) & KERNEL_VM_TO_PM_MASK ); // pg_dir is from link.ld, remove 0xFFFF000000000000 to access phy address
  uint64_t *PUD = (uint64_t*)((uint64_t)PGD + 0x1000);
  uint64_t *PMD = (uint64_t*)((uint64_t)PGD + 0x2000);

  PGD[0] = (uint64_t)PUD | BOOT_PGD_ATTR;
  PUD[0] = 0x00000000 | BOOT_PUD_ATTR;  // 1st 1GB mapped by the 1st entry of PUD
  PUD[1] = 0x40000000 | BOOT_PUD_ATTR;  // 2nd 1GB mapped by the 2nd entry of PUD

  write_sysreg(ttbr0_el1, PGD);         // load PGD to the bottom translation-based register.
  write_sysreg(ttbr1_el1, PGD);         // also load PGD to the upper translation based register.

  uint64_t temp = read_sysreg(sctlr_el1);
  temp |= 1;  // enable MMU
  write_sysreg(sctlr_el1, temp);
}
