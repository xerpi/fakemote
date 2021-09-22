#ifndef FAKEDEV_H
#define FAKEDEV_H

#include "hci.h"

void fakedev_init(void);
void fakedev_tick_devices(void);

/* Proceesses and returns true if the bdaddr belongs to a fake device */
bool fakedev_handle_hci_cmd_accept_con(const bdaddr_t *bdaddr, u8 role);

/* Processes and returns true if the HCI connection handle belongs to a fake device */
bool fakedev_handle_hci_cmd_from_host(u16 hci_con_handle, const hci_cmd_hdr_t *hdr);

/* Processes and returns true if the HCI connection handle belongs to a fake device */
bool fakedev_handle_acl_data_out_request_from_host(u16 hci_con_handle, const hci_acldata_hdr_t *hdr);

/* Processes and returns true if the HCI connection handle belongs to a fake device */
bool fakedev_handle_acl_data_in_request_from_host(u16 hci_con_handle, const hci_acldata_hdr_t *hdr);

#endif
