#ifndef INJMESSAGE_H
#define INJMESSAGE_H

#include "types.h"

/* Custom type for messages that we inject to the ReadyQ.
 * They must *always* be allocated from the injmessages_heap. */
typedef struct {
	u16 size;
	u8 data[];
} ATTRIBUTE_PACKED injmessage;

int injmessage_init_heap(void);
void injmessage_free(void *msg);
bool is_message_injected(const void *msg);

/* HCI event injection helpers */
int inject_hci_event_command_status(u16 opcode);
int inject_hci_event_command_compl(u16 opcode, const void *payload, u32 payload_size);
int inject_hci_event_con_req(const bdaddr_t *bdaddr, u8 uclass0, u8 uclass1, u8 uclass2, u8 link_type);
int inject_hci_event_discon_compl(u16 con_handle, u8 status, u8 reason);
int inject_hci_event_con_compl(const bdaddr_t *bdaddr, u16 con_handle, u8 status);
int inject_hci_event_role_change(const bdaddr_t *bdaddr, u8 role);
int inject_hci_event_mode_change(u16 con_handle, u8 unit_mode, u16 interval);
int inject_hci_event_num_compl_pkts(u8 num_con_handles, const u16 *con_handles, const u16 *compl_pkts);
int inject_hci_event_return_link_keys(u8 num_keys, const bdaddr_t *bdaddr, const u8 key[][HCI_KEY_SIZE]);
int inject_hci_event_con_pkt_type_changed(u16 con_handle, u16 pkt_type);
int inject_hci_event_auth_compl(u8 status, u16 con_handle);
int inject_hci_event_remote_name_req_compl(u8 status, const bdaddr_t *bdaddr, const char *name);
int inject_hci_event_read_remote_features_compl(u16 con_handle, const u8 features[static HCI_FEATURES_SIZE]);
int inject_hci_event_read_remote_ver_info_compl(u16 con_handle, u8 lmp_version, u16 manufacturer,
						 u16 lmp_subversion);
int inject_hci_event_read_clock_offset_compl(u16 con_handle, u16 clock_offset);

/* L2CAP injection helpers */
int inject_l2cap_packet(u16 hci_con_handle, u16 dcid, const void *data, u16 size);
int inject_l2cap_connect_req(u16 hci_con_handle, u16 psm, u16 scid);
int inject_l2cap_disconnect_req(u16 hci_con_handle, u16 dcid, u16 scid);
int inject_l2cap_disconnect_rsp(u16 hci_con_handle, u8 ident, u16 dcid, u16 scid);
int inject_l2cap_config_req(u16 hci_con_handle, u16 remote_cid, u16 mtu, u16 flush_time_out);
int inject_l2cap_config_rsp(u16 hci_con_handle, u16 remote_cid, u8 ident, const u8 *options, u32 options_len);

#endif
