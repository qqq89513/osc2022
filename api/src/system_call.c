
#include "system_call.h"
#include "diy_malloc.h"
#include "cpio.h"
#include "sys_reg.h"
#include "thread.h"
#include "mbox.h"
#include "uart.h"
#include "general.h"
#include "mmu.h"
#include "virtual_file_system.h"
#include "diy_string.h"

#ifdef THREADS  // pass -DTHREADS to compiler for lab5
// Lab5, basic 2 description: The system call numbers given below would be stored in x8
#define SYSCALL_NUM_GETPID     0
#define SYSCALL_NUM_UART_READ  1
#define SYSCALL_NUM_UART_WRITE 2
#define SYSCALL_NUM_EXEC       3
#define SYSCALL_NUM_FORK       4
#define SYSCALL_NUM_EXIT       5
#define SYSCALL_NUM_MBOX_CALL  6
#define SYSCALL_NUM_KILL       7
#define SYSCALL_NUM_OPEN       11
#define SYSCALL_NUM_CLOSE      12
#define SYSCALL_NUM_WRITE      13
#define SYSCALL_NUM_READ       14
#define SYSCALL_NUM_MKDIR      15
#define SYSCALL_NUM_MOUNT      16
#define SYSCALL_NUM_CHDIR      17
#define SYSCALL_NUM_LSEEK      18

extern void kid_thread_return_fork();   // defined in vect_table_and_execption_handler.S

// Private functions, priv stands for private
static int    priv_getpid();
static size_t priv_uart_read(char buf[], size_t size);
static size_t priv_uart_write(const char buf[], size_t size);
static int    priv_exec(const char *name, char *const argv[], trap_frame *tf);
static int    priv_fork(trap_frame *tf_mom);
static void   priv_exit(int status);
static int    priv_mbox_call(unsigned char ch, unsigned int *mbox);
static int    priv_kill(int pid);
static int    priv_open(const char *pathname, int flags);
static int    priv_close(int fd);
static size_t priv_write(int fd, const void *buf, size_t count);
static size_t priv_read(int fd, void *buf, size_t count);
static int    priv_mkdir(const char *pathname, unsigned mode);
static int    priv_mount(const char *src, const char *target, const char *filesystem, unsigned long flags, const void *data);
static int    priv_chdir(const char *path);
static long   priv_lseek64(int fd, long offset, int whence);


/**
 * How to invoke a system call:
 *  1. Call public function, let's say sysc_kill()
 *  2. In sysc_kill(pid), we let x8=SYSCALL_NUM_KILL and x0=pid, and than call "svc 0"
 *  3. Exception triggered by "svc 0", exeception handler save current registers into stack as a trap frame
 *  4. Pass the trap frame pointer to exeception handler in c domain
 *  5. C domain exeception handler recognizes exeception triggered by "svc 0", pass the trap frame pointer to system_call()
 *  6. In system_call(), get system call number from x8 in trap frame, and then execute the corresponding private system call
 *  7. x0..7 in trap frame can be read as parameters passing to system call, x0 is the return value
 *  8. In this case, sysc_kill() has 1 passed in parameter, pid in x0, and 1 return value
*/
void system_call(trap_frame *tf){
  uint64_t num = tf->x8;
  thread_t *thd = NULL;
  switch(num){
    case SYSCALL_NUM_GETPID:      tf->x0 = priv_getpid();                                                 break;
    case SYSCALL_NUM_UART_READ:   tf->x0 = priv_uart_read((char*)tf->x0, (size_t)tf->x1);                 break;
    case SYSCALL_NUM_UART_WRITE:  tf->x0 = priv_uart_write((char*)tf->x0, (size_t)tf->x1);                break;
    case SYSCALL_NUM_EXEC:        tf->x0 = priv_exec((char*)tf->x0, (char **)tf->x1, tf);                 break;
    case SYSCALL_NUM_FORK:        tf->x0 = priv_fork(tf);                                                 break;
    case SYSCALL_NUM_EXIT:        priv_exit((int)tf->x0);                                                 break;
    case SYSCALL_NUM_MBOX_CALL:   tf->x0 = priv_mbox_call((unsigned char)tf->x0, (unsigned int*)tf->x1);  break;
    case SYSCALL_NUM_KILL:        tf->x0 = priv_kill(tf->x0);                                             break;
    case SYSCALL_NUM_OPEN:        tf->x0 = priv_open((const char*)tf->x0, tf->x1);                        break;
    case SYSCALL_NUM_CLOSE:       tf->x0 = priv_close(tf->x0);                                            break;
    case SYSCALL_NUM_WRITE:       tf->x0 = priv_write(tf->x0, (const void*)tf->x1, tf->x2);               break;
    case SYSCALL_NUM_READ:        tf->x0 = priv_read(tf->x0, (void*)tf->x1, tf->x2);                      break;
    case SYSCALL_NUM_MKDIR:       tf->x0 = priv_mkdir((const char*)tf->x0, tf->x1);                       break;
    case SYSCALL_NUM_MOUNT:       
      tf->x0 = priv_mount((const char*)tf->x0, (const char*)tf->x1, (const char*)tf->x2, tf->x3, (const void*)tf->x4);
      break;
    case SYSCALL_NUM_CHDIR:       tf->x0 = priv_chdir((const char*)tf->x0);                               break;
    case SYSCALL_NUM_LSEEK:       tf->x0 = priv_lseek64(tf->x0, tf->x1, tf->x2);                          break;

    default:
      thd = thread_get_current();
      uart_printf("Exeception, unimplemented system call, num=%lu, pid=%d, elr_el1=%lX\r\n", num, thd->pid, thd->elr_el1);
  }
}

int           sysc_getpid(){
  write_gen_reg(x8, SYSCALL_NUM_GETPID);
  asm volatile("svc 0");
  // trap frame in stack is modified in system_call(),
  // and than restored by load_all.
  // by exeception handled, following is executed explicitly
  // ret_val = priv_getpid();
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_getpid(){
  thread_t *thd_now = thread_get_current();
  if(thd_now != NULL)
    return thd_now->pid;
  else
    return -1;
}

size_t        sysc_uart_read(char buf[], size_t size){
  write_gen_reg(x8, SYSCALL_NUM_UART_READ);
  write_gen_reg(x1, size);  // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, buf);   // so write to x0 should be the last one performed, maybe should change to register int var asm("x0");
  asm volatile("svc 0");
  size_t ret_val = read_gen_reg(x0);
  return ret_val;
}
static size_t priv_uart_read(char buf[], size_t size){
  const size_t s = size;
  EL1_ARM_INTERRUPT_ENABLE(); // since uart_read_byte() blocks is no input
  while(size != 0){
    *buf++ = (char)uart_read_byte();
    size--;
  }
  EL1_ARM_INTERRUPT_DISABLE();
  return s;
}

size_t        sysc_uart_write(const char buf[], size_t size){
  write_gen_reg(x8, SYSCALL_NUM_UART_WRITE);
  write_gen_reg(x1, size);  // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, buf);   // so write to x0 should be the last one performed
  asm volatile("svc 0");
  size_t ret_val = read_gen_reg(x0);
  return ret_val;
}
static size_t priv_uart_write(const char buf[], size_t size){
  const size_t s = size;
  while(size != 0){
    uart_send(*buf++);
    size--;
  }
  return s;
}

int           sysc_exec(const char *name, char *const argv[]){
  write_gen_reg(x8, SYSCALL_NUM_EXEC);
  write_gen_reg(x1, argv);    // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, name);    // so write to x0 should be the last one performed
  asm volatile("svc 0");      // should not return
  return 0;
}
static int    priv_exec(const char *name, char *const argv[], trap_frame *tf){
  thread_t *thd = thread_get_current();
  uint8_t *load_addr = NULL; // 64 pages
  // if(thd->mode == KERNEL){
  //   uart_printf("sysc_exec() failed, current thread pid=%d, sysc_exec() for KERNEL thread is now unimplemented\r\n", thd->pid);
  //   return -1;
  // }

  // Copy image to a dynamic allocated space
  load_addr = diy_malloc(PAGE_SIZE*64);
  file *fh = NULL;
  int ret = vfs_open((char*)name, 0, &fh);
  if(ret == 0){
    fh->f_ops->read(fh, load_addr, PAGE_SIZE*64);
    fh->f_ops->close(fh);
  }
  else{
    return -1;
  }
  // if(cpio_copy((char*)name, load_addr) != 0){
  //   uart_printf("sysc_exec() failed, failed to locate file %s.\r\n", name);
  //   return -1;
  // }

#ifdef VIRTUAL_MEM
  // Map custom virtual address to dynamic allocated address
  // Note that diy_malloc() return virtual (with kernel prefix), map_pages() remove it for physical
  map_pages((uint64_t*)thd->ttbr0_el1, DEFAULT_THREAD_VA_CODE_START,  (uint64_t)load_addr, 64);  // map for code space
  
  // Use virtual address instead
  load_addr = (void*) DEFAULT_THREAD_VA_CODE_START;
#endif

  // Modify trap frame, 
  // so when returning from execeptio, eret sets sp to sp_el0 and jumps to elr_el12
  tf->elr_el1 = (uint64_t)load_addr;
  tf->sp_el0 = (uint64_t)thd->user_sp;
  return 0;
  // should call eret here, exec the program direcctly
  // should also clean the original thread
}
#ifdef VIRTUAL_MEM
int           exec_from_kernel_to_user_vm(const char *name){
  thread_t *thd = thread_get_current();   // raise syn exception if running under el0
  uint8_t *load_addr = NULL; // 64 pages
  if(thd->mode != KERNEL){
    uart_printf("exec_from_kernel_to_user_vm() failed, current thread pid=%d is not kernel thread.\r\n", thd->pid);
    return -1;
  }

  // Copy image to a dynamic allocated space
  load_addr = diy_malloc(PAGE_SIZE*64);
  if(cpio_copy((char*)name, load_addr) != 0){
    uart_printf("exec_from_kernel_to_user_vm() failed, failed to locate file %s.\r\n", name);
    return -1;
  }

  thd->mode = USER; // later used in fork

  // Map custom virtual address to dynamic allocated address
  // Note that diy_malloc() return virtual (with kernel prefix), map_pages() remove it for physical
  void *user_space = diy_malloc(PAGE_SIZE*4);
  uint64_t *pgd = (uint64_t*)KERNEL_VA_TO_PA(new_page_table());
  map_pages(pgd, DEFAULT_THREAD_VA_CODE_START,  (uint64_t)load_addr, 64);  // map for code space
  map_pages(pgd, DEFAULT_THREAD_VA_STACK_START, (uint64_t)user_space, 4);  // map for stack
  
  // Use virtual address instead
  load_addr = (void*) DEFAULT_THREAD_VA_CODE_START;
  user_space = (void*) DEFAULT_THREAD_VA_STACK_START;

  thd->target_func = load_addr;
  thd->user_space = user_space;
  thd->user_sp = user_space + DEFAULT_THREAD_SIZE - 1;
  thd->user_sp = (void*)(  (uint64_t)thd->user_sp - ((uint64_t)thd->user_sp % 16)  ); // round down to multiple of 16
  thd->ttbr0_el1 = (uint64_t)pgd;

  // Change ttbr0_el1
  write_gen_reg(x0, thd->ttbr0_el1);
  asm volatile("dsb ish");            // ensure write has completed
  asm volatile("msr ttbr0_el1, x0");  // switch translation based address.
  asm volatile("tlbi vmalle1is");     // invalidate all TLB entries
  asm volatile("dsb ish");            // ensure completion of TLB invalidatation
  asm volatile("isb");                // clear pipeline

  thread_go_to_el0();
  uart_printf("Exception, exec_from_kernel_to_user_vm(), should never get here\r\n");
  return 0;
  // should call eret here, exec the program direcctly
  // should also clean the original thread
}
#endif

int           sysc_fork(){
  write_gen_reg(x8, SYSCALL_NUM_FORK);
  asm volatile("svc 0");
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_fork(trap_frame *tf_mom){
  thread_t *thd_mom = thread_get_current();
  thread_t *thd_kid = thread_create((void*)tf_mom->elr_el1, thd_mom->mode);
  thread_t thd_backup;  // backup for kid
  const uint8_t mom_higher = thd_mom > thd_kid;
  uint64_t offset = (uint64_t)( mom_higher ? ((uint64_t)thd_mom - (uint64_t)thd_kid) : ((uint64_t)thd_kid - (uint64_t)thd_mom) );
  uint64_t offset_tf = (uint64_t)tf_mom - (uint64_t)thd_mom;
  trap_frame *tf_kid = (trap_frame*)((uint64_t)thd_kid + offset_tf);
  uint8_t *copy_src = NULL, *copy_dest = NULL;

  /* When kid thread is scheduled, it 
      1. sp updated by switch_to, and than jumps to kid_thread_return_fork() by setting lr=kid_thread_return_fork() 
      2. In kid_thread_return_fork(), recovery context from sp. Note that sp now is the sp of kid thread
      3. eret to elr_el1, set sp to sp_el0 if returning back to el0
      4. Back to the instruction right after "svc 0"
      5. Get return value from system call, which is 0
  */

  // Copy mother's stack to kid's stack
  {
    // Backup kid
    thd_backup.allocated_addr = thd_kid->allocated_addr;
    thd_backup.user_space     = thd_kid->user_space;
    thd_backup.user_sp        = thd_kid->user_sp;
    thd_backup.pid            = thd_kid->pid;
    thd_backup.ppid           = thd_mom->pid;
    thd_backup.state          = thd_kid->state;
    thd_backup.next           = thd_kid->next;

    // Copy momther thread's entire stack and thread info
    copy_src  = (uint8_t*)thd_mom->allocated_addr;
    copy_dest = (uint8_t*)thd_kid->allocated_addr;
    for(size_t i=0; i<DEFAULT_THREAD_SIZE; i++) copy_dest[i] = copy_src[i];

    // Recover kid
    thd_kid->allocated_addr = thd_backup.allocated_addr;
    thd_kid->user_space     = thd_backup.user_space;
    thd_kid->user_sp        = thd_backup.user_sp;
    thd_kid->pid            = thd_backup.pid;
    thd_kid->ppid           = thd_backup.ppid;
    thd_kid->state          = thd_backup.state;
    thd_kid->next           = thd_backup.next;

    // Copy mother thread's user stack if it's a user thread
    if(thd_kid->mode == USER){
      copy_src  = (uint8_t*)thd_mom->user_space;
      copy_dest = (uint8_t*)thd_kid->user_space;
      for(size_t i=0; i<DEFAULT_THREAD_SIZE; i++) copy_dest[i] = copy_src[i];
    }
  }

  // Set kid thread sp and lr for switch_to
  thd_kid->sp = (uint64_t)tf_kid;   // the trap frame is stored on the stack, load_all recover context from stack
  thd_kid->lr = (uint64_t)kid_thread_return_fork; // jumps to kid_thread_return_fork() when it's first time scheduled

  // Copy mother's trap frame to kid's trap frame
  copy_src  = (uint8_t*)tf_mom;
  copy_dest = (uint8_t*)tf_kid;
  for(size_t i=0; i<sizeof(trap_frame); i++) copy_dest[i] = copy_src[i];
  
#ifdef VIRTUAL_MEM // Copy page table and remap stack space when virtual memory enabled
  thd_kid->ttbr0_el1 = KERNEL_VA_TO_PA(new_page_table());
  copy_page_table((uint64_t*)thd_mom->ttbr0_el1, (uint64_t*)thd_kid->ttbr0_el1);
  map_pages((uint64_t*)thd_kid->ttbr0_el1, DEFAULT_THREAD_VA_STACK_START, (uint64_t)thd_kid->user_space, DEFAULT_THREAD_SIZE/PAGE_SIZE);
  map_pages((uint64_t*)thd_kid->ttbr0_el1, KERNEL_PA_TO_VA(0x3c000000), 0x3c000000, (0x3f000000-0x3c000000)/PAGE_SIZE);
  thd_kid->user_space = (void*)DEFAULT_THREAD_VA_STACK_START;
  thd_kid->user_sp = thd_mom->user_sp;
#else // Set fp, sp_el0 offset if virtual memory not enabled
  // Set trap frame values which are different from mother thread's trap frame
  tf_kid->x0 = 0; // return value of fork() of kid thread is 0
  tf_kid->fp = mom_higher ? (tf_mom->fp - offset) : (tf_mom->fp + offset);
  tf_kid->lr = tf_mom->lr;
  if(tf_mom->sp_el0 == 0)
    tf_kid->sp_el0 = 0;
  else{
    const uint64_t offset_sp_el0 = tf_mom->sp_el0 - (uint64_t)thd_mom->user_space;
    tf_kid->sp_el0 = (uint64_t)thd_kid->user_space + offset_sp_el0;
  }
#endif

#ifndef VIRTUAL_MEM

  // uart_printf("in priv_fork():\r\n");
  // uart_printf("  thd_mom sp=%lx, fp=%lx, user_space=%lx, user_sp=%lx, ttbr0=%lx\r\n", thd_mom->fp, thd_mom->sp, (uint64_t)thd_mom->user_space, (uint64_t)thd_mom->user_sp, thd_mom->ttbr0_el1);
  // uart_printf("  thd_kid sp=%lx, fp=%lx, user_space=%lx, user_sp=%lx, ttbr0=%lx\r\n", thd_kid->fp, thd_kid->sp, (uint64_t)thd_kid->user_space, (uint64_t)thd_kid->user_sp, thd_kid->ttbr0_el1);
  // uart_printf("  tf_mom=%lx elr_el1=%lx, lr=%lx, sp_el0=%lx, fp=%lx\r\n", (uint64_t)tf_mom, tf_mom->elr_el1, tf_mom->lr, tf_mom->sp_el0, tf_mom->fp);
  // uart_printf("  tf_kid=%lx elr_el1=%lx, lr=%lx, sp_el0=%lx, fp=%lx\r\n", (uint64_t)tf_kid, tf_kid->elr_el1, tf_kid->lr, tf_kid->sp_el0, tf_kid->fp);
#endif
  return thd_kid->pid;
}

// Thread self terminate, status unimplmented
void          sysc_exit(int status){
  write_gen_reg(x8, SYSCALL_NUM_EXIT);
  write_gen_reg(x0, status); // pass parameter to priv_exit()
  asm volatile("svc 0");     // by exception handled, priv_exit(status) is called
  return;
}
static void   priv_exit(int status){
  exit_call_by_syscall_only();
}

int           sysc_mbox_call(unsigned char ch, unsigned int *mbox){
  write_gen_reg(x8, SYSCALL_NUM_MBOX_CALL);
  write_gen_reg(x1, mbox);  // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, ch);    // so write to x0 should be the last one performed
  asm volatile("svc 0");
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_mbox_call(unsigned char ch, unsigned int *mbox){
  int ret = mbox_call_user_buffer(ch, mbox);
  return ret;
}

int           sysc_kill(int pid){
  write_gen_reg(x8, SYSCALL_NUM_KILL);
  write_gen_reg(x0, pid);
  asm volatile("svc 0");
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_kill(int pid){
  return kill_call_by_syscall_only(pid);
}


// Virtual File System, system call -------------------------------------------------------

// Return file descriptor
int           sysc_open(const char *pathname, int flags){
  write_gen_reg(x8, SYSCALL_NUM_OPEN);
  write_gen_reg(x1, flags);     // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, pathname);  // so write to x0 should be the last one performed
  asm volatile("svc 0");
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_open(const char *pathname, int flags){
  thread_t *thd = thread_get_current();
  int fd = thread_get_idle_fd(thd);
  if(fd < 0){
    uart_printf("Error, priv_open(), cannot open more file for pid=%d\r\n", thd->pid);
    return -1;
  }

  // Translate to absolute path
  char abs_path[TMPFS_MAX_PATH_LEN];
  abs_path[0] = '\0';
  to_abs_path(abs_path, thd->cwd, pathname);

  file *fh = NULL;
  int ret = vfs_open((char*)abs_path, flags, &fh);
  uart_printf("Debug, priv_open(), abs_path=%s, flags=0x%X, ret=%d\r\n", abs_path, flags, ret);
  if(ret == 0){
    thd->fd_table[fd] = fh;
    return fd;
  }
  else
    return -1;
}

int           sysc_close(int fd){
  write_gen_reg(x8, SYSCALL_NUM_CLOSE);
  write_gen_reg(x0, fd);
  asm volatile("svc 0");
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_close(int fd){
  thread_t *thd = thread_get_current();
  file *fh = thd->fd_table[fd];
  if(fh == NULL){
    uart_printf("Error, priv_close(), unrecognized fd=%d, pid=%d\r\n", fd, thd->pid);
    return 1;
  }
  int ret = vfs_close(fh);
  if(ret == 0)
    thd->fd_table[fd] = NULL; // fd can be reused
  else
    uart_printf("Error, priv_close(), failed on vfs_close(), fd=%d, fh=0x%lX, pid=%d, ret=%d\r\n",
      fd, (uint64_t)fh, thd->pid, ret);
  return ret;
}

// Return size wrote or error code
size_t        sysc_write(int fd, const void *buf, size_t count){
  write_gen_reg(x8, SYSCALL_NUM_WRITE);
  write_gen_reg(x2, count);
  write_gen_reg(x1, buf);  // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, fd);   // so write to x0 should be the last one performed
  asm volatile("svc 0");
  size_t ret_val = read_gen_reg(x0);
  return ret_val;
}
static size_t priv_write(int fd, const void *buf, size_t count){
  thread_t *thd = thread_get_current();
  file *fh = thd->fd_table[fd];
  if(fh == NULL){
    uart_printf("Error, priv_write(), unrecognized fd=%d, pid=%d\r\n", fd, thd->pid);
    return 0;
  }

  return vfs_write(fh, (void*)buf, count);
}

// Return size read or error code
size_t        sysc_read(int fd, void *buf, size_t count){
  write_gen_reg(x8, SYSCALL_NUM_READ);
  write_gen_reg(x2, count);
  write_gen_reg(x1, buf);  // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, fd);   // so write to x0 should be the last one performed
  asm volatile("svc 0");
  size_t ret_val = read_gen_reg(x0);
  return ret_val;
}
static size_t priv_read(int fd, void *buf, size_t count){
  thread_t *thd = thread_get_current();
  file *fh = thd->fd_table[fd];
  if(fh == NULL){
    uart_printf("Error, priv_read(), unrecognized fd=%d, pid=%d\r\n", fd, thd->pid);
    return 0;
  }

  return vfs_read(fh, (void*)buf, count);
}

// you can ignore mode, since there is no access control
int           sysc_mkdir(const char *pathname, unsigned mode){
  write_gen_reg(x8, SYSCALL_NUM_MKDIR);
  write_gen_reg(x1, mode);      // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, pathname);  // so write to x0 should be the last one performed
  asm volatile("svc 0");
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_mkdir(const char *pathname, unsigned mode){
  thread_t *thd = thread_get_current();
  // Translate to absolute path
  char abs_path[TMPFS_MAX_PATH_LEN];
  abs_path[0] = '\0';
  to_abs_path(abs_path, thd->cwd, pathname);
  return vfs_mkdir((char*)abs_path);
}

// arguments other than target (where to mount) and filesystem (fs name) are ignored
int           sysc_mount(const char *src, const char *target, const char *filesystem, unsigned long flags, const void *data){
  write_gen_reg(x8, SYSCALL_NUM_MOUNT);
  write_gen_reg(x4, data);
  write_gen_reg(x3, flags);
  write_gen_reg(x2, filesystem);
  write_gen_reg(x1, target);  // write_gen_reg() seems to use x0 as buffer
  write_gen_reg(x0, src);     // so write to x0 should be the last one performed
  asm volatile("svc 0");
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_mount(const char *src, const char *target, const char *filesystem, unsigned long flags, const void *data){
  thread_t *thd = thread_get_current();
  // Translate to absolute path
  char abs_path[TMPFS_MAX_PATH_LEN];
  abs_path[0] = '\0';
  to_abs_path(abs_path, thd->cwd, target);
  return vfs_mount(abs_path, filesystem);
}

int           sysc_chdir(const char *path){
  write_gen_reg(x8, SYSCALL_NUM_CHDIR);
  write_gen_reg(x0, path);
  asm volatile("svc 0");
  int ret_val = read_gen_reg(x0);
  return ret_val;
}
static int    priv_chdir(const char *path){
  thread_t *thd = thread_get_current();
  char changed_path[TMPFS_MAX_PATH_LEN];
  int ret = 0;
  vnode *node = NULL;
  changed_path[0] = '\0';
  to_abs_path(changed_path, thd->cwd, path);
  if(changed_path[strlen_(changed_path)-1] != '/')
    strcat_(changed_path, "/");
  ret = vfs_lookup(changed_path, &node);
  if(ret == 0)
    strcpy_(thd->cwd, changed_path);

  uart_printf("Debug, priv_chdir(), pid=%d, path=%s, cwd=%s\r\n", thd->pid, path, thd->cwd);
  return ret;
}

static long   priv_lseek64(int fd, long offset, int whence){
  thread_t *thd = thread_get_current();
  file *fh = thd->fd_table[fd];
  if(fh == NULL){
    uart_printf("Error, priv_lseek64(), unrecognized fd=%d, pid=%d\r\n", fd, thd->pid);
    return 0;
  }
  return fh->f_ops->lseek64(fh, offset, whence);
}

#endif