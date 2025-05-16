#ifndef UTILS_H
#define UTILS_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "egc_types.h"

#define le16toh(x) __builtin_bswap16(x)
#define htole16(x) __builtin_bswap16(x)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define BIT(nr)        (1ull << (nr))
#define MIN2(x, y)     (((x) < (y)) ? (x) : (y))
#define MAX2(x, y)     (((x) > (y)) ? (x) : (y))
#define ROUNDUP32(x)   (((u32)(x) + 0x1f) & ~0x1f)
#define ROUNDDOWN32(x) (((u32)(x) - 0x1f) & ~0x1f)

#define UNUSED(x)                 (void)(x)
#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)

#ifdef assert
#undef assert
#endif
#define assert(exp) ((exp) ? (void)0 : my_assert_func(__FILE__, __LINE__, __FUNCTION__, #exp))

#define LOG_INFO(...) printf(__VA_ARGS__)
#if DEBUG
#define LOG_DEBUG(...) printf(__VA_ARGS__)
#else
#define LOG_DEBUG(...) (void)0
#endif

extern void my_assert_func(const char *file, int line, const char *func, const char *failedexpr);

static inline int memmismatch(const void *restrict a, const void *restrict b, int size)
{
    int i = 0;
    while (size) {
        if (((u8 *)a)[i] != ((u8 *)b)[i])
            return i;
        i++;
        size--;
    }
    return i;
}

static inline void reverse_memcpy(void *restrict dst, const void *restrict src, int size)
{
    u8 *d = dst;
    const u8 *s = src;
    for (int i = 0; i < size; i++)
        d[i] = s[size - 1 - i];
}

#endif
