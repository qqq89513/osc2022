#ifndef __TMPFS_H_
#define __TMPFS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "virtual_file_system.h"

#define TMPFS_MAX_PATH_LEN 255
#define TMPFS_MAX_COMPONENT_NAME 32
#define TMPFS_MAX_ENTRY 16

extern filesystem tmpfs;

// fops
int tmpfs_write(file *file, const void *buf, size_t len);
int tmpfs_read(file *file, void *buf, size_t len);
int tmpfs_open(const char *pathname, file **target);
int tmpfs_close(file *file);

// vops
int tmpfs_mkdir(vnode *dir_node, vnode **target, const char *component_name);
int tmpfs_create(vnode *dir_node, vnode **target, const char *component_name);
int tmpfs_lookup(vnode* dir_node, vnode **target, const char* component_name);

int tmpfs_setup_mount(struct filesystem *fs, mount *mount);

#ifdef __cplusplus
}
#endif
#endif  // __TMPFS_H_
