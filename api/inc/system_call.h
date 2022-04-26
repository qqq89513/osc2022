
#ifndef __SYSTEM_CALL_H_
#define __SYSTEM_CALL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "sys_reg.h"

void system_call(trap_frame *tf);

int    getpid();
size_t uart_read(char buf[], size_t size);
size_t uart_write(const char buf[], size_t size);
int    exec(const char *name, char *const argv[]);
int    fork();
void   exit_(int status);
int    mbox_call_(unsigned char ch, unsigned int *mbox);
void   kill_(int pid);

#ifdef __cplusplus
}
#endif
#endif  // __SYSTEM_CALL_H_