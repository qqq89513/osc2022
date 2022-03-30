#ifndef __GENERAL_H
#define __GENERAL_H

#include <stdint.h>

#define MMIO_BASE       0x3F000000

#define GPFSEL0         ((volatile unsigned int*)(MMIO_BASE+0x00200000))
#define GPFSEL1         ((volatile unsigned int*)(MMIO_BASE+0x00200004))
#define GPFSEL2         ((volatile unsigned int*)(MMIO_BASE+0x00200008))
#define GPFSEL3         ((volatile unsigned int*)(MMIO_BASE+0x0020000C))
#define GPFSEL4         ((volatile unsigned int*)(MMIO_BASE+0x00200010))
#define GPFSEL5         ((volatile unsigned int*)(MMIO_BASE+0x00200014))
#define GPSET0          ((volatile unsigned int*)(MMIO_BASE+0x0020001C))
#define GPSET1          ((volatile unsigned int*)(MMIO_BASE+0x00200020))
#define GPCLR0          ((volatile unsigned int*)(MMIO_BASE+0x00200028))
#define GPLEV0          ((volatile unsigned int*)(MMIO_BASE+0x00200034))
#define GPLEV1          ((volatile unsigned int*)(MMIO_BASE+0x00200038))
#define GPEDS0          ((volatile unsigned int*)(MMIO_BASE+0x00200040))
#define GPEDS1          ((volatile unsigned int*)(MMIO_BASE+0x00200044))
#define GPHEN0          ((volatile unsigned int*)(MMIO_BASE+0x00200064))
#define GPHEN1          ((volatile unsigned int*)(MMIO_BASE+0x00200068))
#define GPPUD           ((volatile unsigned int*)(MMIO_BASE+0x00200094))
#define GPPUDCLK0       ((volatile unsigned int*)(MMIO_BASE+0x00200098))
#define GPPUDCLK1       ((volatile unsigned int*)(MMIO_BASE+0x0020009C))

#define GPPUD_NO_PULL   0x00 // Refer to p.101, BCM2835 datasheet
#define GPPUD_PULL_DOWN 0x01
#define GPPUD_PULL_UP   0x02

#define CORE0_IRQ_SOURCE                 ((volatile uint32_t*)(0x40000060))  // ref: page 16, https://datasheets.raspberrypi.com/bcm2836/bcm2836-peripherals.pdf
#define COREx_IRQ_SOURCE_CNTPNSIRQ_MASK  ((volatile uint32_t) (1<<1))        // don't know why left shift 1

#define WAIT_TICKS(cnt, tk) {cnt = tk; while(cnt--) { asm volatile("nop"); }}

#define EL1_ARM_INTERRUPT_ENABLE()  { __asm__ __volatile__("msr daifclr, 0xf"); }
#define EL1_ARM_INTERRUPT_DISABLE() { __asm__ __volatile__("msr daifset, 0xf"); }

void reset(int tick);

#endif /* __GENERAL_H */
