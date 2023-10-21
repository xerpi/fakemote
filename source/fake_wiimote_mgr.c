#include <string.h>
#include "fake_wiimote_mgr.h"
#include "hci.h"
#include "hci_state.h"
#include "injmessage.h"
#include "syscalls.h"
#include "utils.h"
#include "wiimote.h"

static fake_wiimote_t fake_wiimotes[MAX_FAKE_WIIMOTES];

void fake_wiimote_mgr_init(void)
{
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++)
		fake_wiimote_init(&fake_wiimotes[i], &FAKE_WIIMOTE_BDADDR(i));
}

static inline void fake_wiimote_mgr_send_event_number_of_completed_packets(void)
{
	u16 con_handles[MAX_FAKE_WIIMOTES];
	u16 compl_pkts[MAX_FAKE_WIIMOTES];
	u32 total = 0;
	u8 num_con_handles = 0;

	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (fake_wiimote_is_connected(&fake_wiimotes[i])) {
			con_handles[num_con_handles] = fake_wiimotes[i].hci_con_handle;
			compl_pkts[num_con_handles] = fake_wiimotes[i].num_completed_acl_data_packets;
			/* Accumulate completed packets count */
			total += fake_wiimotes[i].num_completed_acl_data_packets;
			/* Reset count */
			fake_wiimotes[i].num_completed_acl_data_packets = 0;
			num_con_handles++;
		}
	}

	/* No completed packets, no event */
	if (total > 0)
		inject_hci_event_num_compl_pkts(num_con_handles, con_handles, compl_pkts);
}

static inline void fake_wiimote_mgr_check_assign_input_devices(void)
{
	input_device_t *input_device;

	/* Find if there's a fake Wiimote without an input device assigned */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (fake_wiimotes[i].active)
			continue;

		input_device = input_device_get_unassigned();
		if (input_device) {
			input_device_assign_wiimote(input_device, &fake_wiimotes[i]);
			fake_wiimote_init_state(&fake_wiimotes[i], input_device);
			fake_wiimotes[i].active = true;
		}
	}
}

void fake_wiimote_mgr_tick_devices(void)
{
	if (hci_can_request_connection())
		fake_wiimote_mgr_check_assign_input_devices();

	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (fake_wiimotes[i].active)
			fake_wiimote_tick(&fake_wiimotes[i]);
	}

	/* This event has to be sent periodically */
	fake_wiimote_mgr_send_event_number_of_completed_packets();
}

static inline bool does_bdaddr_belong_to_fake_wiimote(const bdaddr_t *bdaddr, int *index)
{
	/* Check if the bdaddr belongs to a fake wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (bacmp(bdaddr, &fake_wiimotes[i].bdaddr) == 0) {
			if (index)
				*index = i;
			return true;
		}
	}

	return false;
}

static inline fake_wiimote_t *get_fake_wiimote_for_hci_con_handle(u16 hci_con_handle)
{
	/* Check if the HCI connection handle belongs to a fake wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (fake_wiimote_is_connected(&fake_wiimotes[i]) &&
		    (fake_wiimotes[i].hci_con_handle == hci_con_handle))
			return &fake_wiimotes[i];
	}

	return NULL;
}

static inline bool does_hci_con_handle_belong_to_fake_wiimote(u16 hci_con_handle)
{
	return get_fake_wiimote_for_hci_con_handle(hci_con_handle) != NULL;
}

static bool fake_wiimote_mgr_handle_hci_cmd_accept_con(const bdaddr_t *bdaddr, u8 role)
{
	int i;

	/* Check if the bdaddr belongs to a fake wiimote */
	if (does_bdaddr_belong_to_fake_wiimote(bdaddr, &i)) {
		fake_wiimote_handle_hci_cmd_accept_con(&fake_wiimotes[i], role);
		return true;
	}

	return false;
}

static bool fake_wiimote_mgr_handle_hci_cmd_reject_con(const bdaddr_t *bdaddr, u8 reason)
{
	int i;

	/* Check if the bdaddr belongs to a fake wiimote */
	if (does_bdaddr_belong_to_fake_wiimote(bdaddr, &i)) {
		/* TODO: Send inject_hci_event_command_status(HCI_CMD_REJECT_CON) ? */
		/* Connection rejected to our fake wiimote. Disconnect */
		LOG_DEBUG("Connection to fake Wiimote %d rejected!\n", i);
		fake_wiimote_disconnect(&fake_wiimotes[i]);
		return true;
	}

	return false;
}

static bool fake_wiimote_mgr_handle_hci_cmd_disconnect(u16 hci_con_handle, u8 reason)
{
	int ret;
	fake_wiimote_t *wiimote;

	LOG_DEBUG("Handle HCI_CMD_DISCONNECT: con handle: 0x%x\n", hci_con_handle);

	wiimote = get_fake_wiimote_for_hci_con_handle(hci_con_handle);
	if (!wiimote)
		return false;

	ret = inject_hci_event_command_status(HCI_CMD_DISCONNECT);
	assert(ret == IOS_OK);

	/* Host wants disconnection to our fake wiimote. Disconnect */
	LOG_DEBUG("Host requested disconnection of fake Wiimote\n");
	fake_wiimote_disconnect(wiimote);

	return true;
}

bool fake_wiimote_mgr_handle_hci_cmd_from_host(const hci_cmd_hdr_t *hdr)
{
	void *payload = (void *)((u8 *)hdr + sizeof(hci_cmd_hdr_t));
	u16 opcode = le16toh(hdr->opcode);
	u16 con_handle;
	int ret;
	bool handled = false;

	switch (opcode) {
	case HCI_CMD_DISCONNECT: {
		hci_discon_cp *cp = payload;
		handled = fake_wiimote_mgr_handle_hci_cmd_disconnect(le16toh(cp->con_handle),
								     cp->reason);
		break;
	}
	case HCI_CMD_ACCEPT_CON: {
		hci_accept_con_cp *cp = payload;
		handled = fake_wiimote_mgr_handle_hci_cmd_accept_con(&cp->bdaddr, cp->role);
		break;
	}
	case HCI_CMD_REJECT_CON: {
		hci_reject_con_cp *cp = payload;
		handled = fake_wiimote_mgr_handle_hci_cmd_reject_con(&cp->bdaddr, cp->reason);
		break;
	}
	case HCI_CMD_CHANGE_CON_PACKET_TYPE: {
		hci_change_con_pkt_type_cp *cp = payload;
		con_handle = le16toh(cp->con_handle);
		if (does_hci_con_handle_belong_to_fake_wiimote(con_handle)) {
			u16 pkt_type = le16toh(cp->pkt_type);
			ret = inject_hci_event_command_status(HCI_CMD_CHANGE_CON_PACKET_TYPE);
			assert(ret == IOS_OK);
			ret = inject_hci_event_con_pkt_type_changed(con_handle, pkt_type);
			assert(ret == IOS_OK);
			handled = true;
		}
		break;
	}
	case HCI_CMD_AUTH_REQ: {
		hci_auth_req_cp *cp = payload;
		con_handle = le16toh(cp->con_handle);
		if (does_hci_con_handle_belong_to_fake_wiimote(con_handle)) {
			ret = inject_hci_event_command_status(HCI_CMD_AUTH_REQ);
			assert(ret == IOS_OK);
			ret = inject_hci_event_auth_compl(0, con_handle);
			assert(ret == IOS_OK);
			handled = true;
		}
		break;
	}
	case HCI_CMD_REMOTE_NAME_REQ: {
		int index;
		hci_remote_name_req_cp *cp = payload;
		if (does_bdaddr_belong_to_fake_wiimote(&cp->bdaddr, &index)) {
			char name[32];
			snprintf(name, sizeof(name), "Fake Wiimote %d", index);
			ret = inject_hci_event_command_status(HCI_CMD_REMOTE_NAME_REQ);
			assert(ret == IOS_OK);
			ret = inject_hci_event_remote_name_req_compl(0, &cp->bdaddr, name);
			assert(ret == IOS_OK);
			handled = true;
		}
		break;
	}
	case HCI_CMD_READ_REMOTE_FEATURES: {
		hci_read_remote_features_cp *cp = payload;
		con_handle = le16toh(cp->con_handle);
		if (does_hci_con_handle_belong_to_fake_wiimote(con_handle)) {
			ret = inject_hci_event_command_status(HCI_CMD_READ_REMOTE_FEATURES);
			assert(ret == IOS_OK);
			ret = inject_hci_event_read_remote_features_compl(con_handle,
									  WIIMOTE_REMOTE_FEATURES);
			assert(ret == IOS_OK);
			handled = true;
		}
		break;
	}
	case HCI_CMD_READ_REMOTE_VER_INFO: {
		hci_read_remote_ver_info_cp *cp = payload;
		con_handle = le16toh(cp->con_handle);
		if (does_hci_con_handle_belong_to_fake_wiimote(con_handle)) {
			ret = inject_hci_event_command_status(HCI_CMD_READ_REMOTE_VER_INFO);
			assert(ret == IOS_OK);
			ret = inject_hci_event_read_remote_ver_info_compl(con_handle,
									  WIIMOTE_LMP_VERSION,
									  WIIMOTE_MANUFACTURER_ID,
									  WIIMOTE_LMP_SUBVERSION);
			assert(ret == IOS_OK);
			handled = true;
		}
		break;
	}
	case HCI_CMD_READ_CLOCK_OFFSET: {
		hci_read_clock_offset_cp *cp = payload;
		con_handle = le16toh(cp->con_handle);
		if (does_hci_con_handle_belong_to_fake_wiimote(con_handle)) {
			ret = inject_hci_event_command_status(HCI_CMD_READ_CLOCK_OFFSET);
			assert(ret == IOS_OK);
			ret = inject_hci_event_read_clock_offset_compl(con_handle, 0x3818);
			assert(ret == IOS_OK);
			handled = true;
		}
		break;
	}
	case HCI_CMD_SNIFF_MODE: {
		hci_sniff_mode_cp *cp = payload;
		con_handle = le16toh(cp->con_handle);
		if (does_hci_con_handle_belong_to_fake_wiimote(con_handle)) {
			ret = inject_hci_event_command_status(HCI_CMD_SNIFF_MODE);
			assert(ret == IOS_OK);
			ret = inject_hci_event_mode_change(con_handle, 0x02 /* sniff mode */,
							   cp->max_interval);
			assert(ret == IOS_OK);
			handled = true;
		}
		break;
	}
	case HCI_CMD_WRITE_LINK_POLICY_SETTINGS: {
		hci_write_link_policy_settings_cp *cp = payload;
		con_handle = le16toh(cp->con_handle);
		if (does_hci_con_handle_belong_to_fake_wiimote(con_handle)) {
			ret = inject_hci_event_command_status(HCI_CMD_WRITE_LINK_POLICY_SETTINGS);
			assert(ret == IOS_OK);
			handled = true;
		}
		break;
	}
	case HCI_CMD_RESET:
		for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
			if (fake_wiimotes[i].active) {
				/* Unassign the currently assigned input device (if any) */
				if (fake_wiimotes[i].input_device)
					input_device_release_wiimote(fake_wiimotes[i].input_device);
				fake_wiimotes[i].active = false;
			}
		}
		break;
	case HCI_CMD_READ_STORED_LINK_KEY: {
		hci_read_stored_link_key_cp *cp = payload;
		if (cp->read_all) {
			static bdaddr_t bdaddrs[MAX_FAKE_WIIMOTES];
			static u8 keys[MAX_FAKE_WIIMOTES][HCI_KEY_SIZE];

			for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
				bacpy(&bdaddrs[i], &FAKE_WIIMOTE_BDADDR(i));
				/* We just return 0s as the link key for our Fake Wiimote */
				memset(keys[i], 0, sizeof(keys[i]));
			}

			inject_hci_event_return_link_keys(MAX_FAKE_WIIMOTES, bdaddrs, keys);
		} else {
			if (does_bdaddr_belong_to_fake_wiimote(&cp->bdaddr, NULL)) {
				/* TODO: Return made-up link key for that particular bdaddr */
				assert(0);
				handled = true;
			}
		}
		break;
	}
	case HCI_CMD_HOST_NUM_COMPL_PKTS:
		/* TODO: Is this command ever sent actually? */
		break;
	case HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT: {
		hci_write_link_supervision_timeout_cp *cp = payload;
		con_handle = le16toh(cp->con_handle);
		if (does_hci_con_handle_belong_to_fake_wiimote(con_handle)) {
			hci_write_link_supervision_timeout_rp reply;
			reply.status = 0;
			reply.con_handle = cp->con_handle;
			ret = inject_hci_event_command_compl(HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT,
							     &reply, sizeof(reply));
			assert(ret == IOS_OK);
			handled = true;
		}
		break;
	}
	}

	return handled;
}

bool fake_wiimote_mgr_handle_acl_data_out_request_from_host(u16 hci_con_handle, const hci_acldata_hdr_t *acl)
{
	fake_wiimote_t *wiimote;

	wiimote = get_fake_wiimote_for_hci_con_handle(hci_con_handle);
	if (!wiimote)
		return false;

	fake_wiimote_handle_acl_data_out_request_from_host(wiimote, acl);

	return true;
}
