.section ".text"
_start:  // Entry point, infinity loop
  wfe
  b _start
