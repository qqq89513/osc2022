#include "fat32_on_SD.h"

#include "sd.h"
#include "virtual_file_system.h"
#include "uart.h"
#include "tmpfs.h"
#include "diy_string.h"
#include "diy_malloc.h"
#include <stdint.h>

/*
  Ref[1]: https://wiki.osdev.org/MBR_(x86)
  Ref[2]: https://wiki.osdev.org/FAT#Boot_Record
  Ref[3]: https://github.com/LJP-TW/osc2022
  Ref[4]: https://www.easeus.com/resource/fat32-disk-structure.htm
  Ref[5]: https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
*/


typedef struct partition_t {
  uint8_t status;
  uint8_t chss_head;
  uint8_t chss_sector;
  uint8_t chss_cylinder;
  uint8_t type;
  uint8_t chse_head;
  uint8_t chse_sector;
  uint8_t chse_cylinder;
  uint32_t lba;
  uint32_t sectors;
} __attribute__((packed)) partition_t;

typedef struct MBR_t {
  uint8_t bootstrap[440];
  uint8_t unique_disk_id[4];
  uint8_t reserved[2];
  partition_t part1;
  partition_t part2;
  partition_t part3;
  partition_t part4;
  uint8_t valid_bootsector[2]; // 0x55, 0xAA
} MBR_t;

// page 7, Ref[5]
typedef struct boot_sector_t  {
  uint8_t jmpboot[3];
  uint8_t oemname[8];
  uint16_t bytes_per_sector;
  uint8_t sector_per_cluster;
  uint16_t reserved_sector_cnt;   // Number of reserved sectors in the reserved region of the volume starting at the first sector of the volume. This field is used to align the start of the data area to integral multiples of the cluster size with respect to the start of the partition/media
  uint8_t fat_cnt;  // The count of file allocation tables (FATs) on the volume
  uint16_t root_entry_cnt;
  uint16_t old_sector_cnt;
  uint8_t media;
  uint16_t sector_per_fat16;
  uint16_t sector_per_track;
  uint16_t head_cnt;
  uint32_t hidden_sector_cnt;
  uint32_t sector_cnt;
  uint32_t sector_per_fat32;  // This field is the FAT32 32-bit count of sectors occupied by one FAT. 
  uint16_t extflags;
  uint16_t ver;
  uint32_t root_cluster;
  uint16_t info; // Sector number of FSINFO structure in the reserved area of the FAT32 volume. Usually 1.
  uint16_t bkbooksec;
  uint8_t reserved[12];
  uint8_t drvnum;
  uint8_t reserved1;
  uint8_t bootsig;
  uint32_t volid;
  uint8_t vollab[11];
  uint8_t fstype[8];
  uint8_t skipped[420];
  uint8_t valid_bootsector[2]; // 0x55, 0xAA
} __attribute__((packed)) boot_sector_t;

// page 23, Ref[5]
typedef struct dir_entry  {
  char name[11];        // 0~10
  uint8_t attr;         // 11
  uint8_t NTRes;        // 12
  uint8_t crtTimeTenth; // 13
  uint16_t crtTime;     // 14~15
  uint16_t crtDate;     // 16~17
  uint16_t lstAccDate;  // 18~19
  uint16_t fstClusHI;   // 20~21, High word of first data cluster number for file/directory described by this entry.
  uint16_t wrtTime;     // 22~23
  uint16_t wrtDate;     // 24~26
  uint16_t fstClusLO;   // 26~27, Low word of first data cluster number for file/directory described by this entry.
  uint32_t fileSize;    // 28~31, 32-bit quantity containing size in bytes of file/directory described by this entry. 
} __attribute__((packed)) dir_entry;


// VFS bridge -----------------------------------------------------------
static int fat32fs_mounted = 0;
static int fat32fs_mount(struct filesystem *fs, struct mount *mount);

// fops
static int fat32fs_write(file *file, const void *buf, size_t len);
static int fat32fs_read(file *file, void *buf, size_t len);
static int fat32fs_open(vnode* file_node, file** target);
static int fat32fs_close(file *file);

// vops
static int fat32fs_mkdir(vnode *dir_node, vnode **target, const char *component_name);
static int fat32fs_create(vnode *dir_node, vnode **target, const char *component_name);
static int fat32fs_lookup(vnode *dir_node, vnode **target, const char *component_name);

filesystem fat32fs = {
  .name = "fat32fs",
  .setup_mount = fat32fs_mount
};
file_operations fat32fs_fops = {.write=fat32fs_write, .read=fat32fs_read, .open=fat32fs_open, .close=fat32fs_close};
vnode_operations fat32fs_vops = {.lookup=fat32fs_lookup, .create=fat32fs_create, .mkdir=fat32fs_mkdir};

static int fat32fs_mount(filesystem *fs, mount *mount){
  const MBR_t *mbr;
  uint32_t lba;
  uint8_t buf[SD_BLOCK_SIZE];

  readblock(0, buf);
  mbr = (MBR_t*) buf;

  // MBR check
  if(mbr->valid_bootsector[0] != 0x55 || mbr->valid_bootsector[1] != 0xAA){
    uart_printf("Exception, fat32fs_mount(), mbr->valid_bootsector=0x%02X%02X, instead of 0xAA55\r\n", 
      mbr->valid_bootsector[1], mbr->valid_bootsector[0]);
    return -1;
  }
  if (mbr->part1.type != 0x0B && mbr->part1.type != 0x0C) { // 0x0B for fat32, 0x0C for fat32 with LBA 0x13 extension
    uart_printf("Exception, fat32fs_mount(), unexpected partition type = 0x%02d\r\n", mbr->part1.type);
    return -1;
  }

  lba = mbr->part1.lba;

  readblock(lba, buf);

  uart_printf("First partition, =");
  for(int i=0; i<SD_BLOCK_SIZE; i++) uart_printf("%02X ", buf[i]);
  uart_printf("\r\n");

  boot_sector_t *VBR = (boot_sector_t*) buf;
  uint32_t FAT1_lba = lba + VBR->reserved_sector_cnt;
  uint32_t root_dir_lba = FAT1_lba + (VBR->fat_cnt * VBR->sector_per_fat32);
  uint32_t root_cluster = VBR->root_cluster;
  readblock(root_dir_lba, buf);
  dir_entry *root_dir_entry = (dir_entry*) buf;

  mount->root = diy_malloc(sizeof(vnode));
  mount->fs = fs;
  mount->root->mount = NULL;
  mount->root->comp = diy_malloc(sizeof(vnode_comp));
  mount->root->comp->comp_name = "";
  mount->root->comp->len = 0;
  mount->root->comp->lba = root_dir_lba;
  mount->root->comp->type = COMP_DIR;
  mount->root->f_ops = &fat32fs_fops;
  mount->root->v_ops = &fat32fs_vops;

  // Insert files in the root_dir_entry to the file system
  vnode *dir_node = mount->root;
  vnode *node_new = NULL;
  const dir_entry *item = (dir_entry*) root_dir_entry;
  for(int i=0; i<SD_BLOCK_SIZE/sizeof(dir_entry); i++){
    
    // Skip empty entry
    if(item[i].name[i] != '\0'){
      char *name = diy_malloc(sizeof(item->name) + 2); // +1 for . in fileName.ext and end of string
      
      // Copy entry name
      int j = 0;  // name[j]
      int k = 0;  // item[i].name[k];
      for(k=0; k<8 && item[i].name[k] != ' '; k++)   name[j++] = item[i].name[k];
      name[j++] = '.';
      for(k=8; k<sizeof(item->name) && item[i].name[k] != ' '; k++)   name[j++] = item[i].name[k];
      name[j] = '\0';

      // Create entry along tmpfs
      lookup_recur((char*)name, dir_node, &node_new, 1);
      
      // Config entry to a file
      node_new->comp->type = COMP_FILE;
      node_new->comp->lba =  root_dir_lba + (item[i].fstClusHI << 16 | item[i].fstClusLO) - root_cluster; 
      node_new->comp->len = item[i].fileSize;
    }
  }

  fat32fs_mounted = 1;
  return 0;
}

// fops
static int fat32fs_write(file *file, const void *buf, size_t len){
  uart_printf("Exception, fat32fs_write(), unimplemented.\r\n");
  return -1;
}
static int fat32fs_read(file *file, void *buf, size_t len){
  len = len <= file->vnode->comp->len ? len : file->vnode->comp->len;
  uint32_t lba_count = len / SD_BLOCK_SIZE + (len%SD_BLOCK_SIZE != 0);
  char *temp = diy_malloc(SD_BLOCK_SIZE * lba_count);
  for(int i=0; i<lba_count; i++)
    readblock(file->vnode->comp->lba + i, temp + i*SD_BLOCK_SIZE);
  memcpy_(buf, temp, len);

  uart_printf("Debug, fat32fs_read(), reading file %s, start lba=%d\r\n", file->vnode->comp->comp_name, file->vnode->comp->lba);
  diy_free(temp);
  return len;
}
static int fat32fs_open(vnode* file_node, file** target){
  return tmpfs_open(file_node, target);
}
static int fat32fs_close(file *file){
  return tmpfs_close(file);
}

// vops
static int fat32fs_mkdir(vnode *dir_node, vnode **target, const char *component_name){
  if(fat32fs_mounted){
    uart_printf("Error, fat32fs_mkdir(), already mounted, cannot modify initramfs\r\n");
    return 1;
  }
  else
    return tmpfs_mkdir(dir_node, target, component_name);
}
static int fat32fs_create(vnode *dir_node, vnode **target, const char *component_name){
  if(fat32fs_mounted){
    uart_printf("Error, fat32fs_create(), already mounted, cannot modify initramfs\r\n");
    return 1;
  }
  else
    return tmpfs_create(dir_node, target, component_name);
}
static int fat32fs_lookup(vnode *dir_node, vnode **target, const char *component_name){
  // uart_printf("Exception, fat32fs_lookup(), unimplemented, name=%s.\r\n", component_name);
  return tmpfs_lookup(dir_node, target, component_name);
}
