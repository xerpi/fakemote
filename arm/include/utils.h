#ifndef UTILS_H
#define UTILS_H

#define bswap16 __builtin_bswap16
#define le16toh bswap16
#define htole16 bswap16

#define printf svc_printf

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#ifdef assert
#undef assert
#endif
#define assert(exp) ( (exp) ? (void)0 : my_assert_func(__FILE__, __LINE__, __FUNCTION__, #exp))

extern void my_assert_func(const char *file, int line, const char *func, const char *failedexpr);

#endif
