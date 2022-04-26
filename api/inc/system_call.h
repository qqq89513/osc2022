
#ifndef __SYSTEM_CALL_H_
#define __SYSTEM_CALL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "sys_reg.h"

void system_call(trap_frame *tf);

int    sysc_getpid();
size_t sysc_uart_read(char buf[], size_t size);
size_t sysc_uart_write(const char buf[], size_t size);
int    sysc_exec(const char *name, char *const argv[]);
int    sysc_fork();
void   sysc_exit(int status);
int    sysc_mbox_call(unsigned char ch, unsigned int *mbox);
int    sysc_kill(int pid);

#ifdef __cplusplus
}
#endif
#endif  // __SYSTEM_CALL_H_