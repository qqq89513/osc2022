#include "virtual_file_system.h"
#include "tmpfs.h"
#include "diy_string.h"
#include "uart.h"
#include "diy_malloc.h"

vnode root_vnode;
mount root_mount = {.fs=NULL, .root=&root_vnode};

static int lookup_priv(char *pathname, vnode *dir_node, vnode **node_found);
static char *skip_first_comp(char *pathname);
static int first_component(char *pathname, char *comp_name);

// Return 0 on found, 1 not found, 2 or 3 on error. node_found will be set to as deep as possible
static int lookup_priv(char *pathname, vnode *dir_node, vnode **node_found){
  char *rest_path = NULL;
  char comp_name[TMPFS_MAX_COMPONENT_NAME];
  rest_path = skip_first_comp(pathname);
  first_component(pathname, comp_name);

  // Return if dir_node is not COMP_DIR
  if(dir_node->comp->type != COMP_DIR){
    uart_printf("Error, lookup_priv(), failed, pathname=%s, search_under_name=%s, dir_node=0x%lX, type=%d, not folder\r\n", 
      pathname, dir_node->comp->comp_name, (uint64_t)dir_node, dir_node->comp->type);
    return 2;
  }

  // Search comp_name under dir_node
  if(dir_node->v_ops->lookup(dir_node, node_found, comp_name) == 0){
    dir_node = *node_found;

    // More to lookup and can lookup deeper
    if     (rest_path[0] != '\0' && dir_node->comp->type == COMP_DIR)
      return lookup_priv(rest_path, dir_node, node_found);

    // More to lookup and cannot lookup deeper, return not found
    else if(rest_path[0] != '\0' && dir_node->comp->type == COMP_FILE)
      return 1;

    // No more to lookup, return found
    else if(rest_path[0] == '\0')
      return 0; // pathname found

    // Exception
    else{
      uart_printf("Exception, lookup_priv(), should not get here, dir_node=%lX, comp_name=%s, type=%d\r\n",
        (uint64_t)dir_node, dir_node->comp->comp_name, dir_node->comp->type);
      return 3;
    }
  }

  // comp_name not found under dir_node
  else
    return 1;
}
static char *skip_first_comp(char *pathname){ // return the string pointer right after '/' or '\0'
  int i = 0;
  if(pathname[i] == '/') i++;                           // skip leading '/'
  while(pathname[i] != '\0' && pathname[i] != '/') i++; // skip first directory
  if(pathname[i] == '/') i++;                           // skip leading '/'
  return &pathname[i];
}
static int first_component(char *pathname, char *comp_name){
  int i = 0;
  if(pathname[i] == '/') pathname++;                    // skip leading '/'
  while(pathname[i] != '\0' && pathname[i] != '/'){     // copy until end of string or '/'
    comp_name[i] = pathname[i];
    i++;
  }
  comp_name[i] = '\0';
  // uart_printf("first_component(), pathname=%s, comp_name=%s\r\n", pathname, comp_name);
  return 0;
}


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
    const int ret = mount_at_node->mount->fs->setup_mount(&tmpfs, mount_at_node->mount); // init of tmpfs
    vnode *node = NULL;
    vnode *dirnode = NULL;
    tmpfs_create(&root_vnode, &node, "file1"); node->comp->type = COMP_FILE;
    tmpfs_create(&root_vnode, &node, "file2"); node->comp->type = COMP_FILE;
    tmpfs_create(&root_vnode, &node, "dir0"); node->comp->type = COMP_DIR;
    tmpfs_create(&root_vnode, &node, "dir8"); node->comp->type = COMP_DIR;
    dirnode = node;
    tmpfs_create(dirnode, &node, "under8_file0"); node->comp->type = COMP_FILE;
    tmpfs_create(dirnode, &node, "under8_dir0"); node->comp->type = COMP_DIR;
    tmpfs_create(dirnode, &node, "under8_dir1"); node->comp->type = COMP_DIR;
    tmpfs_create(dirnode, &node, "under8_dir2"); node->comp->type = COMP_DIR;
    dirnode = node;
    tmpfs_create(dirnode, &node, "more_inner_dir0"); node->comp->type = COMP_DIR;
    tmpfs_create(dirnode, &node, "more_inner_dir1"); node->comp->type = COMP_DIR;
    tmpfs_create(dirnode, &node, "more_inner_file.txt"); node->comp->type = COMP_FILE;
    vfs_dump_root();
    if(vfs_lookup("/dir8/under8_dir2/more_inner_file.txt", &node) == 0){
      uart_printf("Found!, node=0x%lX\r\n", (uint64_t)node);
    }else
      uart_printf("Not found!, node=0x%lX\r\n", (uint64_t)node);
    return ret;
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
    return lookup_priv(pathname, &root_vnode, target);
  }
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


void vfs_dump_under(vnode *node, int depth){
  if(node->comp->type == COMP_DIR){
    vnode *entry = NULL;
    for(size_t i=0; i<node->comp->len; i++){
      entry = node->comp->entries[i];
      for(int k=0; k<depth; k++) uart_printf("    ");
      uart_printf("%lu, 0x%lX, %s, type=%d, len=%lu\r\n", 
        i, (uint64_t)entry, entry->comp->comp_name, entry->comp->type, entry->comp->len);
      if(entry->comp->type == COMP_DIR)
        vfs_dump_under(entry, depth + 1);
    }
  }
  else{
    uart_printf("Error, vfs_dump_under(), node type is not dir. type=%d, name=%s\r\n", 
      node->comp->type, node->comp->comp_name);
  }
}

void vfs_dump_root(){
  vfs_dump_under(&root_vnode, 0);
}
