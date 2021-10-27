#include "bt_device_mgr.h"
#include "l2cap.h"
#include "utils.h"

static void handle_l2cap_signal_channel(u8 code, u8 ident, const void *payload, u16 size)
{
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

		break;
	}
	case L2CAP_CONFIG_REQ: {
		const l2cap_cfg_req_cp *rsp = payload;
		u16 dcid = le16toh(rsp->dcid);
		u16 flags = le16toh(rsp->flags);
		const void *options = (const void *)((u8 *)rsp + sizeof(l2cap_cfg_req_cp));
		UNUSED(flags);

		DEBUG("  L2CAP_CONFIG_REQ: dcid: 0x%x, flags: 0x%x\n", dcid, flags);

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

		break;
	}
	case L2CAP_DISCONNECT_REQ: {
		const l2cap_discon_req_cp *req = payload;
		u16 dcid = le16toh(req->dcid);
		u16 scid = le16toh(req->scid);
		DEBUG("  L2CAP_DISCONNECT_REQ: dcid: 0x%x, scid: 0x%x\n", dcid, scid);
		break;
	}
	}
}

bool bt_device_mgr_handle_acl_data_in_response_from_controller(u16 hci_con_handle, const void *data)
{
	const l2cap_hdr_t *header;
	u16 dcid, length;
	const u8 *payload;

	/* L2CAP header */
	header  = (const void *)((u8 *)data + sizeof(hci_acldata_hdr_t));
	length  = le16toh(header->length);
	dcid    = le16toh(header->dcid);
	payload = (u8 *)header + sizeof(l2cap_hdr_t);

	DEBUG("  L2CAP: dcid: 0x%x, len: 0x%x\n", dcid, length);

	if (dcid == L2CAP_SIGNAL_CID) {
		const l2cap_cmd_hdr_t *cmd_hdr;
		const void *cmd_payload;
		u16 cmd_len;

		while (length >= sizeof(l2cap_cmd_hdr_t)) {
			cmd_hdr = (const void *)payload;
			cmd_len = le16toh(cmd_hdr->length);
			cmd_payload = (const void *)((u8 *)payload + sizeof(*cmd_hdr));

			handle_l2cap_signal_channel(cmd_hdr->code, cmd_hdr->ident,
						    cmd_payload, cmd_len);

			payload += sizeof(l2cap_cmd_hdr_t) + cmd_len;
			length -= sizeof(l2cap_cmd_hdr_t) + cmd_len;
		}
	}

	return true;
}
