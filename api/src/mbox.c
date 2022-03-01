
#include "mbox.h"
#include "general.h"
#include "uart.h"

/* mailbox message buffer */
volatile uint32_t  __attribute__((aligned(16))) mbox_buf[36];
#define VIDEOCORE_MBOX  (MMIO_BASE+0x0000B880)
#define MBOX_READ       ((volatile uint32_t*)(VIDEOCORE_MBOX+0x00))
#define MBOX_POLL       ((volatile uint32_t*)(VIDEOCORE_MBOX+0x10))
#define MBOX_SENDER     ((volatile uint32_t*)(VIDEOCORE_MBOX+0x14))
#define MBOX_STATUS     ((volatile uint32_t*)(VIDEOCORE_MBOX+0x18))
#define MBOX_CONFIG     ((volatile uint32_t*)(VIDEOCORE_MBOX+0x1C))
#define MBOX_WRITE      ((volatile uint32_t*)(VIDEOCORE_MBOX+0x20))
#define MBOX_RESPONSE   0x80000000
#define MBOX_FULL       0x80000000
#define MBOX_EMPTY      0x40000000

#define MBOX_GET_BOARD_REVISION  0x00010002 // request_length=0, response_length=4, response_value={u32 board revision}
#define MBOX_GET_ARM_MEMORY      0x00010005 // request_length=0, response_length=8, response_value={u32 base address in bytes, u32 size in bytes}
#define MBOX_REQUEST_CODE        0x00000000
#define MBOX_REQUEST_SUCCEED     0x80000000
#define MBOX_REQUEST_FAILED      0x80000001
#define MBOX_TAG_REQUEST_CODE    0x00000000
#define MBOX_END_TAG             0x00000000

/**
 * Make a mailbox call. Returns 0 on failure, non-zero on success
 */
int mbox_call(unsigned char ch)
{
  uint32_t temp = (
    (    ((uint32_t) ((uint64_t)mbox_buf)) & ~0xF    ) // & ~0xF clears lower 4 bits
     | 
    ( ch & 0xF ) // & 0x0000000F clears upper 28 bits
  ); // 28 bits(MSB) for value, 4 bits for the channel

  // Wait until mailbox is not full (busy)
  while(*MBOX_STATUS & MBOX_FULL);

  // Write buffer address to mobx_write
  *MBOX_WRITE = temp;

  // Wait while it's empty
  while(*MBOX_STATUS & MBOX_EMPTY);

  // Check if the value is the same as the one wrote into MBOX_WRITE
  if(*MBOX_READ == temp)
    /* is it a valid successful response? */
    return mbox_buf[1] == MBOX_RESPONSE;
  else
    return 0;
}

int mbox_board_rev(uint32_t *board_reviion){
  int ret = 0;

  mbox_buf[0] = 7 * 4;                   // buffer size in bytes, 7 for 7 elements to MBOX_END_TAG; 4 for each elements is 4 bytes (u32)
  mbox_buf[1] = MBOX_REQUEST_CODE;       // fixed code
  // tags begin
  mbox_buf[2] = MBOX_GET_BOARD_REVISION; // tag identifier
  mbox_buf[3] = 4;                       // response length
  mbox_buf[4] = MBOX_TAG_REQUEST_CODE;   // fixed code
  mbox_buf[5] = 0;                       // output buffer, clear it here
  // tags end
  mbox_buf[6] = MBOX_END_TAG;

  // Send mailbox request
  ret = mbox_call(MBOX_CH_PROP);
  *board_reviion = mbox_buf[5];
  return ret;
}

int mbox_arm_mem_info(uint32_t **base_addr, uint32_t *size){
  int ret = 0;

  mbox_buf[0] = 8 * 4;                  // buffer size in bytes, 8 for 8 elements to MBOX_END_TAG; 4 for each elements is 4 bytes (u32)
  mbox_buf[1] = MBOX_REQUEST_CODE;      // fixed code
  // tags begin
  mbox_buf[2] = MBOX_GET_ARM_MEMORY;    // tag identifier
  mbox_buf[3] = 8;                      // response length
  mbox_buf[4] = MBOX_TAG_REQUEST_CODE;  // fixed code
  mbox_buf[5] = 0;                      // output buffer 0, clear it here
  mbox_buf[6] = 0;                      // output buffer 1, clear it here
  // tags end
  mbox_buf[7] = MBOX_END_TAG;

  // Send mailbox request
  ret = mbox_call(MBOX_CH_PROP); // message passing procedure call
  *base_addr = (uint32_t*)((uint64_t)mbox_buf[5]); // cast to u64 cuz uint32_t* takes 64 bits
  *size = mbox_buf[6];
  return ret;
}
