// Minimal implementation for OS interface of newlibc
// These remove the warning from linker like: "warning: _close is not implemented and will always fail"
// Ref: https://sourceware.org/newlib/libc.html#Stubs

#include <unistd.h>
#include <sys/stat.h>
#include <sys/times.h>
#include "uart.h"

int _close(int file){
  return -1;
}
int _execve(char *name, char **argv, char **env){
  return -1;
}
int _fork(void){
  return -1;
}
int _fstat(int file, struct stat *st) {
  return 0;
}
int _getpid(void){
  return 1;
}
int _isatty(int file){
  return 1;
}
int _kill(int pid, int sig){
  return -1;
}
int _link(char *old, char *new_) {
  return -1;
}
int _lseek(int file, int ptr, int dir) {
  return 0;
}
int _open(const char *name, int flags, int mode) {
  return -1;
}
int _read(int file, char *ptr, int len) {
  return 0;
}
int _stat(char *file, struct stat *st) {
  return 0;
}
int _times(struct tms *buf) {
  return -1;
}
int _unlink(char *name) {
  return -1; 
}
int _wait(int *status) {
  return -1;
}

int _write(int file, char *ptr, int len) {
  return -1;
}

