#include "tmpfs.h"
#include "virtual_file_system.h"
#include "uart.h"
#include "diy_string.h"
#include "diy_malloc.h"


filesystem tmpfs = {.name="tmpfs", .setup_mount=tmpfs_setup_mount};
file_operations tmpfs_fops = {.write=tmpfs_write, .read=tmpfs_read, .open=tmpfs_open, .close=tmpfs_close};
vnode_operations tmpfs_vops = {.lookup=tmpfs_lookup, .mkdir=tmpfs_mkdir};


int tmpfs_write(file *file, const void *buf, size_t len){
  return 1;
}
int tmpfs_read(file *file, void *buf, size_t len){
  return 1;
}
int tmpfs_open(const char *pathname, file **target){
  return 1;
}
int tmpfs_close(file *file){
  return 1;
}

int tmpfs_setup_mount(struct filesystem *fs, mount *mount){
  if(mount == NULL || mount->root == NULL){
    uart_printf("Error, tmpfs_setup_mount(), NULL pointer.");
    uart_printf(" mount=%p, mount->root=%p\r\n", mount, mount==NULL ? NULL : mount->root);
  }
  mount->root->comp = diy_malloc(sizeof(vnode_comp));
  mount->root->comp->comp_name = "";
  mount->root->comp->len = 0;
  mount->root->comp->entries = NULL;
  mount->root->comp->type = COMP_DIR;
  mount->root->f_ops = &tmpfs_fops;
  mount->root->v_ops = &tmpfs_vops;
  return 0;
}

int tmpfs_mkdir(vnode *dir_node, vnode **target, const char *component_name){
  return 1;
}
int tmpfs_lookup(const char *pathname, vnode **target){
  return 1;
}



