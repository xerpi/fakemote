#ifndef FAKE_WIIMOTE_MGR_H
#define FAKE_WIIMOTE_MGR_H

#include "client.h"
#include "fake_wiimote.h"
#include "hci.h"

#define FAKE_WIIMOTE_BDADDR(i)                                                                     \
    ((bdaddr_t){                                                                                   \
        .b = { 0xFE, 0xED, 0xBA, 0xDF, 0x00, 0xD0 + i } \
    })

/** Used by the main event loop **/
void fake_wiimote_mgr_init(void);
void fake_wiimote_mgr_tick_devices(void);

/** Used by the HCI state tracker **/

/* Proceesses and returns true if the HCI command targeted a fake wiimote */
bool fake_wiimote_mgr_handle_hci_cmd_from_host(const hci_cmd_hdr_t *hdr);

/* Processes and returns true if the HCI connection handle belongs to a fake wiimote */
bool fake_wiimote_mgr_handle_acl_data_out_request_from_host(u16 hci_con_handle,
                                                            const hci_acldata_hdr_t *hdr);

#endif
