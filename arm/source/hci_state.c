#include <stdbool.h>
#include <string.h>
#include "fakedev.h"
#include "hci.h"
#include "hci_state.h"
#include "l2cap.h"
#include "utils.h"
#include "syscalls.h"

#define MAX_HCI_CONNECTIONS	32

/* Snooped HCI state (requested by SW BT stack) */
static u8 hci_unit_class[HCI_CLASS_SIZE];
static u8 hci_page_scan_enable = 0;

/* Simulated HCI state */
static struct {
	bool valid;
	u16 virt; /* The one we return to the BT SW stack */
	u16 phys; /* The one the BT dongle uses */
} hci_virt_con_handle_map_table[MAX_HCI_CONNECTIONS];


void hci_state_init()
{
	for (int i = 0; i < ARRAY_SIZE(hci_virt_con_handle_map_table); i++)
		hci_virt_con_handle_map_table[i].valid = false;

}

u16 hci_con_handle_virt_alloc(void)
{
	/* FIXME: We can have collisions after it wraps around! */
	static u16 last_virt_con_handle = 0;
	u16 ret = last_virt_con_handle;
	last_virt_con_handle = (last_virt_con_handle + 1) & 0x0EFF;
	return ret;
}

bool hci_request_connection(const bdaddr_t *bdaddr, u8 uclass0, u8 uclass1, u8 uclass2,
			    u8 link_type)
{
	int ret;

	/* If page scan is disabled the controller will not see this connection request. */
	if (!(hci_page_scan_enable & HCI_PAGE_SCAN_ENABLE))
		return false;

	ret = enqueue_hci_event_con_req(bdaddr, uclass0, uclass0, uclass1, link_type);
	return ret == IOS_OK;
}

/* HCI connection handle virt<->phys mapping */

static bool hci_virt_con_handle_map(u16 phys, u16 virt)
{
	for (int i = 0; i < ARRAY_SIZE(hci_virt_con_handle_map_table); i++) {
		if (!hci_virt_con_handle_map_table[i].valid) {
			hci_virt_con_handle_map_table[i].virt = virt;
			hci_virt_con_handle_map_table[i].phys = phys;
			hci_virt_con_handle_map_table[i].valid = 1;
			return true;
		}
	}
	return false;
}

static bool hci_virt_con_handle_unmap_phys(u16 phys)
{
	for (int i = 0; i < ARRAY_SIZE(hci_virt_con_handle_map_table); i++) {
		if (hci_virt_con_handle_map_table[i].valid &&
		    hci_virt_con_handle_map_table[i].phys == phys) {
			hci_virt_con_handle_map_table[i].valid = 0;
			return true;
		}
	}
	return false;
}

static bool hci_virt_con_handle_unmap_virt(u16 virt)
{
	for (int i = 0; i < ARRAY_SIZE(hci_virt_con_handle_map_table); i++) {
		if (hci_virt_con_handle_map_table[i].valid &&
		    hci_virt_con_handle_map_table[i].virt == virt) {
			hci_virt_con_handle_map_table[i].valid = 0;
			return true;
		}
	}
	return false;
}

static bool hci_virt_con_handle_get_virt(u16 phys, u16 *virt)
{
	for (int i = 0; i < ARRAY_SIZE(hci_virt_con_handle_map_table); i++) {
		if (hci_virt_con_handle_map_table[i].valid &&
		    hci_virt_con_handle_map_table[i].phys == phys) {
			*virt = hci_virt_con_handle_map_table[i].virt;
			return true;
		}
	}
	return false;
}

static bool hci_virt_con_handle_get_phys(u16 virt, u16 *phys)
{
	for (int i = 0; i < ARRAY_SIZE(hci_virt_con_handle_map_table); i++) {
		if (hci_virt_con_handle_map_table[i].valid &&
		    hci_virt_con_handle_map_table[i].virt == virt) {
			*phys = hci_virt_con_handle_map_table[i].phys;
			return true;
		}
	}
	return false;
}

/* HCI command handlers */

static int handle_hci_cmd_accept_con(void *data, int *fwd_to_usb)
{
	char mac[MAC_STR_LEN];
	hci_accept_con_cp *cp = data;
	static const char *roles[] = {
		"Master (0x00)",
		"Slave (0x01)",
	};

	bdaddr_to_str(mac, &cp->bdaddr);
	svc_printf("HCI_CMD_ACCEPT_CON MAC: %s, role: %s\n", mac, roles[cp->role]);

	/* If the connection was accepted on a fake device, don't
	 * forward this packet to the real USB BT dongle! */
	if (fakedev_handle_hci_cmd_accept_con(&cp->bdaddr, cp->role))
		*fwd_to_usb = 0;

	return 0;
}

static int handle_hci_cmd_write_link_policy_settings(void *data, int *fwd_to_usb)
{
	//int ret;
	hci_write_link_policy_settings_cp *policy = (void *)((u8 *)data + sizeof(hci_cmd_hdr_t));

	//svc_printf("  HCI_CMD_WRITE_LINK_POLICY_SETTINGS: handle: 0x%x, settings: 0x%x\n",
	//	(policy->con_handle), policy->settings);

	//if (con_handle is not from a fake device)
	//	return 0;

	//if (fakedev.acl_state == FAKEDEV_ACL_STATE_LINKING) {
		//svc_printf("L2CAP STARTING\n");
		//ret = l2cap_send_psm_connect_req(fakedev.bdaddr, L2CAP_PSM_HID_CNTL);
		//svc_printf("HID CNTL connect req: %d\n", ret);
		//svc_printf("L2CAP PSM HID connect req: %d\n", ret);
		//ret = l2cap_send_psm_connect_req(fakedev.bdaddr, L2CAP_PSM_HID_INTR);
		//svc_printf("L2CAP PSM INTR connect req: %d\n", ret);

		//fakedev.acl_state = FAKEDEV_ACL_STATE_LINKED;

		//*fwd_to_usb = 0;
	//}

	return 0;
}

/* Handlers for replies coming from OH1 BT dongle and going to the Host (BT SW stack).
 * It does *not* include packets that we injected. */

void hci_state_handle_hci_event(void *data, u32 length)
{
	bool ret;
	u16 virt;
	bool modified = false;
	hci_event_hdr_t *hdr = data;
	void *payload = (void *)((u8 *)hdr + sizeof(hci_event_hdr_t));

#define TRANSLATE_CON_HANDLE(event, type) \
	case event: { \
		u16 virt, phys = le16toh(((type *)payload)->con_handle); \
		assert(hci_virt_con_handle_get_virt(phys, &virt)); \
		((type *)payload)->con_handle = htole16(virt); \
		modified = true; \
		break; \
	}

	//svc_printf("hci_state_handle_hci_event: event: 0x%x, len: 0x%x\n", hdr->event, hdr->length);

	switch (hdr->event) {
	case HCI_EVENT_CON_COMPL: {
		hci_con_compl_ep *ep = payload;
		svc_printf("HCI_EVENT_CON_COMPL: status: 0x%x, handle: 0x%x\n",
			ep->status, le16toh(ep->con_handle));
		if (ep->status == 0) {
			/* Allocate a new virtual connection handle */
			virt = hci_con_handle_virt_alloc();
			/* Create the new connection handle mapping */
			ret = hci_virt_con_handle_map(le16toh(ep->con_handle), virt);
			assert(ret);
			svc_printf("New handle mapping: p 0x%x -> v 0x%x\n", le16toh(ep->con_handle), virt);
			ep->con_handle = htole16(virt);
			modified = true;
		}
		break;
	}
	case HCI_EVENT_DISCON_COMPL: {
		hci_discon_compl_ep *ep = payload;
		svc_printf("HCI_EVENT_DISCON_COMPL: status: 0x%x, handle: 0x%x, reason: 0x%x\n",
			ep->status, le16toh(ep->con_handle), ep->reason);
		if (ep->status == 0) {
			ret = hci_virt_con_handle_get_virt(le16toh(ep->con_handle), &virt);
			assert(ret);
			/* Remove the connection handle mapping */
			ret = hci_virt_con_handle_unmap_virt(virt);
			assert(ret);
			ep->con_handle = htole16(virt);
			modified = true;
		}
		break;
	}
	TRANSLATE_CON_HANDLE(HCI_EVENT_AUTH_COMPL, hci_auth_compl_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_ENCRYPTION_CHANGE, hci_encryption_change_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_CHANGE_CON_LINK_KEY_COMPL, hci_change_con_link_key_compl_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_MASTER_LINK_KEY_COMPL, hci_master_link_key_compl_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_READ_REMOTE_FEATURES_COMPL, hci_read_remote_features_compl_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_READ_REMOTE_VER_INFO_COMPL, hci_read_remote_ver_info_compl_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_QOS_SETUP_COMPL, hci_qos_setup_compl_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_FLUSH_OCCUR, hci_flush_occur_ep)
	case HCI_EVENT_NUM_COMPL_PKTS:
		/* TODO */
		assert(1);
		break;
	TRANSLATE_CON_HANDLE(HCI_EVENT_MODE_CHANGE, hci_mode_change_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_MAX_SLOT_CHANGE, hci_max_slot_change_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_READ_CLOCK_OFFSET_COMPL, hci_read_clock_offset_compl_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_CON_PKT_TYPE_CHANGED, hci_con_pkt_type_changed_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_QOS_VIOLATION, hci_qos_violation_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_FLOW_SPECIFICATION_COMPL, hci_flow_specification_compl_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_READ_REMOTE_EXTENDED_FEATURES, hci_read_remote_extended_features_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_SCO_CON_COMPL, hci_sco_con_compl_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_SCO_CON_CHANGED, hci_sco_con_changed_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_SNIFF_SUBRATING, hci_sniff_subrating_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_ENCRYPTION_KEY_REFRESH, hci_encryption_key_refresh_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_LINK_SUPERVISION_TO_CHANGED, hci_link_supervision_to_changed_ep)
	TRANSLATE_CON_HANDLE(HCI_EVENT_ENHANCED_FLUSH_COMPL, hci_enhanced_flush_compl_ep)
	}

	/* If we have modified the packet, flush from cache */
	if (modified)
		os_sync_after_write(data, sizeof(hci_event_hdr_t) + hdr->length);

#undef TRANSLATE_CON_HANDLE
}

void hci_state_handle_acl_data_in(void *data, u32 length)
{
	bool ret;
	u16 virt;
	hci_acldata_hdr_t *hdr = data;
	u16 handle = le16toh(hdr->con_handle);
	u16 phys = HCI_CON_HANDLE(handle);
	u16 pb = HCI_PB_FLAG(handle);
	u16 pc = HCI_BC_FLAG(handle);

	//svc_printf("hci_state_handle_acl_data_in: con_handle: 0x%x, len: 0x%x\n",
	//	phys, le16toh(hdr->length));

	ret = hci_virt_con_handle_get_virt(phys, &virt);
	assert(ret);
	hdr->con_handle = htole16(HCI_MK_CON_HANDLE(virt, pb, pc));

	os_sync_after_write(data, sizeof(hci_acldata_hdr_t) + hdr->length);
}

/* Handlers for requests coming from the Host (BT stack) and going to OH1.
 * It does *not* include packets that we injected. */

int hci_state_handle_hci_command(void *data, u32 length, int *fwd_to_usb)
{
	int ret = 0;
	u16 phys;
	bool modified = false;
	hci_cmd_hdr_t *hdr = data;
	void *payload = (void *)((u8 *)hdr + sizeof(hci_cmd_hdr_t));

#define TRANSLATE_CON_HANDLE_EXEC(event, type, exp) \
	case event: { \
		exp \
		u16 phys, virt = le16toh(((type *)payload)->con_handle); \
		assert(hci_virt_con_handle_get_phys(virt, &phys)); \
		((type *)payload)->con_handle = htole16(phys); \
		modified = true; \
		break; \
	}

#define TRANSLATE_CON_HANDLE(event, type) \
	TRANSLATE_CON_HANDLE_EXEC(event, type, (void)0;)

	u16 opcode = le16toh(hdr->opcode);
	//svc_printf("hci_state_handle_hci_command: opcode: 0x%x\n", opcode);

	switch (opcode) {
	case HCI_CMD_CREATE_CON:
		//svc_write("HCI_CMD_CREATE_CON\n");
		break;
	case HCI_CMD_ACCEPT_CON:
		ret = handle_hci_cmd_accept_con(payload, fwd_to_usb);
		break;
	case HCI_CMD_REJECT_CON:
		/* TODO */
		svc_write("HCI_CMD_REJECT_CON\n");
		break;
	case HCI_CMD_INQUIRY:
		//svc_write("  HCI_CMD_INQUIRY\n");
		break;
	case HCI_CMD_WRITE_SCAN_ENABLE: {
		static const char *scanning[] = {
			"HCI_NO_SCAN_ENABLE",
			"HCI_INQUIRY_SCAN_ENABLE",
			"HCI_PAGE_SCAN_ENABLE",
			"HCI_INQUIRY_AND_PAGE_SCAN_ENABLE",
		};
		hci_write_scan_enable_cp *cp = payload;
		//svc_printf("  HCI_CMD_WRITE_SCAN_ENABLE: 0x%x (%s)\n",
		//	cp->scan_enable, scanning[cp->scan_enable]);
		hci_page_scan_enable = cp->scan_enable;
		break;
	}
	case HCI_CMD_WRITE_UNIT_CLASS: {
		hci_write_unit_class_cp *cp = payload;
		//svc_printf("  HCI_CMD_WRITE_UNIT_CLASS: 0x%x 0x%x 0x%x\n",
		//	cp->uclass[0], cp->uclass[1], cp->uclass[2]);
		hci_unit_class[0] = cp->uclass[0];
		hci_unit_class[1] = cp->uclass[1];
		hci_unit_class[2] = cp->uclass[2];
		break;
	}
	case HCI_CMD_WRITE_INQUIRY_SCAN_TYPE: {
		hci_write_inquiry_scan_type_cp *cp = payload;
		//svc_printf("  HCI_CMD_WRITE_INQUIRY_SCAN_TYPE: 0x%x\n", cp->type);
		break;
	}
	case HCI_CMD_WRITE_PAGE_SCAN_TYPE: {
		hci_write_page_scan_type_cp *cp = payload;
		//svc_printf("  HCI_CMD_WRITE_PAGE_SCAN_TYPE: 0x%x\n", cp->type);
		break;
	}
	case HCI_CMD_WRITE_INQUIRY_MODE: {
		hci_write_inquiry_mode_cp *cp = payload;
		//svc_printf("  HCI_CMD_WRITE_INQUIRY_MODE: 0x%x\n", cp->mode);
		break;
	}
	case HCI_CMD_SET_EVENT_FILTER: {
		hci_set_event_filter_cp *cp = payload;
		//svc_printf("  HCI_CMD_SET_EVENT_FILTER: 0x%x 0x%x\n",
		//	  cp->filter_type, cp->filter_condition_type);
		break;
	}
	TRANSLATE_CON_HANDLE(HCI_CMD_DISCONNECT, hci_discon_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_ADD_SCO_CON, hci_add_sco_con_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_CHANGE_CON_PACKET_TYPE, hci_change_con_pkt_type_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_AUTH_REQ, hci_auth_req_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_SET_CON_ENCRYPTION, hci_set_con_encryption_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_CHANGE_CON_LINK_KEY, hci_change_con_link_key_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_REMOTE_FEATURES, hci_read_remote_features_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_REMOTE_EXTENDED_FEATURES, hci_read_remote_extended_features_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_REMOTE_VER_INFO, hci_read_remote_ver_info_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_CLOCK_OFFSET, hci_read_clock_offset_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_LMP_HANDLE, hci_read_lmp_handle_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_SETUP_SCO_CON, hci_setup_sco_con_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_HOLD_MODE, hci_hold_mode_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_SNIFF_MODE, hci_sniff_mode_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_EXIT_SNIFF_MODE, hci_exit_sniff_mode_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_PARK_MODE, hci_park_mode_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_EXIT_PARK_MODE, hci_exit_park_mode_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_QOS_SETUP, hci_qos_setup_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_ROLE_DISCOVERY, hci_role_discovery_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_LINK_POLICY_SETTINGS, hci_read_link_policy_settings_cp)
	TRANSLATE_CON_HANDLE_EXEC(HCI_CMD_WRITE_LINK_POLICY_SETTINGS, hci_write_link_policy_settings_cp,
		handle_hci_cmd_write_link_policy_settings(data, fwd_to_usb);
	)
	TRANSLATE_CON_HANDLE(HCI_CMD_FLOW_SPECIFICATION, hci_flow_specification_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_SNIFF_SUBRATING, hci_sniff_subrating_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_FLUSH, hci_flush_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_AUTO_FLUSH_TIMEOUT, hci_read_auto_flush_timeout_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_WRITE_AUTO_FLUSH_TIMEOUT, hci_write_auto_flush_timeout_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_XMIT_LEVEL, hci_read_xmit_level_cp)
	case HCI_CMD_HOST_NUM_COMPL_PKTS:
		/* TODO */
		break;
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_LINK_SUPERVISION_TIMEOUT, hci_read_link_supervision_timeout_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_WRITE_LINK_SUPERVISION_TIMEOUT, hci_write_link_supervision_timeout_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_REFRESH_ENCRYPTION_KEY, hci_refresh_encryption_key_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_ENHANCED_FLUSH, hci_enhanced_flush_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_FAILED_CONTACT_CNTR, hci_read_failed_contact_cntr_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_RESET_FAILED_CONTACT_CNTR, hci_reset_failed_contact_cntr_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_LINK_QUALITY, hci_read_link_quality_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_RSSI, hci_read_rssi_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_AFH_CHANNEL_MAP, hci_read_afh_channel_map_cp)
	TRANSLATE_CON_HANDLE(HCI_CMD_READ_CLOCK, hci_read_clock_cp)
	default:
		//svc_printf("HCI CTRL: opcode: 0x%x (ocf: 0x%x, ogf: 0x%x)\n", opcode, ocf, ogf);
		break;
	}

	/* If we have modified the packet, flush from cache */
	if (modified)
		os_sync_after_write(data, sizeof(hci_cmd_hdr_t) + hdr->length);

#undef TRANSLATE_CON_HANDLE

	return ret;
}

int hci_state_handle_acl_data_out(void *data, u32 size, int *fwd_to_usb)
{
	bool ret;
	u16 phys;
	hci_acldata_hdr_t *hdr = data;
	u16 handle = le16toh(hdr->con_handle);
	u16 virt = HCI_CON_HANDLE(handle);
	u16 pb = HCI_PB_FLAG(handle);
	u16 pc = HCI_BC_FLAG(handle);

	//svc_printf("hci_state_handle_acl_data_out: con_handle: 0x%x, len: 0x%x\n",
	//	virt, le16toh(hdr->length));

	ret = hci_virt_con_handle_get_phys(virt, &phys);
	assert(ret);
	hdr->con_handle = htole16(HCI_MK_CON_HANDLE(phys, pb, pc));

	os_sync_after_write(data, sizeof(hci_acldata_hdr_t) + hdr->length);

#if 0
	/* HCI ACL data packet header */
	hci_acldata_hdr_t *acl_hdr = data;
	u16 acl_con_handle = le16toh(HCI_CON_HANDLE(acl_hdr->con_handle));
	u16 acl_length     = le16toh(acl_hdr->length);
	void *acl_payload    = (void *)((u8 *)data + sizeof(hci_acldata_hdr_t));

	// TODO check handle

	/* L2CAP header */
	l2cap_hdr_t *l2cap_hdr = (void *)acl_payload;
	u16 l2cap_length  = le16toh(l2cap_hdr->length);
	u16 l2cap_dcid    = le16toh(l2cap_hdr->dcid);
	void *l2cap_payload = (void *)((u8 *)acl_payload + sizeof(l2cap_hdr_t));

	//svc_printf("< ACL OUT: hdl: 0x%x, len: 0x%x\n", acl_con_handle, acl_length);
	//svc_printf("  L2CAP: len: 0x%x, dcid: 0x%x\n", l2cap_length, l2cap_dcid);

	if (l2cap_dcid == L2CAP_SIGNAL_CID) {
		l2cap_cmd_hdr_t *cmd_hdr = (void *)l2cap_payload;
		u16 cmd_hdr_length = le16toh(cmd_hdr->length);
		void *cmd_payload = (void *)((u8 *)l2cap_payload + sizeof(l2cap_cmd_hdr_t));

		//svc_printf("  L2CAP CMD: code: 0x%x, ident: 0x%x, length: 0x%x\n",
		//	cmd_hdr->code, cmd_hdr->ident, cmd_hdr_length);

		switch (cmd_hdr->code) {
		case L2CAP_CONNECT_REQ: {
			l2cap_con_req_cp *con_req = cmd_payload;
			u16 con_req_psm = le16toh(con_req->psm);
			u16 con_req_scid = le16toh(con_req->scid);
			//printf("\n    L2CAP_CONNECT_REQ: psm: 0x%x, scid: 0x%x\n", con_req_psm, con_req_scid);

			/* Save PSM -> Remote CID mapping */
			//channel_map_t *chn = channels_get(con_req_scid);
			//assert(chn);
			//chn->psm = con_req_psm;
			break;
		}
		case L2CAP_CONNECT_RSP: {
			l2cap_con_rsp_cp *con_rsp = cmd_payload;
			u16 con_rsp_dcid = le16toh(con_rsp->dcid);
			u16 con_rsp_scid = le16toh(con_rsp->scid);
			u16 con_rsp_result = le16toh(con_rsp->result);
			u16 con_rsp_status = le16toh(con_rsp->status);

			//printf("\n    L2CAP_CONNECT_RSP: dcid: 0x%x, scid: 0x%x, result: 0x%x, status: 0x%x\n",
			//	con_rsp_dcid, con_rsp_scid, con_rsp_result, con_rsp_status);

			/* libogc sets it to the "dcid" and not the "result" field (and scid to 0)... */
			if ((con_rsp_dcid == L2CAP_PSM_NOT_SUPPORTED) &&
			    (con_rsp_scid == 0)) {
				//fakedev.acl_state = FAKEDEV_ACL_STATE_INACTIVE;
			}

			/* Save PSM -> Remote CID mapping */
			//channel_map_t *chn = channels_get(con_rsp_scid);
			//assert(chn);
			//chn->psm = con_rsp_psm;
			break;
		}
		case L2CAP_CONFIG_REQ: {
			l2cap_cfg_req_cp *cfg_req = cmd_payload;
			u16 cfq_req_dcid = le16toh(cfg_req->dcid);
			u16 cfq_req_flags = le16toh(cfg_req->flags);

			//printf("    L2CAP_CONFIG_REQ: dcid: 0x%x, flags: 0x%x\n", cfq_req_dcid, cfq_req_flags);
			break;
		}
		}
	}
#endif
	return 0;
}
