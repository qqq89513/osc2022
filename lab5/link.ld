
SECTIONS
{
  . = 0x80000;               /* set . counter to Initial address */
  __stack_start = .;
  __stack_end = . - 0x2000;  /* 8kB stack, stack grows downward */
  __image_start = .;
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
  __image_end = .;
  __simple_malloc_start = .;
  /* __simple_malloc_end is determined at run time*/

  __simple_malloc_end_no_paging = . + 0x18000; /* for non paging usage, 96kB */
}
_bss_size = (_bss_end - _bss_start)>>3;  /* >>3 is devided by 8, (_bss_end - _bss_start) is size in bytes, _bss_size is size in double_word (64 bits) */
