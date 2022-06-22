#include "fat32_on_SD.h"

#include "sd.h"
#include "virtual_file_system.h"
#include "uart.h"
#include <stdint.h>

/*
  Ref[1]: https://wiki.osdev.org/MBR_(x86)
  Ref[2]: https://wiki.osdev.org/FAT#Boot_Record
  Ref[3]: https://github.com/LJP-TW/osc2022
  Ref[4]: https://www.easeus.com/resource/fat32-disk-structure.htm
  Ref[5]: https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
*/

static int fat32fs_mount(struct filesystem *fs, struct mount *mount);

filesystem fat32fs = {
  .name = "fat32fs",
  .setup_mount = fat32fs_mount
};

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

static int fat32fs_mount(filesystem *fs, mount *mount){
  const MBR_t *mbr;
  struct fat_info_t *fat;
  struct fat_dir_t *dir;
  struct fat_internal *data;
  struct vnode *oldnode, *node;
  const char *name;
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
    uart_printf("Exception, fat32fs_mount(), unexpected partition type = 0x%02D\r\n", mbr->part1.type);
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
  readblock(root_dir_lba, buf);
  dir_entry *root_dir_entry = (dir_entry*) buf;
  uart_printf("lba=%d, FAT_lba=%d, root_dir_lba=%d, filename=%s\r\n", lba, FAT1_lba, root_dir_lba, root_dir_entry->name);
  return 0;
}


