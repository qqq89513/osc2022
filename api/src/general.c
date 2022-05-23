#include "general.h"

#define PM_PASSWORD 0x5a000000

void set(void *addr, unsigned int value) {
    volatile unsigned int* point = (unsigned int*)addr;
    *point = value;
}

void reset(int tick) {                 // reboot after watchdog timer expire
    set((void*)PM_RSTC, PM_PASSWORD | 0x20);  // full reset
    set((void*)PM_WDOG, PM_PASSWORD | tick);  // number of watchdog tick
}

void cancel_reset() {
    set((void*)PM_RSTC, PM_PASSWORD | 0);  // full reset
    set((void*)PM_WDOG, PM_PASSWORD | 0);  // number of watchdog tick
}
