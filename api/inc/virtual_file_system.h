
#ifndef __VIRTUAL_FILE_SYSTEM_H_
#define __VIRTUAL_FILE_SYSTEM_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define VFS_MAX_DEPTH 64
#define O_CREAT 0100 // flag for vfs_open()

typedef enum comp_type{
  COMP_FILE = 1,
  COMP_DIR  // directory
} comp_type;

typedef struct vnode{
  struct mount *mount;
  struct vnode_operations *v_ops;
  struct file_operations *f_ops;
  struct vnode_comp *comp;
} vnode;

typedef struct vnode_comp{ // vnode component
  char *comp_name;
  enum comp_type type;
  size_t len;       // COMP_DIR: entry count in this directory; COM_FILE: file size in byte
  union {
    vnode **entries; // for type of COMP_DIR
    char  *data;     // for type of COM_FILE
  };
} vnode_comp;

// file handle
typedef struct file{
  struct vnode *vnode;
  size_t f_pos;  // RW position of this file handle
  struct file_operations *f_ops;
  int flags;
} file;

typedef struct mount{
  struct vnode *root;
  struct filesystem *fs;
} mount;

typedef struct filesystem{
  const char *name;
  int (*setup_mount)(struct filesystem *fs, mount *mount);
} filesystem;

typedef struct file_operations{
  int  (*write)  (file *file, const void *buf, size_t len);
  int  (*read)   (file *file, void *buf, size_t len);
  int  (*open)   (const char *pathname, file **target);
  int  (*close)  (file *file);
  long (*lseek64)(file *file, long offset, int whence);
} file_operations;

typedef struct vnode_operations{
  int (*lookup)(const char *pathname, vnode **target);
  int (*create)(vnode *dir_node, vnode **target, const char *component_name);
  int (*mkdir) (vnode *dir_node, vnode **target, const char *component_name);
} vnode_operations;

extern mount root_mount;


int register_filesystem(filesystem *fs);
int vfs_open(char *pathname, int flags, file **file_handle);
int vfs_close(file *file);
int vfs_write(file *file, void *buf, size_t len);
int vfs_read(file *file, void *buf, size_t len);
int vfs_mkdir(char *pathname);
int vfs_mount(char *pathname, char *fs_name);
int vfs_lookup(char *pathname, vnode **target);

#ifdef __cplusplus
}
#endif
#endif  // __VIRTUAL_FILE_SYSTEM_H_
