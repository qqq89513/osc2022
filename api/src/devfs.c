#include "devfs.h"
#include "virtual_file_system.h"
#include "tmpfs.h"
#include "diy_string.h"
#include "uart.h"
#include "diy_malloc.h"
#include "mbox.h"
#include "general.h"

#define DEVFS_UART_NAME "uart"
#define DEVFS_FRAMEBUFFER_NAME "framebuffer"
#define SEEK_SET 0

filesystem devfs = {.name="devfs", .setup_mount=devfs_setup_mount};
file_operations devfs_fops = {.write=devfs_write, .read=devfs_read, .open=devfs_open, .close=devfs_close, .lseek64=devfs_lseek};
vnode_operations devfs_vops = {.lookup=devfs_lookup, .create=devfs_create, .mkdir=devfs_mkdir};

int devfs_setup_mount(struct filesystem *fs, mount *mount){
  if(mount == NULL){
    uart_printf("Error, devfs_setup_mount(), NULL pointer.");
  }
  mount->root = diy_malloc(sizeof(vnode));
  mount->fs = fs;
  mount->root->mount = NULL;
  mount->root->comp = diy_malloc(sizeof(vnode_comp));
  mount->root->comp->comp_name = "";
  mount->root->comp->len = 0;
  mount->root->comp->entries = NULL;
  mount->root->comp->type = COMP_DIR;
  mount->root->f_ops = &devfs_fops;
  mount->root->v_ops = &devfs_vops;
  vnode *dir_node = mount->root;

  // Create uart
  vnode *node_new = NULL;
  int ret = dir_node->v_ops->create(dir_node, &node_new, DEVFS_UART_NAME);
  if(ret == 0){
    node_new->comp->type = COMP_FILE;
  }
  else{
    uart_printf("Error, devfs_setup_mount(), failed to create uart, ret=%d\r\n", ret);
  }

  // Create and init framebuffer
  node_new = NULL;
  ret = dir_node->v_ops->create(dir_node, &node_new, DEVFS_FRAMEBUFFER_NAME);
  if(ret == 0){
    node_new->comp->type = COMP_FILE;
    node_new->comp->data = framebuffer_init();
  }
  else{
    uart_printf("Error, devfs_setup_mount(), failed to create framebuffer, ret=%d\r\n", ret);
  }

  return 0;
}


// fops
int devfs_write(file *file, const void *buf, size_t len){
  if     (strcmp_(file->vnode->comp->comp_name, DEVFS_UART_NAME) == 0){
    const char *ptr = buf;
    for(size_t i=0; i<len; i++)
      uart_send(*ptr++);
    return len;
  }
  else if(strcmp_(file->vnode->comp->comp_name, DEVFS_FRAMEBUFFER_NAME) == 0){
    vnode_comp *comp = file->vnode->comp;
    const char *ptr = buf;
    char *dest = (char*)((uint64_t)comp->data + file->f_pos);
    for(size_t i=0; i<len/sizeof(char); i++)
      *dest++ = *ptr++;
    file->f_pos += len;
    // uart_printf("Debug, devfs_write(), f_pos wrote to %ld\r\n", file->f_pos);
    // uint64_t tk = 0;
    // WAIT_TICKS(tk, 1000);
    return len;
  }
  else{
    uart_printf("Error, devfs_write(), writing to unrecognized device %s\r\n", file->vnode->comp->comp_name);
    return 0;
  }
}
int devfs_read(file *file, void *buf, size_t len){
  if(strcmp_(file->vnode->comp->comp_name, DEVFS_UART_NAME) == 0){
    char *ptr = buf;
    for(size_t i=0; i<len; i++)
      *ptr++ = uart_read_byte();
    return len;
  }
  else{
    uart_printf("Error, devfs_read(), reading from unrecognized device %s\r\n", file->vnode->comp->comp_name);
    return 0;
  }
}
int devfs_open(vnode* file_node, file** target){
  return tmpfs_open(file_node, target);
}
int devfs_close(file *file){
  return tmpfs_close(file);
}
long devfs_lseek(file *file, long offset, int whence){
  if(whence == SEEK_SET){
    file->f_pos = (size_t)offset;
    // uart_printf("Debug, devfs_lseek(), f_pos set to %ld\r\n", file->f_pos);
    return file->f_pos;
  }
  else{
    uart_printf("Error, devfs_lseek(), unknown whence=%d\r\n", whence);
    return -1;
  }
}

// vops
int devfs_mkdir(vnode *dir_node, vnode **target, const char *component_name){
  uart_printf("Error, devfs_mkdir(), cannot mkdir with devfs\r\n");
  return 1;
}
int devfs_create(vnode *dir_node, vnode **target, const char *component_name){
  return tmpfs_create(dir_node, target, component_name);
}
int devfs_lookup(vnode *dir_node, vnode **target, const char *component_name){
  return tmpfs_lookup(dir_node, target, component_name);
}



