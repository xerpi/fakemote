#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdio.h>
#include <string.h>

#include "types.h"

/* Size in bytes */
#define BDADDR_SIZE 6

#define BDADDR_STR_LEN 18

/* Bluetooth device (BD) Address */
typedef struct {
    u8 b[6];
} __attribute__((packed)) bdaddr_t;

#define BDADDR_ANY                                                                                 \
    (&(bdaddr_t){                                                                                  \
        { 0, 0, 0, 0, 0, 0 } \
    })
#define BDADDR_LOCAL                                                                               \
    (&(bdaddr_t){                                                                                  \
        { 0, 0, 0, 0xff, 0xff, 0xff } \
    })

/* From linux/include/net/bluetooth/bluetooth.h and linux/net/bluetooth/lib.c */

static inline int bacmp(const bdaddr_t *ba1, const bdaddr_t *ba2)
{
    return memcmp(ba1, ba2, sizeof(bdaddr_t));
}

static inline void bacpy(bdaddr_t *dst, const bdaddr_t *src)
{
    memcpy(dst, src, sizeof(bdaddr_t));
}

static inline void baswap(bdaddr_t *dst, bdaddr_t *src)
{
    unsigned char *d = (unsigned char *)dst;
    unsigned char *s = (unsigned char *)src;
    unsigned int i;

    for (i = 0; i < 6; i++)
        d[i] = s[5 - i];
}

static inline void bdaddr_to_str(char str[static BDADDR_STR_LEN], const bdaddr_t *bdaddr)
{
    snprintf(str, BDADDR_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x", bdaddr->b[5], bdaddr->b[4],
             bdaddr->b[3], bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);
}

#endif
