#include "virtual_file_system.h"
#include "tmpfs.h"
#include "diy_string.h"
#include "uart.h"
#include "diy_malloc.h"

vnode root_vnode;
mount root_mount = {.fs=NULL, .root=&root_vnode};

static int lookup_priv(char *pathname, vnode *search_under, vnode **node_found);
static char *after_slash(char *pathname);

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

int vfs_mount(char *pathname, char *fs_name){
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

// Return 0 if path exists, non-0 otherwise
int vfs_lookup(char *pathname, vnode **target){
  if(strcmp_(pathname, "/") == 0){
    *target = root_mount.root;
    return 0;
  }
  else{
    char *rest_path = after_slash(pathname);
    lookup_priv(rest_path, &root_vnode, target);
    return 1;
  }
}
static int lookup_priv(char *pathname, vnode *search_under, vnode **node_found){
  char *rest_path = NULL;
  switch(search_under->comp->type){
    case COMP_FILE:
      return strcmp_(pathname, search_under->comp->comp_name); // return 0 if match
    case COMP_DIR:
      rest_path = after_slash(pathname);
      // Search under directory
      for(size_t i; i<search_under->comp->len; i++){
        if(lookup_priv(after_slash, &search_under->comp->entries[i], node_found) == 0){
          if(node_found != NULL) *node_found = &search_under->comp->entries[i];
          return 0;
        }
      }
      return 1; // not found under vnode search_under
    default:
      uart_printf("Error, lookup_priv(), unknow node type=%d, name=%s, pathname=%s\r\n", 
        search_under->comp->type, search_under->comp->comp_name, pathname);
      return 2;
  }
}
static char *after_slash(char *pathname){ // return the string pointer right after '/' or '\0'
  int i = 0;
  while(pathname[i] != '\0' && pathname[i] != '/') i++; // skip first directory
  if(pathname[i] == '/') i++;                           // skip leading '/'
  return &pathname[i];
}

int vfs_open(char *pathname, int flags, file **target){
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

int vfs_write(file *file, void *buf, size_t len){
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
