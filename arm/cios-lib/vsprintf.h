#ifndef _VSPRINTF_H_
#define _VSPRINTF_H_

#include "types.h"

int svc_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif
