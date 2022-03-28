#ifndef __DIY_CTYPE_H_
#define __DIY_CTYPE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

// ref: https://www.cplusplus.com/reference/cctype/
#define isascii(__c)	((__c)<=127)
#define isdigit(__c)  ('0'<=(__c) && (__c)<='9')
#define isspace(__c)  ((__c)=='\t' || (__c)=='\n' || (__c)=='\v' || (__c)=='\f' || (__c)=='\r' || (__c)==' ')
#define isupper(__c)  ('A'<=(__c) && (__c)<='Z')
#define islower(__c)  ('a'<=(__c) && (__c)<='z')
#define isalpha(__c)  (isupper(__c) || islower(__c))

#define	__DECONST(type, var)	((type)(uintptr_t)(const void *)(var))




#ifdef __cplusplus
}
#endif
#endif  // __DIY_CTYPE_H_
