#ifndef CONF_H
#define CONF_H

#include "types.h"

/* From libogc/gc/ogc/conf.h */

#define CONF_EBADFILE	-0x6001
#define CONF_ENOENT		-0x6002
#define CONF_ETOOBIG	-0x6003
#define CONF_ENOTINIT	-0x6004
#define CONF_ENOTIMPL	-0x6005
#define CONF_EBADVALUE	-0x6006
#define CONF_ENOMEM		-0x6007
#define	CONF_ERR_OK		0

enum {
	CONF_BIGARRAY = 1,
	CONF_SMALLARRAY,
	CONF_BYTE,
	CONF_SHORT,
	CONF_LONG,
	CONF_BOOL = 7
};

#define CONF_PAD_MAX_REGISTERED 10
#define CONF_PAD_MAX_ACTIVE 4

struct conf_pad_device {
	u8 bdaddr[6];
	char name[0x40];
} ATTRIBUTE_PACKED;

struct conf_pads_setting {
	u8 num_registered;
	struct conf_pad_device registered[CONF_PAD_MAX_REGISTERED];
	struct conf_pad_device active[CONF_PAD_MAX_ACTIVE];
	struct conf_pad_device balance_board;
	struct conf_pad_device unknown;
} ATTRIBUTE_PACKED;

int conf_get(const u8 *conf, const char *name, void *buffer, u32 length);


#endif
