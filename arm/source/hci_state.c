#include <stdbool.h>
#include "hci.h"
#include "hci_state.h"
#include "l2cap.h"
#include "utils.h"
#include "syscalls.h"

#include "ipc.h"

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

/* HCI connection handle virt<->phys mapping */

u16 hci_con_handle_virt_alloc(void)
{
	static u16 last_virt_con_handle = 0;
	u16 ret = last_virt_con_handle;
	last_virt_con_handle = (last_virt_con_handle + 1) & 0x0EFF;
	return ret;
}

bool hci_virt_con_handle_map(u16 phys, u16 virt)
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

bool hci_virt_con_handle_unmap(u16 phys)
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

bool hci_virt_con_handle_get_virt(u16 phys, u16 *virt)
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

bool hci_virt_con_handle_get_phys(u16 virt, u16 *phys)
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
#if 0
	int ret;
	hci_accept_con_cp *cp = data + sizeof(hci_cmd_hdr_t);

	static const char *roles[] = {
		"Master (0x00)",
		"Slave (0x01)",
	};

	/*svc_printf("  HCI_CMD_ACCEPT_CON %02X:%02X:%02X:%02X:%02X:%02X, role: %s\n",
		cp->bdaddr.b[5], cp->bdaddr.b[4], cp->bdaddr.b[3],
		cp->bdaddr.b[2], cp->bdaddr.b[1], cp->bdaddr.b[0],
		roles[cp->role]);*/

	/* Another device, ignore it */
	//if (memcmp(&cp->bdaddr, &fakedev.bdaddr, sizeof(bdaddr_t) != 0))
	//	return 0;
	return 0;

	/* Connection accepted to our fake device */
	//svc_printf("Accepted CON for our fake device!\n");

	/* The Accept_Connection_Request command will cause the Command Status
	   event to be sent from the Host Controller when the Host Controller
	   begins setting up the connection */
	ret = enqueue_hci_event_command_status(HCI_CMD_ACCEPT_CON);
	if (ret)
		return ret;

	fakedev.baseband_state = FAKEDEV_BASEBAND_STATE_COMPLETE;
	/* We can start the ACL (L2CAP) linking now */
	fakedev.acl_state = FAKEDEV_ACL_STATE_LINKING;

	if (cp->role == HCI_ROLE_MASTER) {
		ret = enqueue_hci_event_role_change(cp->bdaddr, HCI_ROLE_MASTER);
		if (ret)
			return ret;
	}

	/* In addition, when the Link Manager determines the connection is established,
	 * the Host Controllers on both Bluetooth devices that form the connection
	 * will send a Connection Complete event to each Host */
	ret = enqueue_hci_event_con_compl(cp->bdaddr, 0);
	if (ret)
		return ret;

		//ret = l2cap_send_psm_connect_req(fakedev.bdaddr, L2CAP_PSM_HID_CNTL);
		//svc_printf("HID CNTL connect req: %d\n", ret);
		//svc_printf("L2CAP PSM HID connect req: %d\n", ret);
		//ret = l2cap_send_psm_connect_req(fakedev.bdaddr, L2CAP_PSM_HID_INTR);

	/* Now let's wait until a "HCI_CMD_WRITE_LINK_POLICY_SETTINGS" to
	 * start sending the L2CAP PSM HID connection requests... */

	*fwd_to_usb = 0;
#endif
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

/* Handlers for messages replies coming from OH1  */

void hci_state_handle_hci_event(void *data, u32 length)
{
	bool ret;
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

	//svc_printf("HCI EV: 0x%x, len: 0x%x\n", hdr->event, hdr->length);

	switch (hdr->event) {
	case HCI_EVENT_CON_COMPL: {
		hci_con_compl_ep *ep = payload;
		//svc_printf("HCI_EVENT_CON_COMPL: status: 0x%x, handle: 0x%x\n",
		//	ep->status, le16toh(ep->con_handle));
		if (ep->status == 0) {
			/* Allocate a new virtual connection handle */
			u16 virt = hci_con_handle_virt_alloc();
			/* Create the connection handle new mapping */
			ret = hci_virt_con_handle_map(le16toh(ep->con_handle), virt);
			assert(ret);
		}
		break;
	}
	case HCI_EVENT_DISCON_COMPL: {
		hci_discon_compl_ep *ep = payload;
		//svc_printf("HCI_EVENT_DISCON_COMPL: status: 0x%x, handle: 0x%x\n",
		//	ep->status, le16toh(ep->con_handle));
		if (ep->status == 0) {
			/* Remove the conneciton handle hmapping */
			ret = hci_virt_con_handle_unmap(le16toh(ep->con_handle));
			assert(ret);
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
		os_sync_after_write(data, hdr->length);

#undef TRANSLATE_CON_HANDLE
}

void hci_state_handle_acl_data_in(void *data, u32 length)
{
}

/* Handlers for messages requests going to OH1 */

int hci_state_handle_hci_command(void *data, u32 length, int *fwd_to_usb)
{
	int ret = 0;

	//svc_printf("CTL: bmRequestType 0x%x, bRequest: 0x%x, "
	//	   "wValue: 0x%x, wIndex: 0x%x, wLength: 0x%x\n",
	//	   bmRequestType, bRequest, wValue, wIndex, wLength);
	//svc_printf("  data: 0x%x 0x%x\n", *(u32 *)data, *(u32 *)(data + 4));

	hci_cmd_hdr_t *cmd_hdr = data;
	u16 opcode = le16toh(cmd_hdr->opcode);
	u16 ocf = HCI_OCF(opcode);
	u16 ogf = HCI_OGF(opcode);

	//svc_printf("EP_HCI_CTRL: opcode: 0x%x (ocf: 0x%x, ogf: 0x%x)\n", opcode, ocf, ogf);

	switch (opcode) {
	case HCI_CMD_CREATE_CON:
		//svc_write("  HCI_CMD_CREATE_CON\n");
		break;
	case HCI_CMD_ACCEPT_CON:
		//svc_write("  HCI_CMD_ACCEPT_CON\n");
		ret = handle_hci_cmd_accept_con(data, fwd_to_usb);
		break;
	case HCI_CMD_DISCONNECT:
		//svc_write("  HCI_CMD_DISCONNECT\n");
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
		hci_write_scan_enable_cp *cp = data + sizeof(hci_cmd_hdr_t);
		//svc_printf("  HCI_CMD_WRITE_SCAN_ENABLE: 0x%x (%s)\n",
		//	cp->scan_enable, scanning[cp->scan_enable]);
		hci_page_scan_enable = cp->scan_enable;
		break;
	}
	case HCI_CMD_WRITE_UNIT_CLASS: {
		hci_write_unit_class_cp *cp = data + sizeof(hci_cmd_hdr_t);
		//svc_printf("  HCI_CMD_WRITE_UNIT_CLASS: 0x%x 0x%x 0x%x\n",
		//	cp->uclass[0], cp->uclass[1], cp->uclass[2]);
		hci_unit_class[0] = cp->uclass[0];
		hci_unit_class[1] = cp->uclass[1];
		hci_unit_class[2] = cp->uclass[2];
		break;
	}
	case HCI_CMD_WRITE_INQUIRY_SCAN_TYPE: {
		hci_write_inquiry_scan_type_cp *cp = data + sizeof(hci_cmd_hdr_t);
		//svc_printf("  HCI_CMD_WRITE_INQUIRY_SCAN_TYPE: 0x%x\n", cp->type);
		break;
	}
	case HCI_CMD_WRITE_PAGE_SCAN_TYPE: {
		hci_write_page_scan_type_cp *cp = data + sizeof(hci_cmd_hdr_t);
		//svc_printf("  HCI_CMD_WRITE_PAGE_SCAN_TYPE: 0x%x\n", cp->type);
		break;
	}
	case HCI_CMD_WRITE_INQUIRY_MODE: {
		hci_write_inquiry_mode_cp *cp = data + sizeof(hci_cmd_hdr_t);
		//svc_printf("  HCI_CMD_WRITE_INQUIRY_MODE: 0x%x\n", cp->mode);
		break;
	}
	case HCI_CMD_SET_EVENT_FILTER: {
		hci_set_event_filter_cp *cp = data + sizeof(hci_cmd_hdr_t);
		//svc_printf("  HCI_CMD_SET_EVENT_FILTER: 0x%x 0x%x\n",
		//	  cp->filter_type, cp->filter_condition_type);
		break;
	}
	case HCI_CMD_WRITE_LINK_POLICY_SETTINGS:
		ret = handle_hci_cmd_write_link_policy_settings(data, fwd_to_usb);
		break;
	default:
		//svc_printf("HCI CTRL: opcode: 0x%x (ocf: 0x%x, ogf: 0x%x)\n", opcode, ocf, ogf);
		break;
	}

	return ret;
}

int hci_state_handle_acl_data_out(void *data, u32 size, int *fwd_to_usb)
{
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

	return 0;
}
