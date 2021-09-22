#ifndef FAKEDEV_H
#define FAKEDEV_H

#include "hci.h"

void fakedev_init(void);
void fakedev_tick_devices(void);
/* Returns true if the bdaddr belongs to a fake device */
bool fakedev_handle_hci_cmd_accept_con(const bdaddr_t *bdaddr, u8 role);

#endif
