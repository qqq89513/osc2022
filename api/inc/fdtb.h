
#ifndef __FDTB_H_
#define __FDTB_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include "cpio.h"

typedef struct __fdt_header__ {
  uint32_t magic;
  uint32_t totalsize;
  uint32_t off_dt_struct;
  uint32_t off_dt_strings;
  uint32_t off_mem_rsvmap;
  uint32_t version;
  uint32_t last_comp_version;
  uint32_t boot_cpuid_phys;
  uint32_t size_dt_strings;
  uint32_t size_dt_struct;
} fdt_header;

typedef struct __fdt_node_prop__{
  uint32_t len;
  uint32_t nameoff;
} fdt_node_prop;


int fdtb_parse(void *dtb_addr, int print, cpio_parse_func *callback);

#ifdef __cplusplus
}
#endif
#endif  // __FDTB_H_
