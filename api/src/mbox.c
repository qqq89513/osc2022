
#include "mbox.h"
#include "general.h"
#include "uart.h"
#include "mmu.h"
#include "stddef.h"

/* mailbox message buffer */
volatile int  __attribute__((aligned(16))) mbox_buf[36];
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
  return mbox_call_user_buffer(ch, (unsigned int*)mbox_buf);
}
/**
 * @param ch: 0~8, 8 for property
 * @param mbox: a pointer to the buffer from user space. 
 *              caller should fill it as documented. 
 *              If the there is an output from mail box, the output is also placed somewhere in the buffer
 * @return 0 on failure, non-zero on success
*/
int mbox_call_user_buffer(unsigned char ch, unsigned int *mbox){
#ifdef VIRTUAL_MEM
  void *addr = 0;
  if(((uint64_t)mbox & 0xFFFF000000000000) == 0) // translation for lower VA
    addr = virtual_mem_translate(mbox);
  else
    addr = mbox;

  uint32_t temp = (
    (    ((uint32_t) ((uint64_t)addr)) & ~0xF    ) // & ~0xF clears lower 4 bits
     | 
    ( ch & 0xF ) // & 0x0000000F clears upper 28 bits
  ); // 28 bits(MSB) for value, 4 bits for the channel
#else
  uint32_t temp = (
    (    ((uint32_t) ((uint64_t)mbox)) & ~0xF    ) // & ~0xF clears lower 4 bits
     | 
    ( ch & 0xF ) // & 0x0000000F clears upper 28 bits
  ); // 28 bits(MSB) for value, 4 bits for the channel
#endif

  // Wait until mailbox is not full (busy)
  while(*MBOX_STATUS & MBOX_FULL);

  // Write buffer address to mobx_write
  *MBOX_WRITE = temp;

  // Wait while it's empty
  while(*MBOX_STATUS & MBOX_EMPTY);

  // Check if the value is the same as the one wrote into MBOX_WRITE
  if(*MBOX_READ == temp)
    /* is it a valid successful response? */
    return mbox[1] == MBOX_RESPONSE;
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

// lab7, ref: https://oscapstone.github.io/labs/lab7.html
char *framebuffer_init(){
  unsigned int __attribute__((unused)) width, height, pitch, isrgb; /* dimensions and channel order */
  char *lfb;                                /* raw frame buffer address */

  mbox_buf[0] = 35 * 4;
  mbox_buf[1] = MBOX_REQUEST;

  mbox_buf[2] = 0x48003; // set phy wh
  mbox_buf[3] = 8;
  mbox_buf[4] = 8;
  mbox_buf[5] = 1024; // FrameBufferInfo.width
  mbox_buf[6] = 768;  // FrameBufferInfo.height

  mbox_buf[7] = 0x48004; // set virt wh
  mbox_buf[8] = 8;
  mbox_buf[9] = 8;
  mbox_buf[10] = 1024; // FrameBufferInfo.virtual_width
  mbox_buf[11] = 768;  // FrameBufferInfo.virtual_height

  mbox_buf[12] = 0x48009; // set virt offset
  mbox_buf[13] = 8;
  mbox_buf[14] = 8;
  mbox_buf[15] = 0; // FrameBufferInfo.x_offset
  mbox_buf[16] = 0; // FrameBufferInfo.y.offset

  mbox_buf[17] = 0x48005; // set depth
  mbox_buf[18] = 4;
  mbox_buf[19] = 4;
  mbox_buf[20] = 32; // FrameBufferInfo.depth

  mbox_buf[21] = 0x48006; // set pixel order
  mbox_buf[22] = 4;
  mbox_buf[23] = 4;
  mbox_buf[24] = 1; // RGB, not BGR preferably

  mbox_buf[25] = 0x40001; // get framebuffer, gets alignment on request
  mbox_buf[26] = 8;
  mbox_buf[27] = 8;
  mbox_buf[28] = 4096; // FrameBufferInfo.pointer
  mbox_buf[29] = 0;    // FrameBufferInfo.size

  mbox_buf[30] = 0x40008; // get pitch
  mbox_buf[31] = 4;
  mbox_buf[32] = 4;
  mbox_buf[33] = 0; // FrameBufferInfo.pitch

  mbox_buf[34] = MBOX_TAG_LAST;

  // this might not return exactly what we asked for, could be
  // the closest supported resolution instead
  if(mbox_call(MBOX_CH_PROP) && mbox_buf[20] == 32 && mbox_buf[28] != 0){
    mbox_buf[28] &= 0x3FFFFFFF; // convert GPU address to ARM address
    width = mbox_buf[5];        // get actual physical width
    height = mbox_buf[6];       // get actual physical height
    pitch = mbox_buf[33];       // get number of bytes per line
    isrgb = mbox_buf[24];       // get the actual channel order
    lfb = (void *)((unsigned long)mbox_buf[28]);
    return lfb;
  }
  else{
    uart_printf("Error, framebuffer_init(), Unable to set screen resolution to 1024x768x32\r\n");
    return NULL;
  }
}
