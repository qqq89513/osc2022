#include "tmpfs.h"
#include "virtual_file_system.h"
#include "uart.h"
#include "diy_string.h"
#include "diy_malloc.h"


filesystem tmpfs = {.name="tmpfs", .setup_mount=tmpfs_setup_mount};
file_operations tmpfs_fops = {.write=tmpfs_write, .read=tmpfs_read, .open=tmpfs_open, .close=tmpfs_close};
vnode_operations tmpfs_vops = {.lookup=tmpfs_lookup, .create=tmpfs_create, .mkdir=tmpfs_mkdir};


int tmpfs_write(file *file, const void *buf, size_t len){

  // Return on null pointers
  if(file == NULL){
    uart_printf("Error, tmpfs_write(), file=NULL\r\n");
    return 1;
  }
  vnode *node = file->vnode;
  if(node == NULL){
    uart_printf("Error, tmpfs_write(), file->vnode=NULL, file=0x%lX\r\n", (uint64_t)file);
    return 1;
  }
  vnode_comp *comp = node->comp;
  if(node == NULL){
    uart_printf("Error, tmpfs_write(), node->comp=NULL, file=0x%lX, node=0x%lX\r\n", (uint64_t)file, (uint64_t)node);
    return 1;
  }

  // Return if file_node is not COMP_FILE
  if(comp->type != COMP_FILE){
    uart_printf("Error, tmpfs_write(), failed opening node_name=%s, node=0x%lX, type=%d, not file\r\n", 
      comp->comp_name, (uint64_t)node, comp->type);
    return 2;
  }

  // Reallocate new space if current size is not big enough
  const size_t ideal_final_pos = file->f_pos + len;
  if(ideal_final_pos > comp->len && comp->len < TMPFS_MAX_FILE_SIZE){
    size_t new_len = ideal_final_pos + 512; // always allocate 512 bytes more
    new_len = new_len > TMPFS_MAX_FILE_SIZE ? TMPFS_MAX_FILE_SIZE : new_len;  // truncate to TMPFS_MAX_FILE_SIZE
    char *new_space = diy_malloc(sizeof(char) * new_len);
    if(comp->len > 0){
      memcpy_(new_space, comp->data, comp->len);
      memset_(new_space+comp->len, 0, new_len - comp->len); // clear the rest
      memset_(comp->data, 0, comp->len);                    // clear before free
      diy_free(comp->data);
    }
    comp->len = new_len;
    comp->data = new_space;
  }

  const size_t wrtie_able = ideal_final_pos >= TMPFS_MAX_FILE_SIZE ? (TMPFS_MAX_FILE_SIZE-file->f_pos) : len;
  memcpy_(comp->data + file->f_pos, buf, wrtie_able);
  file->f_pos += wrtie_able;
  return wrtie_able;
}
int tmpfs_read(file *file, void *buf, size_t len){

  // Return on null pointers
  if(file == NULL){
    uart_printf("Error, tmpfs_read(), file=NULL\r\n");
    return 1;
  }
  vnode *node = file->vnode;
  if(node == NULL){
    uart_printf("Error, tmpfs_read(), file->vnode=NULL, file=0x%lX\r\n", (uint64_t)file);
    return 1;
  }
  vnode_comp *comp = node->comp;
  if(node == NULL){
    uart_printf("Error, tmpfs_read(), node->comp=NULL, file=0x%lX, node=0x%lX\r\n", (uint64_t)file, (uint64_t)node);
    return 1;
  }

  // Return if file_node is not COMP_FILE
  if(comp->type != COMP_FILE){
    uart_printf("Error, tmpfs_read(), failed opening node_name=%s, node=0x%lX, type=%d, not file\r\n", 
      comp->comp_name, (uint64_t)node, comp->type);
    return 2;
  }

  const size_t ideal_final_pos = file->f_pos + len;
  const size_t read_able = ideal_final_pos >= comp->len ? (comp->len - file->f_pos) : len;
  memcpy_(buf, comp->data, read_able);
  file->f_pos += read_able;
  return read_able;
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
  if(file == NULL){
    uart_printf("Error, tmpfs_close(), file=NULL\r\n");
    return 1;
  }
  memset_(file, 0, sizeof(file)); // not necessarily
  diy_free(file);
  return 0;
}

int tmpfs_setup_mount(struct filesystem *fs, mount *mount){
  if(mount == NULL){
    uart_printf("Error, tmpfs_setup_mount(), NULL pointer.");
  }
  mount->root = diy_malloc(sizeof(vnode));
  mount->fs = fs;
  mount->root->mount = NULL;
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
  (*target)->mount = NULL; // no mount point
  
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
      *target = entry->mount == NULL ? entry : (entry->mount->root);
      return 0;
    }
  }

  // not found under dir_node
  return 1;
}



