#include "virtual_file_system.h"
#include "tmpfs.h"
#include "diy_string.h"
#include "uart.h"
#include "diy_malloc.h"

#define CURRENT_DIR "."
#define PARENT_DIR ".."

vnode root_vnode;
mount root_mount = {.fs=NULL, .root=&root_vnode};

static int lookup_priv(char *pathname, vnode *dir_node, vnode **node_found, int create);
static char *skip_first_comp(char *pathname);
static int first_component(char *pathname, char *comp_name);

/** Search pathname under dir_node recursively. Return 0 on found. Create if requested.
 * @param pathname: relative path to dir_node, if dir_node is root_vnode, than this is abs path
 * @param dir_node: Directory node to look under
 * @param node_found: Output of this function. The node pointer if file/dir specified with pathname exists.
 *  If not exist, node_found is set to the last node that match partial pathname.
 * @param create: Set to 1 to create entry if the path not exist, 0 to do nothing. The created node is set to COMP_DIR, can be changed after
 * @return 0 on found, non-0 otherwise. If create==1 and not found, return create()
*/
static int lookup_priv(char *pathname, vnode *dir_node, vnode **node_found, int create){
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
      return lookup_priv(rest_path, dir_node, node_found, create);

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

  // Create entry iteratively if not found
  // Should be refactored, too many return point in this function. but I'm lazy
  else if(create == 1){
    char *rest_comps[VFS_MAX_DEPTH];
    char path_copy[TMPFS_MAX_PATH_LEN];
    int comp_count = 0;
    int mkdir_ret = 0;
    // uart_printf("Debug, lookup_priv(), path=%s, rest=%s, comp=%s\r\n", pathname, rest_path, comp_name);
    // copy string since rest_path is from pathname, where pathname should not be changed
    strcpy_(path_copy, pathname);
    comp_count = str_spilt(rest_comps, path_copy, "/");
    for(int i=0; i<comp_count; i++){
      if(rest_comps[i][0] == '\0') continue; // skip NULL strings
      // uart_printf("Debug, lookup_priv(), create, i=%d, dir=0x%lX, %s\r\n", i, (uint64_t)dir_node, rest_comps[i]);
      mkdir_ret = dir_node->v_ops->mkdir(dir_node, node_found, rest_comps[i]);
      if(mkdir_ret == 0){
        dir_node = *node_found;
      }
      else{
        uart_printf("Error, lookup_priv(), failed to create(), %s, rest_comps[%d]=%s\r\n", path_copy, i, rest_comps[i]);
        return mkdir_ret;
      }
    }
    return 0;
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

/** Translate (absolute/relative) path to absolute path. "." and ".." is also handled.
 * @param abs_path: output abs path
 * @param cwd: current working directory, used when @param path is relative path. Starts and ends with '/'
 * @param path: Start with '/' for absolute path, relative path otherwise
 * @return 0 on success, 1 on cwd either starts or ends without '/'
 * @note If ".." meets root "/", ".." accumulates at the begining.
*/
int to_abs_path(char *abs_path, const char *cwd, const char *path){

  // Input abs path, copy directly and return
  if(path[0] == '/'){
    strcpy_(abs_path, path);
    return 0;
  }

  // Return if cwd not starts or not ends with '/'
  if(cwd[0] != '/' || cwd[strlen_(cwd)-1] != '/'){
    uart_printf("Error, to_abs_path(), cwd should stars and ends with \"/\", cwd=%s\r\n", cwd);
    return 1;
  }

  // Concatenate cwd and input path
  char untrans[TMPFS_MAX_PATH_LEN]; // "." and ".." are untranslated yet
  untrans[0] = '\0';
  strcat_(untrans, cwd);
  strcat_(untrans, path);

  // Spilt string by "/"
  char *comps[VFS_MAX_DEPTH]; // component string array
  int count = str_spilt(comps, untrans, "/");

  // Translate "." and ".."
  int w = 0;  // write(update) index, w-- is like pop, w++ is like push. in place FILO
  int r = 0;  // read index, always advance by 1. in place FIFO
  while(w < count && r < count){
    // Skip blank and "."
    while(comps[r][0] == '\0' || strcmp_(comps[r], CURRENT_DIR) == 0)
      r++;

    // Handle ".."
    if(strcmp_(comps[r], PARENT_DIR) == 0){
      // when stack empty (w==0) or stack top is "..", then push ".."
      if(w == 0 || strcmp_(comps[w-1], PARENT_DIR)==0)
        comps[w++] = comps[r];   // push ".."
      else
        comps[w--] = NULL;    // pop, just "w--;" also does the job
    }

    // Push normal component
    else
      comps[w++] = comps[r];

    r++;
  }

  count = w; // items after w are ignored
  abs_path[0] = '\0'; // clear string
  for(int i=0; i<count; i++){
    strcat_(abs_path, "/");
    strcat_(abs_path, comps[i]);
  }
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
    char abs_path[255];
    to_abs_path(abs_path, "/hello/", "text/../../../lr/../hello2/./bull/da/disapper/../apper");
    uart_printf("abs_path=%s\r\n", abs_path);
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
    return lookup_priv(pathname, &root_vnode, target, 0);
  }
}

int vfs_open(char *pathname, int flags, file **file_handle){
  vnode *node = NULL;
  int ret = 0;

  // Lookup pathname
  ret = vfs_lookup(pathname, &node);
  
  // Create a new file if vnode not found and O_CREAT
  if(ret != 0 && (flags & O_CREAT)){
    ret = lookup_priv(pathname, &root_vnode, &node, 1); // create via lookup_priv()
    if(ret == 0) node->comp->type = COMP_FILE;
    else{
      uart_printf("Exception, vfs_open(), failed to create %s, ret=%d\r\n", pathname, ret);
      return ret;
    }
  }
  else if(ret != 0){
    uart_printf("Error, vfs_open(), no such file %s\r\n", pathname);
    return ret;
  }

  // Open file through fops of this node
  ret = node->f_ops->open(node, file_handle);
  if(ret == 0)
    (*file_handle)->flags = flags;
  else{
    uart_printf("Error, vfs_open(), node->f_ops->open() failed, ret=%d", ret);
    uart_printf(", path=%s", pathname);
    uart_printf(", node=0x%lX, %s, type=%d, len=%lu\r\n", 
      (uint64_t)node, node->comp->comp_name, node->comp->type, node->comp->len);
  }
  return ret;
}

int vfs_close(file *file){
  // 1. release the file handle
  // 2. Return error code if fails
  return file->f_ops->close(file);
}

int vfs_write(file *file, void *buf, size_t len){
  // 1. write len byte from buf to the opened file.
  // 2. return written size or error code if an error occurs.
  return file->f_ops->write(file, buf, len);
}

int vfs_read(file *file, void *buf, size_t len){
  // 1. read min(len, readable size) byte to buf from the opened file.
  // 2. block if nothing to read for FIFO type
  // 2. return read size or error code if an error occurs.
  return file->f_ops->read(file, buf, len);
}

int vfs_mkdir(char *pathname){
  vnode *node = NULL;
  if(vfs_lookup(pathname, &node) == 0){
    uart_printf("Error, vfs_mkdir(), cannot make directory, %s already exist\r\n", pathname);
    return 1;
  }
  else{
    vnode *node = NULL;
    int ret = lookup_priv(pathname, &root_vnode, &node, 1);
    if(ret != 0){
      uart_printf("Error, vfs_mkdir(), failed to create %s", pathname);
      uart_printf(", last entry 0x%lX, %s, type=%d, len=%lu\r\n", 
        (uint64_t)node, node->comp->comp_name, node->comp->type, node->comp->len);
    }
    return ret;
  }
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
