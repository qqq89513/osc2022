
#include <stdint.h>
#include "diy_string.h"
#include "cpio.h"
#include "diy_sscanf.h"
#include "uart.h"
#include "virtual_file_system.h"
#include "tmpfs.h"
#include "diy_malloc.h"


static cpio_file_ll files_arr[CPIO_MAX_FILES];
static int mounted = 0;


static uint32_t header_get_mem(uint8_t* member, size_t size){
  static char str_buf[32];
  uint32_t ret = 0;
  memcpy_(str_buf, member, size);
  str_buf[size] = '\0';
  if(sscanf_(str_buf, "%8X", &ret) != 1)
    return -1;
  return ret;
}

static int pad_to_4(int num){
  int modded = num % 4;
  return modded==0 ? 0 : 4 - modded;
}

int cpio_parse(void *addr){
  cpio_newc_header_t *header = (cpio_newc_header_t*) addr;
  uint32_t magic_num = 0;
  uint32_t file_size = 0;
  uint32_t name_size = 0;
  uint8_t *data_ptr = NULL;
  uint8_t *file_name = NULL;
  cpio_file_ll *file = files_arr;
  while(1){
    // Check for magic num
    magic_num = header_get_mem(header->c_magic, sizeof(header->c_magic));
    if(magic_num != CPIO_MAGIC_NUM){
      uart_printf("[Error] magic_num=%d, should be %d instead.\r\n", magic_num, CPIO_MAGIC_NUM);
      file->next = NULL;
      return -1;
    }

    // Get file size and pathname size
    file_size = header_get_mem(header->c_filesize, sizeof(header->c_filesize));
    name_size = header_get_mem(header->c_namesize, sizeof(header->c_namesize));

    // Get file name (aka pathname)
    file_name = (uint8_t*)(&header[1]); // equivalent to ((u8*)header) + sizeof(header) or (u8*)(header+1)

    // Get file data address
    data_ptr = (uint8_t*)(&header[1]) + name_size + pad_to_4(name_size + sizeof(cpio_newc_header_t));
    
    // Get next address
    file->data_ptr  = data_ptr;
    file->file_size = file_size;
    file->pathname  = file_name;
    if(strcmp_(CPIO_END_RECORD, (const char*)file_name) != 0){
      header = (cpio_newc_header_t*)( data_ptr + file_size + pad_to_4(file_size) );
      file->next = file + 1; // should be malloc() here.
      file = file->next;
    }
    else{
      file->next = NULL;
      return 0;
    }
  }
}

void cpio_ls(){
  cpio_file_ll *file = files_arr;

  // Traverse the linked list
  while(file->next != NULL){
    uart_printf("%s, size=%d\r\n", file->pathname, file->file_size);
    file = file->next;
  }
}

// This function copies content of file_name to destination
int cpio_copy(char *file_name, uint8_t *destination){
  cpio_file_ll *file = files_arr;
  uint8_t *dest_ptr = destination;
  uint8_t *src_ptr = NULL;
  uint32_t size = 0;

  // Traverse the linked list
  while(file->next != NULL){
    if(strcmp_(file_name, (char*)file->pathname) == 0){
      src_ptr = file->data_ptr;
      size = file->file_size;
      // Copy executable to destination
      for(uint32_t i = 0; i < size; i++)
        *dest_ptr++ = *src_ptr++;

      uart_printf("cpio_copy: copied %s to %p, size=%u.\r\n", file_name, destination, size);
      return 0;
    }
    file = file->next;
  }
  uart_printf("cpio_copy: cannot access '%s': No such file or directory\r\n", file_name);
  return -1;
}

int cpio_cat(char *file_name){
  cpio_file_ll *file = files_arr;

  // Traverse the linked list
  while(file->next != NULL){
    if(strcmp_(file_name, (char*)file->pathname) == 0){
      // Dump for debug
      for(int i = 0; i < file->file_size; i++) uart_printf("%c", file->data_ptr[i]);
      uart_printf("\r\n");
      return 0;
    }
    file = file->next;
  }
  uart_printf("cat: cannot access '%s': No such file or directory\r\n", file_name);
  return -1;
}

// initramfs, API to virtual_file_system.h --------------------------------
filesystem initramfs = {.name="initramfs", .setup_mount=initramfs_setup_mount};
file_operations initramfs_fops = {.write=initramfs_write, .read=initramfs_read, .open=initramfs_open, .close=initramfs_close};
vnode_operations initramfs_vops = {.lookup=initramfs_lookup, .create=initramfs_create, .mkdir=initramfs_mkdir};

int initramfs_setup_mount(struct filesystem *fs, mount *mount){
  if(mount == NULL){
    uart_printf("Error, initramfs_setup_mount(), NULL pointer.");
  }
  mount->root = diy_malloc(sizeof(vnode));
  mount->fs = fs;
  mount->root->mount = NULL;
  mount->root->comp = diy_malloc(sizeof(vnode_comp));
  mount->root->comp->comp_name = "";
  mount->root->comp->len = 0;
  mount->root->comp->entries = NULL;
  mount->root->comp->type = COMP_DIR;
  mount->root->f_ops = &initramfs_fops;
  mount->root->v_ops = &initramfs_vops;

  if(mounted)
    return 0;

  // Insert files in the linked list to the file system
  cpio_file_ll *file = files_arr;
  vnode *dir_node = mount->root;
  vnode *node_new = NULL;
  while(file->next != NULL){

    // Skip 0 files
    if(file->file_size > 0){
      // Create entry
      lookup_recur((char*)file->pathname, dir_node, &node_new, 1);

      // Config entry to a file
      node_new->comp->type = COMP_FILE;
      node_new->comp->data = (char*)file->data_ptr;
      node_new->comp->len = file->file_size;
    }
    file = file->next;
  }
  mounted = 1;
  return 0;
}

// fops
int initramfs_write(file *file, const void *buf, size_t len){
  uart_printf("Error, initramfs_write(), cannot modify initramfs\r\n");
  return 0;
}
int initramfs_read(file *file, void *buf, size_t len){
  return tmpfs_read(file, buf, len);
}
int initramfs_open(vnode* file_node, file** target){
  return tmpfs_open(file_node, target);
}
int initramfs_close(file *file){
  return tmpfs_close(file);
}

// vops
int initramfs_mkdir(vnode *dir_node, vnode **target, const char *component_name){
  if(mounted){
    uart_printf("Error, initramfs_mkdir(), cannot modify initramfs\r\n");
    return 1;
  }
  else
    return tmpfs_mkdir(dir_node, target, component_name);
}
int initramfs_create(vnode *dir_node, vnode **target, const char *component_name){
  if(mounted){
    uart_printf("Error, initramfs_create(), cannot modify initramfs\r\n");
    return 1;
  }
  else
    return tmpfs_create(dir_node, target, component_name);
}
int initramfs_lookup(vnode *dir_node, vnode **target, const char *component_name){
  return tmpfs_lookup(dir_node, target, component_name);
}
