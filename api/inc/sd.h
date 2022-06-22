#ifndef __SD_H_
#define __SD_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SD_BLOCK_SIZE 512

void readblock(int block_idx, void* buf);
void writeblock(int block_idx, void* buf);
void sd_init();

#ifdef __cplusplus
}
#endif
#endif  // __SD_H_
