
SECTIONS
{
  . = 0x80000;               /* set . counter to Initial address */
  . = ALIGN(16);
  .text : { *(.text*) }      /* text section */
  .data : {                  /* Data section */
    . = ALIGN(16);            /* prevent overlapping */
    *(.data)
    *(.data*)
  }
  .bss  : {                  /* Place bss right behind data in the memmory */
    . = ALIGN(16);            /* prevent overlapping */
    _bss_start = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    _bss_end = .;
  }
  end = .;
}
_bss_size = (_bss_end - _bss_start)>>3;  /* >>3 is devided by 8, (_bss_end - _bss_start) is size in bytes, _bss_size is size in double_word (64 bits) */
