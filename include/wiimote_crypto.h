#ifndef WIIMOTE_CRYPTO_H
#define WIIMOTE_CRYPTO_H

#include "types.h"

struct wiimote_encryption_key_t {
    u8 ft[8];
    u8 sb[8];
};

void wiimote_crypto_generate_key_from_extension_key_data(struct wiimote_encryption_key_t *ext_key,
                                                         const u8 key_data[static 16]);
void wiimote_crypto_encrypt(u8 *data, const struct wiimote_encryption_key_t *key, u32 addr,
                            u32 size);

#endif
