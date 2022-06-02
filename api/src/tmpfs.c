#include "tmpfs.h"
#include "virtual_file_system.h"
#include "uart.h"
#include "diy_string.h"
#include "diy_malloc.h"


filesystem tmpfs = {.name="tmpfs", .setup_mount=tmpfs_setup_mount};
file_operations tmpfs_fops = {.write=tmpfs_write, .read=tmpfs_read, .open=tmpfs_open, .close=tmpfs_close};
vnode_operations tmpfs_vops = {.lookup=tmpfs_lookup, .create=tmpfs_create, .mkdir=tmpfs_mkdir};


int tmpfs_write(file *file, const void *buf, size_t len){
  return 1;
}
int tmpfs_read(file *file, void *buf, size_t len){
  return 1;
}
int tmpfs_open(vnode *file_node, file **file_handle){

  // Return if file_node is not COMP_FILE
  if(file_node->comp->type != COMP_FILE){
    uart_printf("Error, tmpfs_open(), failed opening file_node_name=%s, file_node=0x%lX, type=%d, not file\r\n", 
      file_node->comp->comp_name, (uint64_t)file_node, file_node->comp->type);
    return 2;
  }

  // Create a new file handle for this vnode
  *file_handle = diy_malloc(sizeof(file));
  (*file_handle)->f_ops = file_node->f_ops;
  (*file_handle)->f_pos = 0;
  (*file_handle)->vnode = file_node;
  return 0;
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
  
  // Return if dir_node is not COMP_DIR
  if(dir_node->comp->type != COMP_DIR){
    uart_printf("Error, tmpfs_mkdir(), failed creating %s, dir_node_name=%s, dir_node=0x%lX, type=%d, not folder\r\n", 
      component_name, dir_node->comp->comp_name, (uint64_t)dir_node, dir_node->comp->type);
    return 2;
  }

  if(tmpfs_create(dir_node, target, component_name) == 0){
    (*target)->comp->type = COMP_DIR;
    return 0;
  }
  else{
    uart_printf("Error, tmpfs_mkdir(), failed creating %s, dir_node_name=%s, dir_node=0x%lX, type=%d, not folder\r\n", 
      component_name, dir_node->comp->comp_name, (uint64_t)dir_node, dir_node->comp->type);
    return 1;
  }
}
int tmpfs_create(vnode *dir_node, vnode **target, const char *component_name){

  // Return if dir_node is not COMP_DIR
  if(dir_node->comp->type != COMP_DIR){
    uart_printf("Error, tmpfs_create(), failed creating %s, dir_node_name=%s, dir_node=0x%lX, type=%d, not folder\r\n", 
      component_name, dir_node->comp->comp_name, (uint64_t)dir_node, dir_node->comp->type);
    return 2;
  }
  
  // Return if already exist
  vnode *entry = NULL;
  for(size_t i=0; i<dir_node->comp->len; i++){
    entry = dir_node->comp->entries[i];
    if(strcmp_(component_name, entry->comp->comp_name) == 0){
      uart_printf("Warning, tmpfs_create(), %s already exist under %s, entry=0x%lX, dir_node=0x%lX, \r\n",
        component_name, dir_node->comp->comp_name, (uint64_t)entry, (uint64_t)dir_node);
      *target = entry;
      return 1;
    }
  }

  // Return if TMPFS_MAX_ENTRY already created
  if(dir_node->comp->len >= TMPFS_MAX_ENTRY){
    uart_printf("Error, tmpfs_create(), failed creating %s, no more entry allowed\r\n", component_name);
    return 1;
  }
  
  // Create vnode, note that type is not specified here
  *target = diy_malloc(sizeof(vnode));
  (*target)->comp = diy_malloc(sizeof(vnode_comp));
  (*target)->comp->comp_name = diy_malloc(strlen_(component_name));
  strcpy_((*target)->comp->comp_name, component_name);
  (*target)->comp->data = NULL;
  (*target)->comp->len = 0;
  
  // Inherit from dir node
  (*target)->f_ops = dir_node->f_ops;
  (*target)->v_ops = dir_node->v_ops;
  (*target)->mount = dir_node->mount;
  
  // Update dir node
  if(dir_node->comp->entries == NULL) dir_node->comp->entries = diy_malloc(sizeof(vnode*) * TMPFS_MAX_ENTRY);
  dir_node->comp->entries[dir_node->comp->len] = *target;
  dir_node->comp->len++;

  return 0;
}
int tmpfs_lookup(vnode *dir_node, vnode **target, const char *component_name){

  // Return if dir_node is not COMP_DIR
  if(dir_node->comp->type != COMP_DIR){
    uart_printf("Error, tmpfs_lookup(), failed lookup %s, dir_node_name=%s, dir_node=0x%lX, type=%d, not folder\r\n", 
      component_name, dir_node->comp->comp_name, (uint64_t)dir_node, dir_node->comp->type);
    return 2;
  }

  vnode *entry = NULL;
  for(size_t i=0; i<dir_node->comp->len; i++){
    entry = dir_node->comp->entries[i];
    if(strcmp_(component_name, entry->comp->comp_name) == 0){
      *target = entry;
      return 0;
    }
  }

  // not found under dir_node
  return 1;
}



