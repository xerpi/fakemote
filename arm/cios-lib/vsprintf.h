#ifndef _VSPRINTF_H_
#define _VSPRINTF_H_

int svc_printf(const char *restrict fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
