#include <string.h>
#include "fakedev.h"
#include "hci_state.h"
#include "l2cap.h"
#include "syscalls.h"
#include "utils.h"

/* Wiimote definitions */
#define WIIMOTE_HCI_CLASS_0 0x00
#define WIIMOTE_HCI_CLASS_1 0x04
#define WIIMOTE_HCI_CLASS_2 0x48
#define WII_REQUEST_MTU     185

/* Fake devices */
// 00:21:BD:2D:57:FF Name: Nintendo RVL-CNT-01

typedef enum {
	FAKEDEV_BASEBAND_STATE_INACTIVE,
	FAKEDEV_BASEBAND_STATE_REQUEST_CONNECTION,
	FAKEDEV_BASEBAND_STATE_COMPLETE
} fakedev_baseband_state_e;

typedef enum {
	FAKEDEV_ACL_STATE_INACTIVE,
	FAKEDEV_ACL_STATE_LINKING
} fakedev_acl_state_e;

typedef enum {
	FAKEDEV_L2CAP_CHANNEL_STATE_INACTIVE,
	FAKEDEV_L2CAP_CHANNEL_STATE_CONFIG_PEND,
	FAKEDEV_L2CAP_CHANNEL_STATE_COMPLETE
} fakedev_l2cap_channel_state_e;

typedef struct {
	bool valid;
	fakedev_l2cap_channel_state_e state;
	u16 local_cid;
	u16 remote_cid;
	u16 remote_mtu;
} l2cap_channel_info_t;

typedef struct {
	bdaddr_t bdaddr;
	u16 hci_con_handle;
	fakedev_baseband_state_e baseband_state;
	fakedev_acl_state_e acl_state;
	l2cap_channel_info_t l2cap_psm_sdp_channel;
	l2cap_channel_info_t l2cap_psm_hid_cntl_channel;
	l2cap_channel_info_t l2cap_psm_hid_intr_channel;
} fakedev_t;

static fakedev_t fakedev = {
	.bdaddr = {.b = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}},
	.baseband_state = FAKEDEV_BASEBAND_STATE_INACTIVE,
	.acl_state = FAKEDEV_ACL_STATE_INACTIVE,
};

/* Helper functions */

static inline bool fakedev_is_connected(void)
{
	return fakedev.baseband_state == FAKEDEV_BASEBAND_STATE_COMPLETE;
}

/* Channel bookkeeping */

static inline u16 generate_l2cap_channel_id(void)
{
	/* "Identifiers from 0x0001 to 0x003F are reserved" */
	static u16 starting_id = 0x40;
	return starting_id++;
}

static bool is_l2cap_channel_accepted(const l2cap_channel_info_t *info)
{
	return info->valid && (info->remote_cid != L2CAP_NULL_CID);
}

static bool is_l2cap_channel_is_remote_configured(const l2cap_channel_info_t *info)
{
	return info->valid && (info->remote_mtu != 0);
}

static bool is_l2cap_channel_complete(const l2cap_channel_info_t *info)
{
	return info->valid &&
	       is_l2cap_channel_accepted(info) &&
	       is_l2cap_channel_is_remote_configured(info) &&
	       (info->state == FAKEDEV_L2CAP_CHANNEL_STATE_COMPLETE);
}

/* Init state */

void fakedev_init(void)
{
	fakedev.baseband_state = FAKEDEV_BASEBAND_STATE_INACTIVE;
	fakedev.acl_state = FAKEDEV_L2CAP_CHANNEL_STATE_INACTIVE;
	fakedev.l2cap_psm_sdp_channel.valid = false;
	fakedev.l2cap_psm_hid_cntl_channel.valid = false;
	fakedev.l2cap_psm_hid_intr_channel.valid = false;

	/* Activate */
	fakedev.baseband_state = FAKEDEV_BASEBAND_STATE_REQUEST_CONNECTION;
}

/* Functions called by the HCI state manager */

static void check_send_config_for_new_channel(l2cap_channel_info_t *info)
{
	int ret;

	if (is_l2cap_channel_accepted(info) &&
	    (info->state == FAKEDEV_L2CAP_CHANNEL_STATE_INACTIVE)) {
		ret = l2cap_send_psm_config_req(fakedev.hci_con_handle, info->remote_cid,
						WII_REQUEST_MTU, L2CAP_FLUSH_TIMO_DEFAULT);
		if (ret == IOS_OK) {
			info->remote_mtu = WII_REQUEST_MTU;
			info->state = FAKEDEV_L2CAP_CHANNEL_STATE_CONFIG_PEND;
		}
		printf("L2CAP send config req: %d\n", ret);
	}
}

void fakedev_tick_devices(void)
{
	int ret;
	bool req;

	if (fakedev.baseband_state == FAKEDEV_BASEBAND_STATE_REQUEST_CONNECTION) {
		req = hci_request_connection(&fakedev.bdaddr, WIIMOTE_HCI_CLASS_0,
					     WIIMOTE_HCI_CLASS_1, WIIMOTE_HCI_CLASS_2,
					     HCI_LINK_ACL);
		/* After a connection request is visible to the controller switch to inactive */
		if (req)
			fakedev.baseband_state = FAKEDEV_BASEBAND_STATE_INACTIVE;
	}

	if (!fakedev_is_connected())
		return;

	/*  Send configuration for any newly connected channels. */
	check_send_config_for_new_channel(&fakedev.l2cap_psm_sdp_channel);
	check_send_config_for_new_channel(&fakedev.l2cap_psm_hid_cntl_channel);
	check_send_config_for_new_channel(&fakedev.l2cap_psm_hid_intr_channel);

	/* "If the connection originated from the device (Wiimote) it will create
	 * HID control and interrupt channels (in that order)." */
	if (fakedev.acl_state == FAKEDEV_ACL_STATE_LINKING) {
		if (!fakedev.l2cap_psm_hid_cntl_channel.valid) {
			u16 local_cid = generate_l2cap_channel_id();
			ret = l2cap_send_psm_connect_req(fakedev.hci_con_handle,
							 L2CAP_PSM_HID_CNTL,
							 local_cid);
			assert(ret == IOS_OK);
			fakedev.l2cap_psm_hid_cntl_channel.state = FAKEDEV_L2CAP_CHANNEL_STATE_INACTIVE;
			fakedev.l2cap_psm_hid_cntl_channel.local_cid = local_cid;
			fakedev.l2cap_psm_hid_cntl_channel.remote_cid = L2CAP_NULL_CID;
			fakedev.l2cap_psm_hid_cntl_channel.remote_mtu = 0;
			fakedev.l2cap_psm_hid_cntl_channel.valid = true;
			printf("L2CAP PSM HID CNTL connect req: %d\n", ret);
		}

		if (!fakedev.l2cap_psm_hid_intr_channel.valid) {
			u16 local_cid = generate_l2cap_channel_id();
			ret = l2cap_send_psm_connect_req(fakedev.hci_con_handle,
							 L2CAP_PSM_HID_INTR,
							 local_cid);
			assert(ret == IOS_OK);
			fakedev.l2cap_psm_hid_intr_channel.state = FAKEDEV_L2CAP_CHANNEL_STATE_INACTIVE;
			fakedev.l2cap_psm_hid_intr_channel.local_cid = local_cid;
			fakedev.l2cap_psm_hid_intr_channel.remote_cid = L2CAP_NULL_CID;
			fakedev.l2cap_psm_hid_intr_channel.remote_mtu = 0;
			fakedev.l2cap_psm_hid_intr_channel.valid = true;
			printf("L2CAP PSM HID INTR connect req: %d\n", ret);
		}

		if (is_l2cap_channel_complete(&fakedev.l2cap_psm_hid_cntl_channel) &&
		    is_l2cap_channel_complete(&fakedev.l2cap_psm_hid_intr_channel))
			fakedev.acl_state = FAKEDEV_ACL_STATE_INACTIVE;
	}
}

bool fakedev_handle_hci_cmd_accept_con(const bdaddr_t *bdaddr, u8 role)
{
	int ret;

	printf("fakedev_handle_hci_cmd_accept_con\n");

	/* Check if this bdaddr belongs to a fake device */
	if (memcmp(bdaddr, &fakedev.bdaddr, sizeof(bdaddr_t) != 0))
		return false;

	/* Connection accepted to our fake device */
	printf("Connection accepted for our fake device!\n");

	/* The Accept_Connection_Request command will cause the Command Status
	   event to be sent from the Host Controller when the Host Controller
	   begins setting up the connection */

	ret = enqueue_hci_event_command_status(HCI_CMD_ACCEPT_CON);
	assert(ret == IOS_OK);

	fakedev.baseband_state = FAKEDEV_BASEBAND_STATE_COMPLETE;
	fakedev.hci_con_handle = hci_con_handle_virt_alloc();
	printf("Our fake device got HCI con_handle: 0x%x\n", fakedev.hci_con_handle);

	/* We can start the ACL (L2CAP) linking now */
	fakedev.acl_state = FAKEDEV_ACL_STATE_LINKING;

	if (role == HCI_ROLE_MASTER) {
		ret = enqueue_hci_event_role_change(bdaddr, HCI_ROLE_MASTER);
		assert(ret == IOS_OK);
	}

	/* In addition, when the Link Manager determines the connection is established,
	 * the Host Controllers on both Bluetooth devices that form the connection
	 * will send a Connection Complete event to each Host */
	ret = enqueue_hci_event_con_compl(bdaddr, fakedev.hci_con_handle, 0);
	assert(ret == IOS_OK);

	printf("Connection complete sent, starting ACL linking!\n");

	return true;
}

bool fakedev_handle_hci_cmd_from_host(u16 hci_con_handle, const hci_cmd_hdr_t *hdr)
{
	/* TODO */
	printf("fakedev_handle_hci_cmd_from_host: 0x%x\n", hci_con_handle);

	/* Check if this HCI connection handle belongs to a fake device */
	if (!fakedev_is_connected() || (fakedev.hci_con_handle != hci_con_handle))
		return false;

	return true;
}

bool fakedev_handle_acl_data_out_request_from_host(u16 hci_con_handle, const hci_acldata_hdr_t *hdr)
{
	printf("fakedev_handle_acl_data_out_request_from_host: 0x%x\n", hci_con_handle);

	/* Check if this HCI connection handle belongs to a fake device */
	if (!fakedev_is_connected() || (fakedev.hci_con_handle != hci_con_handle))
		return false;

	/* L2CAP header */
	l2cap_hdr_t *l2cap_hdr = (void *)((u8 *)hdr + sizeof(hci_acldata_hdr_t));
	//u16 l2cap_length       = le16toh(l2cap_hdr->length);
	u16 l2cap_dcid         = le16toh(l2cap_hdr->dcid);
	void *l2cap_payload    = (void *)((u8 *)l2cap_hdr + sizeof(l2cap_hdr_t));

	//printf("< ACL OUT: hdl: 0x%x, len: 0x%x\n", acl_con_handle, acl_length);
	//printf("  L2CAP: len: 0x%x, dcid: 0x%x\n", l2cap_length, l2cap_dcid);

	if (l2cap_dcid == L2CAP_SIGNAL_CID) {
		l2cap_cmd_hdr_t *cmd_hdr = (void *)l2cap_payload;
		u16 cmd_hdr_length = le16toh(cmd_hdr->length);
		void *cmd_payload = (void *)((u8 *)l2cap_payload + sizeof(l2cap_cmd_hdr_t));

		printf("  L2CAP CMD: code: 0x%x, ident: 0x%x, length: 0x%x\n",
			cmd_hdr->code, cmd_hdr->ident, cmd_hdr_length);

		switch (cmd_hdr->code) {
		case L2CAP_CONNECT_REQ: {
			l2cap_con_req_cp *con_req = cmd_payload;
			u16 req_psm = le16toh(con_req->psm);
			u16 req_scid = le16toh(con_req->scid);
			printf("    L2CAP_CONNECT_REQ: psm: 0x%x, scid: 0x%x\n", req_psm, req_scid);

			/* Save PSM -> Remote CID mapping */
			//channel_map_t *chn = channels_get(con_req_scid);
			//assert(chn);
			//chn->psm = con_req_psm;
			break;
		}
		case L2CAP_CONNECT_RSP: {
			l2cap_con_rsp_cp *con_rsp = cmd_payload;
			u16 rsp_dcid = le16toh(con_rsp->dcid);
			u16 rsp_scid = le16toh(con_rsp->scid);
			u16 rsp_result = le16toh(con_rsp->result);
			u16 rsp_status = le16toh(con_rsp->status);

			printf("    L2CAP_CONNECT_RSP: dcid: 0x%x, scid: 0x%x, result: 0x%x, status: 0x%x\n",
				rsp_dcid, rsp_scid, rsp_result, rsp_status);

			assert(rsp_result == L2CAP_SUCCESS);
			assert(rsp_status == L2CAP_NO_INFO);
			//assert(DoesChannelExist(rsp->scid));

			/* libogc sets it to the "dcid" and not the "result" field (and scid to 0)... */
			if ((rsp_dcid == L2CAP_PSM_NOT_SUPPORTED) &&
			    (rsp_scid == 0)) {
				printf("FUKKUUUUUUU\n");
				//fakedev.acl_state = FAKEDEV_ACL_STATE_INACTIVE;
			}

			/* Save endpoint's Destination CID  */
			if (fakedev.l2cap_psm_sdp_channel.local_cid == rsp_scid) {
				assert(fakedev.l2cap_psm_sdp_channel.valid);
				fakedev.l2cap_psm_sdp_channel.remote_cid = rsp_dcid;
			} else if (fakedev.l2cap_psm_hid_cntl_channel.local_cid == rsp_scid) {
				assert(fakedev.l2cap_psm_hid_cntl_channel.valid);
				fakedev.l2cap_psm_hid_cntl_channel.remote_cid = rsp_dcid;
			} else if (fakedev.l2cap_psm_hid_intr_channel.local_cid == rsp_scid) {
				assert(fakedev.l2cap_psm_hid_intr_channel.valid);
				fakedev.l2cap_psm_hid_intr_channel.remote_cid = rsp_dcid;
			} else {
				assert(1);
			}
			break;
		}
		case L2CAP_CONFIG_REQ: {
			l2cap_cfg_req_cp *cfg_req = cmd_payload;
			u16 req_dcid = le16toh(cfg_req->dcid);
			u16 req_flags = le16toh(cfg_req->flags);

			printf("    L2CAP_CONFIG_REQ: dcid: 0x%x, flags: 0x%x\n", req_dcid, req_flags);
			break;
		}
		case L2CAP_CONFIG_RSP: {
			l2cap_cfg_rsp_cp *cfg_rsp = cmd_payload;
			u16 rsp_scid = le16toh(cfg_rsp->scid);
			u16 rsp_flags = le16toh(cfg_rsp->flags);
			u16 rsp_result = le16toh(cfg_rsp->result);

			assert(rsp_result == L2CAP_SUCCESS);

			printf("    L2CAP_CONFIG_RSP: scid: 0x%x, flags: 0x%x, result: 0x%x\n",
				rsp_scid, rsp_flags, rsp_result);

			/* Mark channel as complete!  */
			if (fakedev.l2cap_psm_sdp_channel.local_cid == rsp_scid) {
				fakedev.l2cap_psm_sdp_channel.state = FAKEDEV_L2CAP_CHANNEL_STATE_COMPLETE;
			} else if (fakedev.l2cap_psm_hid_cntl_channel.local_cid == rsp_scid) {
				fakedev.l2cap_psm_hid_cntl_channel.state = FAKEDEV_L2CAP_CHANNEL_STATE_COMPLETE;
			} else if (fakedev.l2cap_psm_hid_intr_channel.local_cid == rsp_scid) {

				fakedev.l2cap_psm_hid_intr_channel.state = FAKEDEV_L2CAP_CHANNEL_STATE_COMPLETE;
			} else {
				assert(1);
			}

			break;
		}
		}
	}

	return true;
}


bool fakedev_handle_acl_data_in_request_from_host(u16 hci_con_handle, const hci_acldata_hdr_t *hdr)
{
	printf("fakedev_handle_acl_data_in_request_from_host: 0x%x\n", hci_con_handle);

	/* Check if this HCI connection handle belongs to a fake device */
	if (!fakedev_is_connected() || (fakedev.hci_con_handle != hci_con_handle))
		return false;

	/* L2CAP header */
	l2cap_hdr_t *l2cap_hdr = (void *)((u8 *)hdr + sizeof(hci_acldata_hdr_t));
	//u16 l2cap_length       = le16toh(l2cap_hdr->length);
	u16 l2cap_dcid         = le16toh(l2cap_hdr->dcid);
	void *l2cap_payload    = (void *)((u8 *)l2cap_hdr + sizeof(l2cap_hdr_t));

	//printf("< ACL OUT: hdl: 0x%x, len: 0x%x\n", acl_con_handle, acl_length);
	//printf("  L2CAP: len: 0x%x, dcid: 0x%x\n", l2cap_length, l2cap_dcid);

	return true;
}
