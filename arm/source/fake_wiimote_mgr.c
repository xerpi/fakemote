#include <string.h>
#include "fake_wiimote_mgr.h"
#include "hci.h"
#include "hci_state.h"
#include "l2cap.h"
#include "syscalls.h"
#include "utils.h"
#include "wiimote.h"

typedef enum {
	BASEBAND_STATE_INACTIVE,
	BASEBAND_STATE_REQUEST_CONNECTION,
	BASEBAND_STATE_COMPLETE
} baseband_state_e;

typedef enum {
	ACL_STATE_INACTIVE,
	ACL_STATE_LINKING
} acl_state_e;

typedef enum {
	L2CAP_CHANNEL_STATE_INACTIVE_INACTIVE,
	L2CAP_CHANNEL_STATE_INACTIVE_CONFIG_PEND,
	L2CAP_CHANNEL_STATE_INACTIVE_COMPLETE
} l2cap_channel_state_e;

typedef struct {
	bool valid;
	l2cap_channel_state_e state;
	u16 psm;
	u16 local_cid;
	u16 remote_cid;
	u16 remote_mtu;
} l2cap_channel_info_t;

typedef struct fake_wiimote_t {
	bool active;
	bdaddr_t bdaddr;
	/* Bluetooth connection state */
	u16 hci_con_handle;
	baseband_state_e baseband_state;
	acl_state_e acl_state;
	l2cap_channel_info_t psm_sdp_chn;
	l2cap_channel_info_t psm_hid_cntl_chn;
	l2cap_channel_info_t psm_hid_intr_chn;
	/* Associated input device with this fake Wiimote */
	void *usrdata;
	const input_device_ops_t *input_device_ops;
	/* Input state */
	u16 buttons;
} fake_wiimote_t;

static fake_wiimote_t fake_wiimotes[MAX_FAKE_WIIMOTES];

/* Helper functions */

static inline bool fake_wiimote_is_connected(const fake_wiimote_t *wiimote)
{
	return wiimote->baseband_state == BASEBAND_STATE_COMPLETE;
}

/* Channel bookkeeping */

static inline u16 generate_l2cap_channel_id(void)
{
	/* "Identifiers from 0x0001 to 0x003F are reserved" */
	static u16 starting_id = 0x40;
	return starting_id++;
}

static inline bool l2cap_channel_is_accepted(const l2cap_channel_info_t *info)
{
	return info->valid && (info->remote_cid != L2CAP_NULL_CID);
}

static inline bool l2cap_channel_is_is_remote_configured(const l2cap_channel_info_t *info)
{
	return info->valid && (info->remote_mtu != 0);
}

static inline bool l2cap_channel_is_complete(const l2cap_channel_info_t *info)
{
	return info->valid &&
	       l2cap_channel_is_accepted(info) &&
	       l2cap_channel_is_is_remote_configured(info) &&
	       (info->state == L2CAP_CHANNEL_STATE_INACTIVE_COMPLETE);
}

static l2cap_channel_info_t *get_channel_info(fake_wiimote_t *dev, u16 local_cid)
{
	if (dev->psm_sdp_chn.valid && (local_cid == dev->psm_sdp_chn.local_cid)) {
		return &dev->psm_sdp_chn;
	} else if (dev->psm_hid_cntl_chn.valid && (local_cid == dev->psm_hid_cntl_chn.local_cid)) {
		return &dev->psm_hid_cntl_chn;
	} else if (dev->psm_hid_intr_chn.valid && (local_cid == dev->psm_hid_intr_chn.local_cid)) {
		return &dev->psm_hid_intr_chn;
	}
	return NULL;
}

static void l2cap_channel_info_setup(l2cap_channel_info_t *info, u16 psm, u16 local_cid)
{
	info->psm = psm;
	info->state = L2CAP_CHANNEL_STATE_INACTIVE_INACTIVE;
	info->local_cid = local_cid;
	info->remote_cid = L2CAP_NULL_CID;
	info->remote_mtu = 0;
	info->valid = true;
}

/* HID reports */

static int send_hid_data(u16 hci_con_handle, u16 dcid, u8 hid_type, const void *data, u32 size)
{
	u8 buf[WIIMOTE_MAX_PAYLOAD];
	assert(size <= (WIIMOTE_MAX_PAYLOAD - 1));
	buf[0] = hid_type;
	memcpy(&buf[1], data, size);
	return l2cap_send_msg(hci_con_handle, dcid, buf, size + 1);
}

static inline int send_hid_input_report(u16 hci_con_handle, u16 dcid, u8 report_id,
					const void *data, u32 size)
{
	u8 buf[WIIMOTE_MAX_PAYLOAD - 1];
	assert(size <= (WIIMOTE_MAX_PAYLOAD - 2));
	buf[0] = report_id;
	memcpy(&buf[1], data, size);
	return send_hid_data(hci_con_handle, dcid, (HID_TYPE_DATA << 4) | HID_PARAM_INPUT, buf, size + 1);
}

static int wiimote_send_ack(const fake_wiimote_t *wiimote, u8 rpt_id, u8 error_code)
{
	struct wiimote_input_report_ack_t ack ATTRIBUTE_ALIGN(32);
	ack.buttons = 0;
	ack.rpt_id = rpt_id;
	ack.error_code = error_code;
	return send_hid_input_report(wiimote->hci_con_handle, wiimote->psm_hid_intr_chn.remote_cid,
				     INPUT_REPORT_ID_ACK, &ack, sizeof(ack));
}

/* Disconnection helper functions */

static inline int disconnect_l2cap_channel(u16 hci_con_handle, l2cap_channel_info_t *info)
{
	int ret;
	ret = l2cap_send_disconnect_req(hci_con_handle, info->remote_cid, info->local_cid);
	info->valid = false;
	return ret;
}

static int fake_wiimote_disconnect(fake_wiimote_t *wiimote)
{
	int ret = 0, ret2;

	if (l2cap_channel_is_accepted(&wiimote->psm_sdp_chn)) {
		ret2 = disconnect_l2cap_channel(wiimote->hci_con_handle, &wiimote->psm_sdp_chn);
		if (ret2 < 0 && ret == 0)
			ret = ret2;
	}

	if (l2cap_channel_is_accepted(&wiimote->psm_hid_cntl_chn)) {
		ret2 = disconnect_l2cap_channel(wiimote->hci_con_handle, &wiimote->psm_hid_cntl_chn);
		if (ret2 < 0 && ret == 0)
			ret = ret2;
	}

	if (l2cap_channel_is_accepted(&wiimote->psm_hid_intr_chn)) {
		ret2 = disconnect_l2cap_channel(wiimote->hci_con_handle, &wiimote->psm_hid_intr_chn);
		if (ret2 < 0 && ret == 0)
			ret = ret2;
	}

	if (wiimote->baseband_state == BASEBAND_STATE_COMPLETE) {
		ret2 = enqueue_hci_event_discon_compl(wiimote->hci_con_handle,
						      0, 0x13 /* User Ended Connection */);
		if (ret2 < 0 && ret == 0)
			ret = ret2;
	}

	wiimote->active = false;

	return ret;
}

/* Init state */

void fake_wiimote_mgr_init(void)
{
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		fake_wiimotes[i].active = false;
		fake_wiimotes[i].bdaddr = FAKE_WIIMOTE_BDADDR(i);
		fake_wiimotes[i].baseband_state = BASEBAND_STATE_INACTIVE;
		fake_wiimotes[i].acl_state = L2CAP_CHANNEL_STATE_INACTIVE_INACTIVE;
		fake_wiimotes[i].psm_sdp_chn.valid = false;
		fake_wiimotes[i].psm_hid_cntl_chn.valid = false;
		fake_wiimotes[i].psm_hid_intr_chn.valid = false;
		fake_wiimotes[i].usrdata = NULL;
		fake_wiimotes[i].input_device_ops = NULL;
		fake_wiimotes[i].buttons = 0;
	}
}

bool fake_wiimote_mgr_add_input_device(void *usrdata, const input_device_ops_t *ops)
{
	if (!ops)
		return false;

	/* Find an inactive fake Wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (!fake_wiimotes[i].active) {
			fake_wiimotes[i].baseband_state = BASEBAND_STATE_REQUEST_CONNECTION;
			fake_wiimotes[i].usrdata = usrdata;
			fake_wiimotes[i].input_device_ops = ops;
			fake_wiimotes[i].active = true;
			return true;
		}
	}
	return false;
}

bool fake_wiimote_mgr_remove_input_device(fake_wiimote_t *wiimote)
{
	return fake_wiimote_disconnect(wiimote) == IOS_OK;
}

int fake_wiimote_mgr_report_input(fake_wiimote_t *wiimote, u16 buttons)
{
	int ret = IOS_OK;
	u16 changed = wiimote->buttons ^ buttons;

	if (changed) {
		if (l2cap_channel_is_complete(&wiimote->psm_hid_intr_chn)) {
			ret = send_hid_input_report(wiimote->hci_con_handle,
						    wiimote->psm_hid_intr_chn.remote_cid,
						    INPUT_REPORT_ID_REPORT_CORE,
						    &buttons, sizeof(buttons));
		}
		wiimote->buttons = buttons;
	}

	return ret;
}

static void check_send_config_for_new_channel(u16 hci_con_handle, l2cap_channel_info_t *info)
{
	int ret;

	if (l2cap_channel_is_accepted(info) &&
	    (info->state == L2CAP_CHANNEL_STATE_INACTIVE_INACTIVE)) {
		ret = l2cap_send_config_req(hci_con_handle, info->remote_cid,
					    WII_REQUEST_MTU, L2CAP_FLUSH_TIMO_DEFAULT);
		if (ret == IOS_OK) {
			info->state = L2CAP_CHANNEL_STATE_INACTIVE_CONFIG_PEND;
		}
	}
}

static void fake_wiimote_tick(fake_wiimote_t *wiimote)
{
	int ret;
	bool req;

	if (wiimote->baseband_state == BASEBAND_STATE_REQUEST_CONNECTION) {
		req = hci_request_connection(&wiimote->bdaddr, WIIMOTE_HCI_CLASS_0,
					     WIIMOTE_HCI_CLASS_1, WIIMOTE_HCI_CLASS_2,
					     HCI_LINK_ACL);
		/* After a connection request is visible to the controller switch to inactive */
		if (req)
			wiimote->baseband_state = BASEBAND_STATE_INACTIVE;
	} else if (wiimote->baseband_state == BASEBAND_STATE_COMPLETE) {
		/* "If the connection originated from the device (Wiimote) it will create
		 * HID control and interrupt channels (in that order)." */
		if (wiimote->acl_state == ACL_STATE_LINKING) {
			/* If-else-if cascade to avoid sending too many packets on the same "tick" */
			if (!wiimote->psm_hid_cntl_chn.valid) {
				u16 local_cid = generate_l2cap_channel_id();
				ret = l2cap_send_connect_req(wiimote->hci_con_handle, L2CAP_PSM_HID_CNTL,
							     local_cid);
				assert(ret == IOS_OK);
				l2cap_channel_info_setup(&wiimote->psm_hid_cntl_chn, L2CAP_PSM_HID_CNTL, local_cid);
				DEBUG("Generated local CID for HID CNTL: 0x%x\n", local_cid);
			} else if (!wiimote->psm_hid_intr_chn.valid) {
				u16 local_cid = generate_l2cap_channel_id();
				ret = l2cap_send_connect_req(wiimote->hci_con_handle, L2CAP_PSM_HID_INTR,
							     local_cid);
				assert(ret == IOS_OK);
				l2cap_channel_info_setup(&wiimote->psm_hid_intr_chn, L2CAP_PSM_HID_INTR, local_cid);
				DEBUG("Generated local CID for HID INTR: 0x%x\n", local_cid);
			} else if (l2cap_channel_is_complete(&wiimote->psm_hid_cntl_chn) &&
				   l2cap_channel_is_complete(&wiimote->psm_hid_intr_chn)) {
				wiimote->acl_state = ACL_STATE_INACTIVE;
				/* Call assigned() input_device callback */
				wiimote->input_device_ops->assigned(wiimote->usrdata, wiimote);
			}
		}

		/* Send configuration for any newly connected channels. */
		check_send_config_for_new_channel(wiimote->hci_con_handle, &wiimote->psm_sdp_chn);
		check_send_config_for_new_channel(wiimote->hci_con_handle, &wiimote->psm_hid_cntl_chn);
		check_send_config_for_new_channel(wiimote->hci_con_handle, &wiimote->psm_hid_intr_chn);
	}
}

void fake_wiimote_mgr_tick_devices(void)
{
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (fake_wiimotes[i].active)
			fake_wiimote_tick(&fake_wiimotes[i]);
	}
}

/* Functions called by the HCI state manager */

bool fake_wiimote_mgr_handle_hci_cmd_accept_con(const bdaddr_t *bdaddr, u8 role)
{
	int ret;

	/* Check if the bdaddr belongs to a fake wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (memcmp(bdaddr, &fake_wiimotes[i].bdaddr, sizeof(bdaddr_t) != 0))
			continue;

		/* Connection accepted to our fake wiimote */
		DEBUG("Connection accepted for fake Wiimote %d!\n", i);

		/* The Accept_Connection_Request command will cause the Command Status
		   event to be sent from the Host Controller when the Host Controller
		   begins setting up the connection */

		ret = enqueue_hci_event_command_status(HCI_CMD_ACCEPT_CON);
		assert(ret == IOS_OK);

		fake_wiimotes[i].baseband_state = BASEBAND_STATE_COMPLETE;
		fake_wiimotes[i].hci_con_handle = hci_con_handle_virt_alloc();
		DEBUG("Fake Wiimote %d got HCI con_handle: 0x%x\n", i, fake_wiimotes[i].hci_con_handle);

		/* We can start the ACL (L2CAP) linking now */
		fake_wiimotes[i].acl_state = ACL_STATE_LINKING;

		if (role == HCI_ROLE_MASTER) {
			ret = enqueue_hci_event_role_change(bdaddr, HCI_ROLE_MASTER);
			assert(ret == IOS_OK);
		}

		/* In addition, when the Link Manager determines the connection is established,
		 * the Host Controllers on both Bluetooth devices that form the connection
		 * will send a Connection Complete event to each Host */
		ret = enqueue_hci_event_con_compl(bdaddr, fake_wiimotes[i].hci_con_handle, 0);
		assert(ret == IOS_OK);

		DEBUG("Connection complete sent, starting ACL linking!\n");
		return true;
	}

	return false;
}

bool fake_wiimote_mgr_handle_hci_cmd_reject_con(const bdaddr_t *bdaddr, u8 reason)
{
	/* Check if the bdaddr belongs to a fake wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (memcmp(bdaddr, &fake_wiimotes[i].bdaddr, sizeof(bdaddr_t) != 0))
			continue;

		/* Connection rejected to our fake wiimote. Disconnect */
		DEBUG("Connection to fake Wiimote %d rejected!\n", i);
		fake_wiimote_disconnect(&fake_wiimotes[i]);
		return true;
	}

	return false;
}

bool fake_wiimote_mgr_handle_hci_cmd_from_host(u16 hci_con_handle, const hci_cmd_hdr_t *hdr)
{
	/* Check if the HCI connection handle belongs to a fake wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (!fake_wiimote_is_connected(&fake_wiimotes[i]) ||
		    (fake_wiimotes[i].hci_con_handle != hci_con_handle))
			continue;

		/* Do we have to do anything here? We already have handle_hci_cmd_xxxx calls. */
		DEBUG("FAKEDEV H > C HCI CMD: 0x%x\n", le16toh(hdr->opcode));
		return true;
	}

	return false;
}

static void handle_l2cap_config_req(fake_wiimote_t *wiimote, u8 ident, u16 dcid, u16 flags,
				    const u8 *options, u16 options_size)
{
	u8 tmp[256];
	u32 opt_size;
	l2cap_channel_info_t *info;
	l2cap_cfg_opt_t *opt;
	l2cap_cfg_opt_val_t *val;
	l2cap_cfg_rsp_cp *rsp = (l2cap_cfg_rsp_cp *)tmp;
	u32 offset = 0;
	u32 resp_len = 0;
	/* If the option is not provided, configure the default. */
	u16 remote_mtu = L2CAP_MTU_DEFAULT;

	assert(flags == 0x00);
	assert(options_size <= sizeof(tmp));

	info = get_channel_info(wiimote, dcid);
	assert(info);

	/* Response to the config request */
	rsp->scid = htole16(info->remote_cid);
	rsp->flags = htole16(0x00);
	rsp->result = htole16(L2CAP_SUCCESS);
	resp_len += sizeof(l2cap_cfg_rsp_cp);

	/* Read configuration options. */
	while (offset < options_size) {
		opt = (l2cap_cfg_opt_t *)&options[offset];
		offset += sizeof(l2cap_cfg_opt_t);
		val = (l2cap_cfg_opt_val_t *)&options[offset];

		switch (opt->type) {
		case L2CAP_OPT_MTU:
			assert(opt->length == L2CAP_OPT_MTU_SIZE);
			remote_mtu = le16toh(val->mtu);
			DEBUG("      MTU configured to: 0x%x\n", remote_mtu);
			break;
		/* We don't care what the flush timeout is. Our packets are not dropped. */
		case L2CAP_OPT_FLUSH_TIMO:
			assert(opt->length == L2CAP_OPT_FLUSH_TIMO_SIZE);
			DEBUG("      Flush timeout configured to 0x%x\n", val->flush_timo);
			break;
		default:
			DEBUG("      Unknown Option: 0x%02x", opt->type);
			break;
		}

		offset += opt->length;
		opt_size = sizeof(l2cap_cfg_opt_t) + opt->length;
		memcpy(&tmp[resp_len], options, opt_size);
		resp_len += opt_size;
	}

	/* Send Respone */
	l2cap_send_config_rsp(wiimote->hci_con_handle, dcid, ident, tmp, resp_len);

	/* Set the MTU */
	info->remote_mtu = remote_mtu;
}

static void handle_l2cap_signal_channel(fake_wiimote_t *wiimote, u8 code, u8 ident,
					const void *payload, u16 size)
{
	l2cap_channel_info_t *info;

	DEBUG("  signal channel: code: 0x%x, ident: 0x%x\n", code, ident);

	switch (code) {
	case L2CAP_CONNECT_REQ: {
		const l2cap_con_req_cp *req = payload;
		u16 psm = le16toh(req->psm);
		u16 scid = le16toh(req->scid);
		UNUSED(psm);
		UNUSED(scid);
		DEBUG("  L2CAP_CONNECT_REQ: psm: 0x%x, scid: 0x%x\n", psm, scid);
		/* TODO */
		break;
	}
	case L2CAP_CONNECT_RSP: {
		const l2cap_con_rsp_cp *rsp = payload;
		u16 dcid = le16toh(rsp->dcid);
		u16 scid = le16toh(rsp->scid);
		u16 result = le16toh(rsp->result);
		u16 status = le16toh(rsp->status);
		DEBUG("  L2CAP_CONNECT_RSP: dcid: 0x%x, scid: 0x%x, result: 0x%x, status: 0x%x\n",
			dcid, scid, result, status);

		/* libogc/master/lwbt/l2cap.c#L318 sets it to the "dcid" and not to
		 * the "result" field (and scid to 0)... */
		if ((result != L2CAP_SUCCESS) || ((dcid == L2CAP_PSM_NOT_SUPPORTED) &&
						  (scid == 0))) {
			fake_wiimote_disconnect(wiimote);
			break;
		}

		assert(status == L2CAP_NO_INFO);
		info = get_channel_info(wiimote, scid);
		assert(info);

		/* Save endpoint's Destination CID  */
		info->remote_cid = dcid;
		break;
	}
	case L2CAP_CONFIG_REQ: {
		const l2cap_cfg_req_cp *rsp = payload;
		u16 dcid = le16toh(rsp->dcid);
		u16 flags = le16toh(rsp->flags);
		const void *options = (const void *)((u8 *)rsp + sizeof(l2cap_cfg_req_cp));
		UNUSED(flags);

		DEBUG("  L2CAP_CONFIG_REQ: dcid: 0x%x, flags: 0x%x\n", dcid, flags);
		handle_l2cap_config_req(wiimote, ident, dcid, flags, options,
					size - sizeof(l2cap_cfg_req_cp));
		break;
	}
	case L2CAP_CONFIG_RSP: {
		const l2cap_cfg_rsp_cp *rsp = payload;
		u16 scid = le16toh(rsp->scid);
		u16 flags = le16toh(rsp->flags);
		u16 result = le16toh(rsp->result);
		UNUSED(flags);
		DEBUG("  L2CAP_CONFIG_RSP: scid: 0x%x, flags: 0x%x, result: 0x%x\n",
			scid, flags, result);

		assert(result == L2CAP_SUCCESS);
		info = get_channel_info(wiimote, scid);
		assert(info);

		/* Mark channel as complete!  */
		info->state = L2CAP_CHANNEL_STATE_INACTIVE_COMPLETE;
		break;
	}
	}
}

static void handle_l2cap_signal_channel_request(fake_wiimote_t *wiimote, const void *data, u16 length)
{
	const l2cap_cmd_hdr_t *cmd_hdr;
	const void *cmd_payload;
	u16 cmd_len;

	while (length >= sizeof(l2cap_cmd_hdr_t)) {
		cmd_hdr = (const void *)data;
		cmd_len = le16toh(cmd_hdr->length);
		cmd_payload = (const void *)((u8 *)data + sizeof(*cmd_hdr));

		handle_l2cap_signal_channel(wiimote, cmd_hdr->code, cmd_hdr->ident,
					    cmd_payload, cmd_len);

		data += sizeof(l2cap_cmd_hdr_t) + cmd_len;
		length -= sizeof(l2cap_cmd_hdr_t) + cmd_len;
	}
}

static void extension_read_data(void *dst, u16 address, u16 size)
{
	struct wiimote_extension_registers_t regs;
	assert(address + size <= sizeof(regs));

	memset(&regs, 0, sizeof(regs));
	/* Good for now... */
	memcpy(regs.identifier, EXT_NUNCHUNK_ID, sizeof(regs.identifier));

	/* Copy the requested data from the extension registers */
	memcpy(dst, ((u8 *)&regs) + address, size);
}

static void handle_hid_intr_data_output(fake_wiimote_t *wiimote, const u8 *data, u16 size)
{
	DEBUG("handle_hid_intr_data_output: size: 0x%x, 0x%x\n", size, *(u32 *)(data-1));

	if (size == 0)
		return;

	switch (data[0]) {
	case OUTPUT_REPORT_ID_LED: {
		struct wiimote_output_report_led_t *led = (void *)&data[1];
		/* Call set_leds() input_device callback */
		if (wiimote->input_device_ops->set_leds)
			wiimote->input_device_ops->set_leds(wiimote->usrdata, led->leds);
		if (led->ack)
			wiimote_send_ack(wiimote, OUTPUT_REPORT_ID_LED, ERROR_CODE_SUCCESS);
		break;
	}
	case OUTPUT_REPORT_ID_STATUS: {
		struct wiimote_input_report_status_t status;
		memset(&status, 0, sizeof(status));
		status.extension = 1;
		status.buttons = 0;
		send_hid_input_report(wiimote->hci_con_handle,
				     wiimote->psm_hid_intr_chn.remote_cid,
				     INPUT_REPORT_ID_STATUS, &status, sizeof(status));
		break;
	}
	case OUTPUT_REPORT_ID_REPORT_MODE: {
		struct wiimote_output_report_mode_t *mode = (void *)&data[1];
		DEBUG("  Report mode: 0x%02x, cont: %d, rumble: %d, ack: %d\n",
			mode->mode, mode->continuous, mode->rumble, mode->ack);
		if (mode->ack)
			wiimote_send_ack(wiimote, OUTPUT_REPORT_ID_REPORT_MODE, ERROR_CODE_SUCCESS);
		break;
	}
	case OUTPUT_REPORT_ID_WRITE_DATA: {
		struct wiimote_output_report_write_data_t *write =  (void *)&data[1];
		UNUSED(write);
		DEBUG("  Write data to slave 0x%02x, address: 0x%x, size: 0x%x 0x%x\n",
			write->slave_address, write->address, write->size, write->data[0]);
		/* TODO */
		/* Write data is, among other things, used to decrypt the extension bytes */
		wiimote_send_ack(wiimote, OUTPUT_REPORT_ID_WRITE_DATA, ERROR_CODE_SUCCESS);
		break;
	}
	case OUTPUT_REPORT_ID_READ_DATA: {
		struct wiimote_output_report_read_data_t *read = (void *)&data[1];
		struct wiimote_input_report_read_data_t reply;
		u16 offset, size, read_size;
		u8 error = ERROR_CODE_SUCCESS;
		memset(&reply.data, 0, sizeof(reply.data));

		DEBUG("  Read data from slave 0x%02x, addrspace: %d, address: 0x%x, size: 0x%x\n\n",
			read->slave_address, read->space, read->address, read->size);

		if (read->size == 0)
			break;

		offset = read->address;
		size = read->size;
		/* TODO: Move this to an update() function that runs periodically */
		while (size > 0) {
			read_size = (size < 16) ? size : 16;
			switch (read->space) {
			case ADDRESS_SPACE_EEPROM:
				/* TODO */
				break;
			case ADDRESS_SPACE_I2C_BUS:
			case ADDRESS_SPACE_I2C_BUS_ALT:
				if (read->slave_address == EXTENSION_I2C_ADDR) {
					extension_read_data(reply.data, offset, read_size);
					break;
				} else if (read->slave_address == EEPROM_I2C_ADDR) {
					error = ERROR_CODE_INVALID_ADDRESS;
					break;
				}
				break;
			default:
				error = ERROR_CODE_INVALID_SPACE;
				break;
			}

			reply.buttons = 0;
			reply.size_minus_one = read_size - 1;
			reply.error = error;
			reply.address = offset;
			send_hid_input_report(wiimote->hci_con_handle,
					      wiimote->psm_hid_intr_chn.remote_cid,
					      INPUT_REPORT_ID_READ_DATA_REPLY,
					      &reply, sizeof(reply));
			if (error != ERROR_CODE_SUCCESS)
				break;
			offset += 16;
			size -= read_size;
		}
		break;
	}
	default:
		DEBUG("Unhandled output report: 0x%x\n", data[0]);
		break;
	}
}

bool fake_wiimote_mgr_handle_acl_data_out_request_from_host(u16 hci_con_handle, const hci_acldata_hdr_t *acl)
{
	const l2cap_hdr_t *header;
	u16 dcid, length;
	const u8 *payload;

	/* Check if the HCI connection handle belongs to a fake wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (!fake_wiimote_is_connected(&fake_wiimotes[i]) ||
		    (fake_wiimotes[i].hci_con_handle != hci_con_handle))
			continue;

		/* L2CAP header */
		header  = (const void *)((u8 *)acl + sizeof(hci_acldata_hdr_t));
		length  = le16toh(header->length);
		dcid    = le16toh(header->dcid);
		payload = (u8 *)header + sizeof(l2cap_hdr_t);

		DEBUG("FD ACL OUT: con_handle: 0x%x, dcid: 0x%x, len: 0x%x\n", hci_con_handle, dcid, length);

		if (dcid == L2CAP_SIGNAL_CID) {
			handle_l2cap_signal_channel_request(&fake_wiimotes[i], payload, length);
		} else {
			l2cap_channel_info_t *info = get_channel_info(&fake_wiimotes[i], dcid);
			if (info) {
				switch (info->psm) {
				case L2CAP_PSM_SDP:
					/* TODO */
					DEBUG("  PSM HID SDP\n");
					break;
				case L2CAP_PSM_HID_CNTL:
					/* TODO */
					DEBUG("  PSM HID CNTL\n");
					break;
				case L2CAP_PSM_HID_INTR:
					if (payload[0] == ((HID_TYPE_DATA << 4) | HID_PARAM_OUTPUT))
						handle_hid_intr_data_output(&fake_wiimotes[i],
									    &payload[1],
									    length - 1);
					break;
				}
			} else {
				DEBUG("Received L2CAP packet to unknown channel: 0x%x\n", dcid);
			}
		}
		return true;
	}

	return false;
}
