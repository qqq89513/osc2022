#include "virtual_file_system.h"
#include "tmpfs.h"
#include "diy_string.h"
#include "uart.h"
#include "diy_malloc.h"

vnode root_vnode;
mount root_mount = {.fs=NULL, .root=&root_vnode};

int register_filesystem(filesystem *fs){
  // register the file system to the kernel.

  if(fs == NULL){
    uart_printf("Error, register_filesystem(), NULL pointer\r\n");
    return -1;
  }

  if(fs == &tmpfs){
    return 0;
  }
  else{
    uart_printf("Error, register_filesystem(), file system %s is not supported\r\n", fs->name);
    return -1;
  }

  return -1;
}

int vfs_mount(const char *pathname, const char *fs_name){
  vnode *mount_at_node = NULL;

  // Search for the mount point, return if not found
  if(vfs_lookup(pathname, &mount_at_node) != 0){
    uart_printf("Error, vfs_mount(), pathname %s not found\r\n", pathname);
    return 1;
  }

  if(strcmp_(fs_name, tmpfs.name) == 0){
    mount_at_node->mount = diy_malloc(sizeof(mount));
    mount_at_node->mount->fs = &tmpfs;
    mount_at_node->mount->root = mount_at_node;  // mount_at_node is the root node of this mount
    return mount_at_node->mount->fs->setup_mount(&tmpfs, mount_at_node->mount); // init of tmpfs
  }
  else{
    uart_printf("Error, vfs_mount(), file system %s is not supported\r\n", fs_name);
    return -1;
  }
}

int vfs_lookup(const char *pathname, vnode **target){
  if(strcmp_(pathname, "/") == 0){
    *target = root_mount.root;
    return 0;
  }
  else
    return 1;
    
}

int vfs_open(const char *pathname, int flags, file **target){
  // 1. Lookup pathname
  // 2. Create a new file handle for this vnode if found.
  // 3. Create a new file if O_CREAT is specified in flags and vnode not found
  // lookup error code shows if file exist or not or other error occurs
  // 4. Return error code if fails
  return 1;
}

int vfs_close(file *file){
  // 1. release the file handle
  // 2. Return error code if fails
  return 1;
}

int vfs_write(file *file, const void *buf, size_t len){
  // 1. write len byte from buf to the opened file.
  // 2. return written size or error code if an error occurs.
  return 1;
}

int vfs_read(file *file, void *buf, size_t len){
  // 1. read min(len, readable size) byte to buf from the opened file.
  // 2. block if nothing to read for FIFO type
  // 2. return read size or error code if an error occurs.
  return 1;
}
