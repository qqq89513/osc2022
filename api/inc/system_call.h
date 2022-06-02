
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
int    exec_from_kernel_to_user_vm(const char *name);
int    sysc_fork();
void   sysc_exit(int status);
int    sysc_mbox_call(unsigned char ch, unsigned int *mbox);
int    sysc_kill(int pid);

// Virtual File System, system call ---------------
int    sysc_open(const char *pathname, int flags);
int    sysc_close(int fd);
size_t sysc_write(int fd, const void *buf, size_t count);
size_t sysc_read(int fd, void *buf, size_t count);
int    sysc_mkdir(const char *pathname, unsigned mode);
int    sysc_mount(const char *src, const char *target, const char *filesystem, unsigned long flags, const void *data);
int    sysc_chdir(const char *path);

#ifdef __cplusplus
}
#endif
#endif  // __SYSTEM_CALL_H_