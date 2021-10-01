#ifndef WIIMOTE_CRYPTO_H
#define WIIMOTE_CRYPTO_H

#include "types.h"

struct wiimote_encryption_key_t {
	u8 ft[8];
	u8 sb[8];
};

void wiimote_crypto_generate_key(struct wiimote_encryption_key_t *key, u8 key_data[static 0x10]);
void wiimote_crypto_encrypt(void *data, const struct wiimote_encryption_key_t *key, u16 size);

#endif
