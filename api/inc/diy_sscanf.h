
#ifndef __DIY_SSCANF_H_
#define __DIY_SSCANF_H_

#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif



int sscanf_(const char *ibuf, const char *fmt, ...);
int vsscanf_(const char *inp, char const *fmt0, __builtin_va_list ap);

#ifdef __cplusplus
}
#endif

#endif  // __DIY_SSCANF_H_
