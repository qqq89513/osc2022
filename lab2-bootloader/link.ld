
SECTIONS
{
  . = 0x60000;               /* set . counter to Initial address, 0x8000(O), 0x7FFF(O), 0x0000(X) */
  __start__ = .;
  . = ALIGN(8);
  .text : { *(.text*) }      /* text section */
  .data : {                  /* Data section */
    . = ALIGN(8);            /* prevent overlapping */
    *(.data)
    *(.data*)
  }
  .bss  : {                  /* Place bss right behind data in the memmory */
    . = ALIGN(8);            /* prevent overlapping */
    _bss_start = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    _bss_end = .;
  }
  . = ALIGN(8);
  _end = .;
}
_bss_size = (_bss_end - _bss_start)>>3;  /* >>3 is devided by 8, (_bss_end - _bss_start) is size in bytes, _bss_size is size in double_word (64 bits) */
_whole_size = (_end - __start__) >> 3;