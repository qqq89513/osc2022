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
  Ref[6]: https://www.youtube.com/watch?v=lz83GavddB0
*/

static uint32_t FAT1_phy_bk;     // physical block, FAT has size of sec_per_fat32*SD_BLOCK_SIZE bytes
static uint32_t root_dir_phy_bk; // physical block
static uint32_t root_cluster;    // boot_sector_t.root_cluster, usually 2
static uint32_t sec_per_fat32;   // boot_sector_t.sector_per_fat32

#define FROM_LBA_TO_PHY_BK(lba) (root_dir_phy_bk +(lba) +  - root_cluster)
#define FAT_ENTRY_EOF           ((uint32_t)0x0FFFFFF8) // end of file (end of linked list in FAT)
#define FAT_ENTRY_EMPTY         ((uint32_t)0x00000000)
#define DIR_ENTRY_ATTR_ARCHIVE  0x20    // file, page 23, Ref[5]
#define DIR_ENTRY_wrtTime_mock  0x58D8  // 11:06:48AM
#define DIR_ENTRY_wrtDate_mock  0x50C4  // 20200604

typedef struct partition_t {
  uint8_t status;
  uint8_t chss_head;
  uint8_t chss_sector;
  uint8_t chss_cylinder;
  uint8_t type;
  uint8_t chse_head;
  uint8_t chse_sector;
  uint8_t chse_cylinder;
  uint32_t phy_bk;
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

static int fat32_filename_to_str(const char *fat32_filename, char *str){
  // Copy entry name
  int j = 0;  // str[j]
  int k = 0;  // fat32_filename[k];
  for(k=0; k<8 && fat32_filename[k] != ' '; k++)    str[j++] = fat32_filename[k];
  str[j++] = '.';
  for(k=8; k<11 && fat32_filename[k] != ' '; k++)   str[j++] = fat32_filename[k];
  str[j] = '\0';
  return j;
}

filesystem fat32fs = {
  .name = "fat32fs",
  .setup_mount = fat32fs_mount
};
file_operations fat32fs_fops = {.write=fat32fs_write, .read=fat32fs_read, .open=fat32fs_open, .close=fat32fs_close};
vnode_operations fat32fs_vops = {.lookup=fat32fs_lookup, .create=fat32fs_create, .mkdir=fat32fs_mkdir};

static int fat32fs_mount(filesystem *fs, mount *mount){
  const MBR_t *mbr;
  uint32_t part1_phy_bk;
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

  part1_phy_bk = mbr->part1.phy_bk;

  readblock(part1_phy_bk, buf);

  boot_sector_t *VBR = (boot_sector_t*) buf;
  FAT1_phy_bk = part1_phy_bk + VBR->reserved_sector_cnt;
  root_dir_phy_bk = FAT1_phy_bk + (VBR->fat_cnt * VBR->sector_per_fat32);
  root_cluster = VBR->root_cluster;
  sec_per_fat32 = VBR->sector_per_fat32;
  readblock(root_dir_phy_bk, buf);
  dir_entry *root_dir_entry = (dir_entry*) buf;

  uart_printf("Debug, fat32fs_mount(), FAT1_phy_bk=%d, root_dir_phy_bk=%d, root_cluster=%d, sec_per_fat32=%d\r\n",
    FAT1_phy_bk, root_dir_phy_bk, root_cluster, sec_per_fat32);

  mount->root = diy_malloc(sizeof(vnode));
  mount->fs = fs;
  mount->root->mount = NULL;
  mount->root->comp = diy_malloc(sizeof(vnode_comp));
  mount->root->comp->comp_name = "";
  mount->root->comp->len = 0;
  mount->root->comp->lba = root_dir_phy_bk;
  mount->root->comp->type = COMP_DIR;
  mount->root->f_ops = &fat32fs_fops;
  mount->root->v_ops = &fat32fs_vops;

  // Insert files in the root_dir_entry to the file system
  vnode *dir_node = mount->root;
  vnode *node_new = NULL;
  const dir_entry *item = (dir_entry*) root_dir_entry;
  for(int i=0; i<SD_BLOCK_SIZE/sizeof(dir_entry); i++){
    
    // Skip empty entry
    if(item[i].name[0] != '\0'){
      char *name = diy_malloc(sizeof(item->name) + 2); // +1 for . in fileName.ext and end of string
      
      // Copy entry name
      fat32_filename_to_str(item[i].name, name);

      // Create entry along tmpfs
      lookup_recur((char*)name, dir_node, &node_new, 1);
      
      // Config entry to a file
      node_new->comp->type = COMP_FILE;
      node_new->comp->lba =  item[i].fstClusHI << 16 | item[i].fstClusLO; 
      node_new->comp->len = item[i].fileSize;
    }
  }

  fat32fs_mounted = 1;
  return 0;
}

// fops
static int fat32fs_write(file *file, const void *buf, size_t len){
  len = len <= SD_BLOCK_SIZE ? len : SD_BLOCK_SIZE;
  uint32_t block_cnt = len / SD_BLOCK_SIZE + (len%SD_BLOCK_SIZE != 0);
  char *temp = diy_malloc(SD_BLOCK_SIZE * block_cnt);
  char name[13];  // 11 char + '.' + '\0'
  uint32_t phy_block = FROM_LBA_TO_PHY_BK(file->vnode->comp->lba);

  // Read modify write
  readblock(phy_block, temp);
  memcpy_(temp, buf, len);
  writeblock(phy_block, temp);

  // Update root dir entry
  readblock(root_dir_phy_bk, temp);
  dir_entry *root_dir_entry = (dir_entry*) temp;
  dir_entry *item = (dir_entry*) root_dir_entry;
  const uint32_t entires_per_dir = SD_BLOCK_SIZE/sizeof(dir_entry);
  int i = 0;
  for(i=0; i<entires_per_dir; i++){
    if(item[i].name[0] != '\0'){  // skip empty entry
      fat32_filename_to_str(item[i].name, name);
      if(strcmp_(file->vnode->comp->comp_name, name) == 0)
        break;
    }
  }
  if(i >= entires_per_dir) return -1;
  item = &item[i];
  item->fileSize = len;
  writeblock(root_dir_phy_bk, temp);

  file->vnode->comp->len = len;
  diy_free(temp);
  return len;
}
static int fat32fs_read(file *file, void *buf, size_t len){
  len = len <= file->vnode->comp->len ? len : file->vnode->comp->len;
  uint32_t block_cnt = len / SD_BLOCK_SIZE + (len%SD_BLOCK_SIZE != 0);
  char *temp = diy_malloc(SD_BLOCK_SIZE * block_cnt);
  uint32_t phy_block = FROM_LBA_TO_PHY_BK(file->vnode->comp->lba);
  for(int i=0; i<block_cnt; i++)
    readblock(phy_block + i, temp + i*SD_BLOCK_SIZE);
  memcpy_(buf, temp, len);

  uart_printf("Debug, fat32fs_read(), reading file %s, starting phy block=%d\r\n", file->vnode->comp->comp_name, phy_block);
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
    // Find free entry in FAT1 and mark it as used (eof)
    char temp[SD_BLOCK_SIZE];
    uint32_t lba = 0;
    const uint32_t entries_per_sec = SD_BLOCK_SIZE/sizeof(uint32_t);
    for(int b=0; b<sec_per_fat32; b++){  // walk through sectors(blocks) in FAT1
      readblock(b + FAT1_phy_bk, temp);
      uint32_t *fat_entry = (uint32_t*) temp;
      for(int i=0; i<entries_per_sec; i++){  // walk through each entry
        if(fat_entry[i] == FAT_ENTRY_EMPTY){
          fat_entry[i] = FAT_ENTRY_EOF; // immediate mark this as entry EOF, i.e., this file occupies 1 sector
          writeblock(b + FAT1_phy_bk, temp); // write back
          lba = b * entries_per_sec + i;
          break;
        }
      }
      if(lba != 0)
        break;
    }

    // Fill free directory entry
    if(lba != 0){
      // uart_printf("Debug, fat32fs_create(), ------------- lba = %d\r\n", lba);

      // Find free entry in root directory
      readblock(root_dir_phy_bk, temp);
      dir_entry *root_dir_entry = (dir_entry*) temp;
      dir_entry *item = (dir_entry*) root_dir_entry;
      const uint32_t entires_per_dir = SD_BLOCK_SIZE/sizeof(dir_entry);
      int i = 0;
      for(i=0; i<entires_per_dir; i++){
        if(item[i].name[0] == '\0')
          break;
      }

      // Free entry found
      if(i < entires_per_dir){
        item = &item[i];
        int j = 0; // component_name[j]
        int k = 0; // item->name[k]
        // Copy filename
        while(k < 8 && component_name[j] != '\0' && component_name[j] != '.') item->name[k++] = component_name[j++];
        j++; // skip '.'
        // Fill space
        while(k < 8)  item->name[k++] = ' ';
        // Copy extention
        while(k < 11 && component_name[j] != '\0')  item->name[k++] = component_name[j++];

        // Config entry metadata and write back to SD card
        item->attr = DIR_ENTRY_ATTR_ARCHIVE;
        item->wrtDate = DIR_ENTRY_wrtDate_mock;
        item->wrtTime = DIR_ENTRY_wrtTime_mock;
        item->fstClusHI = (lba >> 16) & 0x0000FFFF;
        item->fstClusLO = lba & 0x0000FFFF;
        item->fileSize = 0;
        writeblock(root_dir_phy_bk, temp);  // since item is sort of temp's reference

        int ret = tmpfs_create(dir_node, target, component_name);
        if(ret == 0) (*target)->comp->lba = lba;
        return ret;
      }
      else{
        uart_printf("Error, fat32fs_create(), cannot find free entry in root_dir_entry\r\n");
        return 1;
      }
    }
    else {
      uart_printf("Error, fat32fs_create(), cannot find free entry in FAT1\r\n");
      return 1;
    }
  }
  else
    return tmpfs_create(dir_node, target, component_name);
}
static int fat32fs_lookup(vnode *dir_node, vnode **target, const char *component_name){
  // uart_printf("Exception, fat32fs_lookup(), unimplemented, name=%s.\r\n", component_name);
  return tmpfs_lookup(dir_node, target, component_name);
}
