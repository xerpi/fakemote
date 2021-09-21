#include <string.h>
#include "conf.h"

static u8 *conf_find(u8 *conf, const char *name)
{
	u16 count;
	u16 *offset;
	int nlen = strlen(name);
	count = *((u16*)(&conf[4]));
	offset = (u16*)&conf[6];

	while(count--) {
		if((nlen == ((conf[*offset]&0x0F)+1)) && !memcmp(name, &conf[*offset+1], nlen))
			return &conf[*offset];
		offset++;
	}
	return NULL;
}

static s32 conf_get_length(u8 *conf, const char *name)
{
	u8 *entry;

	entry = conf_find(conf, name);
	if(!entry)
		return CONF_ENOENT;

	switch (*entry>>5) {
	case 1:
		return *((u16*)&entry[strlen(name)+1]) + 1;
	case 2:
		return entry[strlen(name)+1] + 1;
	case 3:
		return 1;
	case 4:
		return 2;
	case 5:
		return 4;
	case 7:
		return 1;
	default:
		return CONF_ENOTIMPL;
	}
}

int conf_get(u8 *conf, const char *name, void *buffer, u32 length)
{
	u8 *entry;
	s32 len;

	entry = conf_find(conf, name);
	if(!entry)
		return CONF_ENOENT;

	len = conf_get_length(conf, name);
	if (len < 0)
		return len;
	else if (len > length)
		return CONF_ETOOBIG;

	switch (*entry >> 5) {
	case CONF_BIGARRAY:
		memcpy(buffer, &entry[strlen(name)+3], len);
		break;
	case CONF_SMALLARRAY:
		memcpy(buffer, &entry[strlen(name)+2], len);
		break;
	case CONF_BYTE:
	case CONF_SHORT:
	case CONF_LONG:
	case CONF_BOOL:
		memset(buffer, 0, length);
		memcpy(buffer, &entry[strlen(name)+1], len);
		break;
	default:
		return CONF_ENOTIMPL;
	}
	return len;
}

int conf_set(u8 *conf, const char *name,  const void *buffer, u32 length)
{
	u8 *entry;
	s32 len;

	entry = conf_find(conf, name);
	if(!entry)
		return CONF_ENOENT;

	len = conf_get_length(conf, name);
	if (len < 0)
		return len;
	else if (len > length)
		return CONF_ETOOBIG;

	switch (*entry >> 5) {
	case CONF_BIGARRAY:
		memcpy(&entry[strlen(name)+3], buffer, len);
		break;
	case CONF_SMALLARRAY:
		memcpy(&entry[strlen(name)+2], buffer, len);
		break;
	case CONF_BYTE:
	case CONF_SHORT:
	case CONF_LONG:
	case CONF_BOOL:
		memcpy(&entry[strlen(name)+1], buffer, len);
		break;
	default:
		return CONF_ENOTIMPL;
	}
	return len;
}
