#ifndef FAKE_WIIMOTE_MGR_H
#define FAKE_WIIMOTE_MGR_H

#include "hci.h"
#include "input_device.h"
#include "wiimote.h"

#define MAX_FAKE_WIIMOTES	2

#define FAKE_WIIMOTE_BDADDR(i) ((bdaddr_t){.b = {0xFE, 0xED, 0xBA, 0xDF, 0x00, 0xD0 + i}})

typedef struct fake_wiimote_t fake_wiimote_t;

enum wiimote_mgr_ext_u {
	WIIMOTE_MGR_EXT_NONE = 0,
	WIIMOTE_MGR_EXT_NUNCHUK,
	WIIMOTE_MGR_EXT_CLASSIC,
	WIIMOTE_MGR_EXT_CLASSIC_WIIU_PRO,
	WIIMOTE_MGR_EXT_GUITAR,
	WIIMOTE_MGR_EXT_MOTION_PLUS,
};

/** Used by the main event loop **/
void fake_wiimote_mgr_init(void);
void fake_wiimote_mgr_tick_devices(void);

/** Used by the HCI state tracker **/

/* Proceesses and returns true if the bdaddr belongs to a fake wiimote */
bool fake_wiimote_mgr_handle_hci_cmd_accept_con(const bdaddr_t *bdaddr, u8 role);

/* Processes and returns true if the HCI connection handle belongs to a fake wiimote */
bool fake_wiimote_mgr_handle_hci_cmd_disconnect(u16 hci_con_handle, u8 reason);

/* Proceesses and returns true if the bdaddr belongs to a fake wiimote */
bool fake_wiimote_mgr_handle_hci_cmd_reject_con(const bdaddr_t *bdaddr, u8 reason);

/* Returns true if the HCI connection handle belongs to a fake wiimote */
bool fake_wiimote_mgr_hci_handle_belongs_to_fake_wiimote(u16 hci_con_handle);

/* Processes and returns true if the HCI connection handle belongs to a fake wiimote */
bool fake_wiimote_mgr_handle_acl_data_out_request_from_host(u16 hci_con_handle, const hci_acldata_hdr_t *hdr);

/** Used by the input devices **/

bool fake_wiimote_mgr_add_input_device(void *usrdata, const input_device_ops_t *ops);
bool fake_wiimote_mgr_remove_input_device(fake_wiimote_t *wiimote);
void fake_wiimote_mgr_set_extension(fake_wiimote_t *wiimote, enum wiimote_mgr_ext_u ext);
void fake_wiimote_mgr_report_input(fake_wiimote_t *wiimote, u16 buttons);
void fake_wiimote_mgr_report_ir_dots(fake_wiimote_t *wiimote, u8 num_dots, struct ir_dot_t *dots);
void fake_wiimote_mgr_report_input_ext(fake_wiimote_t *wiimote, u16 buttons,
				       const void *ext_data, u8 ext_size);

#endif
