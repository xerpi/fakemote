/*
	Custom IOS Module (USB)

	Copyright (C) 2008 neimod.
	Copyright (C) 2009 WiiGator.
	Copyright (C) 2009 Waninkoko.
	Copyright (C) 2011 davebaol.
	Copyright (C) 2020 Leseratte.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "conf.h"
#include "ipc.h"
#include "mem.h"
#include "syscalls.h"
#include "vsprintf.h"
#include "tools.h"
#include "types.h"
#include "hci.h"
#include "l2cap.h"

#define bswap16 __builtin_bswap16
#define printf svc_printf
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

/* OH1 module hook information */
#define OH1_SYSCALL_RECEIVE_MESSAGE_ADDR1 0x138b365c
#define OH1_SYSCALL_RECEIVE_MESSAGE_ADDR2 0x138b366c
#define OH1_DEV_OH1_QUEUEID_ADDR          0x138b5004

#define USBV0_IOCTLV_CTRLMSG		 0
#define USBV0_IOCTLV_BLKMSG		 1
#define USBV0_IOCTLV_INTRMSG		 2
#define USBV0_IOCTL_SUSPENDDEV		 5
#define USBV0_IOCTL_RESUMEDEV		 6
#define USBV0_IOCTLV_ISOMSG		 9
#define USBV0_IOCTLV_LBLKMSG		10
#define USBV0_IOCTLV_GETDEVLIST		12
#define USBV0_IOCTL_GETRHDESCA		15
#define USBV0_IOCTLV_GETRHPORTSTATUS	20
#define USBV0_IOCTLV_SETRHPORTSTATUS	25
#define USBV0_IOCTL_DEVREMOVALHOOK	26
#define USBV0_IOCTLV_DEVINSERTHOOK	27
#define USBV0_IOCTLV_DEVICECLASSCHANGE	28
#define USBV0_IOCTL_RESET_DEVICE	29
#define USBV0_IOCTLV_DEVINSERTHOOKID	30
#define USBV0_IOCTL_CANCEL_INSERT_HOOK	31
#define USBV0_IOCTLV_UNKNOWN_32		32

/* Wiimote definitions */
#define WIIMOTE_HCI_CLASS_0 0x00
#define WIIMOTE_HCI_CLASS_1 0x04
#define WIIMOTE_HCI_CLASS_2 0x48

#define EP_HCI_CTRL	0x00
#define EP_HCI_EVENT	0x81
#define EP_ACL_DATA_IN	0x82
#define EP_ACL_DATA_OUT	0x02

/* Private definitions */

#define MAX_CHANNEL_MAP	32
#define THREAD_STACK_SIZE 4096

typedef struct {
	bool active;
	u16 psm;
	u16 local_cid;
	u16 remote_cid;
} channel_map_t;

/* Heap for dynamic memory */
static u8 heapspace[0x1000] ATTRIBUTE_ALIGN(32);

/* Global state */
char *moduleName = "TST";
static u8 thread_stack[THREAD_STACK_SIZE] ATTRIBUTE_ALIGN(32);
static int thread_id;
static int thread_initialized = 0;
static ipcmessage *new_msg_queue[8] ATTRIBUTE_ALIGN(32);
static int new_msg_queueid;
static int orig_msg_queueid;

/* List of pending HCI events to deliver to the BT SW stack (USB INTR) */
static void *pending_hci_event_queue[8] ATTRIBUTE_ALIGN(32);
static int pending_hci_event_queueid;

/* List of pending ACL (L2CAP) command to deliver to the BT SW stack (USB BULK IN) */
static void *pending_acl_in_event_queue[8] ATTRIBUTE_ALIGN(32);
static int pending_acl_in_event_queueid;

/* Fake devices */
// 00:21:BD:2D:57:FF Name: Nintendo RVL-CNT-01
static bdaddr_t fake_dev_baddr = {.b = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}};
static int con_req_sent = 0;
static int fake_sync_pressed_sent = 1; // Disable for now
static channel_map_t channels[MAX_CHANNEL_MAP];

/* HCI snooped status (requested by SW BT stack) */
static u8 hci_unit_class[HCI_CLASS_SIZE];
static int hci_page_scan_enabled = 0;

#define CON_HANDLE (0x100 + 1)

static u16 generate_l2cap_channel_id(void)
{
	// "Identifiers from 0x0001 to 0x003F are reserved"
	static u16 starting_id = 0x40;
	return starting_id++;
}

static channel_map_t *channels_get(u16 local_cid)
{
	for (int i = 0; i < MAX_CHANNEL_MAP; i++) {
		if (channels[i].active && (channels[i].local_cid == local_cid))
			return &channels[i];
	}
	return NULL;
}

static channel_map_t *channels_get_free_slot(void)
{
	for (int i = 0; i < MAX_CHANNEL_MAP; i++) {
		if (!channels[i].active) {
			channels[i].active = true;
			return &channels[i];
		}
	}
	return NULL;
}

/* L2CAP helpers */

static int enqueue_acl_in_command(bdaddr_t baddr, const void *data, u16 size)
{
	int ret;
	u16 con_handle = CON_HANDLE; // TODO: Get from baddr

	hci_acldata_hdr_t *hdr = Mem_Alloc(sizeof(hci_acldata_hdr_t) + size);
	if (!hdr)
		return IOS_ENOMEM;

	hdr->con_handle = HCI_MK_CON_HANDLE(con_handle, HCI_PACKET_START, HCI_POINT2POINT);
	hdr->length = size;
	memcpy((u8 *)hdr + sizeof(hci_acldata_hdr_t), data, size);

	ret = os_message_queue_send(pending_acl_in_event_queueid, hdr, IOS_MESSAGE_NOBLOCK);
	if (ret) {
		Mem_Free(hdr);
		return ret;
	}

	return 0;
}

static int l2cap_send_command_to_acl(u8 ident, u8 code, u8 command_length, const void *command_data)
{
	int ret;

	l2cap_hdr_t *hdr = Mem_Alloc(sizeof(l2cap_hdr_t) + sizeof(l2cap_cmd_hdr_t) + command_length);
	if (!hdr)
		return IOS_ENOMEM;

	hdr->length = sizeof(l2cap_cmd_hdr_t) + command_length;
	hdr->dcid = L2CAP_SIGNAL_CID;

	l2cap_cmd_hdr_t *cmd = (void *)hdr + sizeof(l2cap_hdr_t);
	cmd->code = code;
	cmd->ident = ident;
	cmd->length = command_length;

	memcpy((void *)cmd + sizeof(l2cap_cmd_hdr_t), command_data, command_length);

	ret = enqueue_acl_in_command(fake_dev_baddr, hdr, sizeof(l2cap_hdr_t) + hdr->length);

	Mem_Free(hdr);

	return ret;
}

static int l2cap_send_psm_connect_req(u16 psm)
{
	channel_map_t *chn = channels_get_free_slot();
	if (!chn)
		return IOS_ENOMEM;

	chn->psm = psm;
	chn->local_cid = generate_l2cap_channel_id();

	l2cap_con_req_cp cr;
	cr.psm = psm;
	cr.scid = chn->local_cid;

	return l2cap_send_command_to_acl(L2CAP_CONNECT_REQ, L2CAP_CONNECT_REQ, sizeof(cr), &cr);
}

/* HCI "pending event" queue enqueue helper */
static int enqueue_hci_event_command_status(u16 opcode)
{
	int ret;
	u16 size = sizeof(hci_event_hdr_t) + sizeof(hci_command_status_ep);
	hci_event_hdr_t *hdr;
	hci_command_status_ep *stat;

	hdr = Mem_Alloc(size);
	if (!hdr)
		return IOS_ENOMEM;

	/* Fill event data */
	stat = (void *)hdr + sizeof(hci_event_hdr_t);
	hdr->event = HCI_EVENT_COMMAND_STATUS;
	hdr->length = sizeof(hci_command_status_ep);
	stat->status = 0;
	stat->num_cmd_pkts = 1;
	stat->opcode = bswap16(opcode);

	ret = os_message_queue_send(pending_hci_event_queueid, hdr, IOS_MESSAGE_NOBLOCK);
	if (ret) {
		Mem_Free(hdr);
		return ret;
	}

	return 0;
}

static int enqueue_hci_event_con_req(bdaddr_t bdaddr)
{
	int ret;
	u16 size = sizeof(hci_event_hdr_t) + sizeof(hci_con_req_ep);
	hci_event_hdr_t *hdr;
	hci_con_req_ep *con;

	hdr = Mem_Alloc(size);
	if (!hdr)
		return IOS_ENOMEM;

	/* Fill event data */
	con = (void *)hdr + sizeof(hci_event_hdr_t);
	hdr->event = HCI_EVENT_CON_REQ;
	hdr->length = sizeof(hci_con_req_ep);
	con->bdaddr = bdaddr;
	con->uclass[0] = WIIMOTE_HCI_CLASS_0;
	con->uclass[1] = WIIMOTE_HCI_CLASS_1;
	con->uclass[2] = WIIMOTE_HCI_CLASS_2;
	con->link_type = HCI_LINK_ACL;

	ret = os_message_queue_send(pending_hci_event_queueid, hdr, IOS_MESSAGE_NOBLOCK);
	if (ret) {
		Mem_Free(hdr);
		return ret;
	}

	return 0;
}

static int enqueue_hci_event_con_compl(bdaddr_t bdaddr, u8 status)
{
	int ret;
	u16 size = sizeof(hci_event_hdr_t) + sizeof(hci_con_compl_ep);
	hci_event_hdr_t *hdr;
	hci_con_compl_ep *con;

	hdr = Mem_Alloc(size);
	if (!hdr)
		return IOS_ENOMEM;

	/* Fill event data */
	con = (void *)hdr + sizeof(hci_event_hdr_t);
	hdr->event = HCI_EVENT_CON_COMPL;
	hdr->length = sizeof(hci_con_compl_ep);
	con->status = status;
	con->con_handle = bswap16(CON_HANDLE); // TODO: assign bdaddr to con handle
	con->bdaddr = bdaddr;
	con->link_type = HCI_LINK_ACL;
	con->encryption_mode = HCI_ENCRYPTION_MODE_NONE;

	ret = os_message_queue_send(pending_hci_event_queueid, hdr, IOS_MESSAGE_NOBLOCK);
	if (ret) {
		Mem_Free(hdr);
		return ret;
	}

	return 0;
}

static int enqueue_hci_event_role_change(bdaddr_t bdaddr, u8 role)
{
	int ret;
	u16 size = sizeof(hci_event_hdr_t) + sizeof(hci_role_change_ep);
	hci_event_hdr_t *hdr;
	hci_role_change_ep *chg;

	hdr = Mem_Alloc(size);
	if (!hdr)
		return IOS_ENOMEM;

	/* Fill event data */
	chg = (void *)hdr + sizeof(hci_event_hdr_t);
	hdr->event = HCI_EVENT_ROLE_CHANGE;
	hdr->length = sizeof(hci_role_change_ep);
	chg->status = 0;
	chg->bdaddr = bdaddr;
	chg->role = role;

	ret = os_message_queue_send(pending_hci_event_queueid, hdr, IOS_MESSAGE_NOBLOCK);
	if (ret) {
		Mem_Free(hdr);
		return ret;
	}

	return 0;
}

/* Handlers */

static int handle_l2cap_signal_channel_in(void *data, u32 size)
{
	l2cap_cmd_hdr_t *cmd_hdr = data;
	u16 cmd_hdr_length = bswap16(cmd_hdr->length);

	//svc_printf("  l2cap_signal_channel_in: code: 0x%x, ident: 0x%x, length: 0x%x\n",
	//	cmd_hdr->code, cmd_hdr->ident, cmd_hdr_length);

	switch (cmd_hdr->code) {
	case L2CAP_CONNECT_RSP: {
		l2cap_con_rsp_cp *con_rsp = data + sizeof(l2cap_cmd_hdr_t);
		u16 con_rsp_dcid = bswap16(con_rsp->dcid);
		u16 con_rsp_scid = bswap16(con_rsp->scid);
		u16 con_rsp_result = bswap16(con_rsp->result);
		u16 con_rsp_status = bswap16(con_rsp->status);

		//printf("\n    L2CAP_CONNECT_RSP: dcid: 0x%x, scid: 0x%x, result: 0x%x, status: 0x%x\n",
		//	con_rsp_dcid, con_rsp_scid, con_rsp_result, con_rsp_status);

		/* Register new channel */
		//channels_register(con_rsp_scid, con_rsp_dcid);
		break;
	}
	}

	return 0;
}

static int handle_l2cap_signal_channel_out(void *data, u32 size)
{
	l2cap_cmd_hdr_t *cmd_hdr = data;
	u16 cmd_hdr_length = bswap16(cmd_hdr->length);

	//svc_printf("  l2cap_signal_channel out: code: 0x%x, ident: 0x%x, length: 0x%x\n",
	//	cmd_hdr->code, cmd_hdr->ident, cmd_hdr_length);

	switch (cmd_hdr->code) {
	case L2CAP_CONNECT_REQ: {
		l2cap_con_req_cp *con_req = data + sizeof(l2cap_cmd_hdr_t);
		u16 con_req_psm = bswap16(con_req->psm);
		u16 con_req_scid = bswap16(con_req->scid);
		//printf("\n    L2CAP_CONNECT_REQ: psm: 0x%x, scid: 0x%x\n", con_req_psm, con_req_scid);

		/* Save PSM -> Remote CID mapping */
		//channel_map_t *chn = channels_get(con_req_scid);
		//assert(chn);
		//chn->psm = con_req_psm;
		break;
	}
	case L2CAP_CONFIG_REQ: {
		l2cap_cfg_req_cp *cfg_req = data + sizeof(l2cap_cmd_hdr_t);
		u16 cfq_req_dcid = bswap16(cfg_req->dcid);
		u16 cfq_req_flags = bswap16(cfg_req->flags);

		//printf("    L2CAP_CONFIG_REQ: dcid: 0x%x, flags: 0x%x\n", cfq_req_dcid, cfq_req_flags);
		break;
	}
	}

	return 0;
}

static int handle_hid_intr(void *data, u32 size)
{
	//svc_printf("PSM HID INTR, size: %d, data: 0x%x\n", size, *(u32 *)data);
	return 0;
}

static int handle_acl_data_in_l2cap(void *data, u32 size, int *fwd_to_usb)
{
	int ret;
	svc_printf("  l2cap  in:");

	//if (kk++ < 10)
	//svc_printf("INT: bEndpoint 0x%x, wLength: 0x%x\n", bEndpoint, wLength);
	//svc_printf("  data: 0x%x 0x%x\n", *(u32 *)data, *(u32 *)(data + 4));

	hci_acldata_hdr_t *hdr;
	ret = os_message_queue_receive(pending_acl_in_event_queueid, &hdr,
				       IOS_MESSAGE_NOBLOCK);
	if (ret == 0) {
		u16 length = sizeof(hci_acldata_hdr_t) + hdr->length;
		svc_printf("We have a pending ACL DATA_IN event, size: %d!\n", length);
		memcpy(data, hdr, length);
		Mem_Free(hdr);
		*fwd_to_usb = 0;
		return length;
	}

	*fwd_to_usb = 1;

	return 0;

	/* channel_map_t *chn = channels_get(dcid);
	if (!chn)
		return 0;

	if (chn->psm == L2CAP_PSM_HID_CNTL)
		svc_printf("PSM HID CNTL\n");
	else if (chn->psm == L2CAP_PSM_HID_INTR)
		return handle_hid_intr(data, size);*/
}

static int handle_acl_data_out_l2cap(u16 acl_con_handle, u16 dcid, void *data, u32 size)
{
	//svc_printf("  l2cap out: len: 0x%x, dcid: 0x%x, data: 0x%08x\n",
	//	size, dcid, *(u32 *)data);

	if (dcid == L2CAP_SIGNAL_CID) {
		return handle_l2cap_signal_channel_out(data, size);
	}

	return 0;
}

static int handle_oh1_dev_bulk_message(u8 bEndpoint, u16 wLength, void *data, int *fwd_to_usb)
{
	svc_printf("BLK: bEndpoint 0x%x, wLength: 0x%x\n", bEndpoint, wLength);
	//svc_printf("  data: 0x%x 0x%x\n", *(u32 *)data, *(u32 *)(data + 4));

	if (bEndpoint == EP_ACL_DATA_OUT) {
		/* HCI ACL data packet header */
		hci_acldata_hdr_t *acl_hdr = data;
		u16 acl_con_handle = HCI_CON_HANDLE(bswap16(acl_hdr->con_handle));
		u16 acl_length     = bswap16(acl_hdr->length);
		u8 *acl_payload    = data + sizeof(hci_acldata_hdr_t);

		/* L2CAP header */
		l2cap_hdr_t *l2cap_hdr = (void *)acl_payload;
		u16 l2cap_length  = bswap16(l2cap_hdr->length);
		u16 l2cap_dcid    = bswap16(l2cap_hdr->dcid);
		u8 *l2cap_payload = acl_payload + sizeof(l2cap_hdr_t);

		//svc_printf("< ACL OUT: hdl: 0x%x, len: 0x%x\n",
		//	   acl_con_handle, acl_length);

		/* This is the ACL datapath from CPU to device (Wiimote) */
		return handle_acl_data_out_l2cap(acl_con_handle, l2cap_dcid,
						 l2cap_payload, l2cap_length);
	} else if (bEndpoint == EP_ACL_DATA_IN) {
		svc_printf("> ACL  IN: len: 0x%x\n", wLength);

		/* We are given an ACL buffer to fill */
		return handle_acl_data_in_l2cap(data, wLength, fwd_to_usb);
	}

	return 0;
}

static int handle_hci_cmd_accept_con(void *data, int *fwd_to_usb)
{
	int ret;
	hci_accept_con_cp *cp = data + sizeof(hci_cmd_hdr_t);

	static const char *roles[] = {
		"Master (0x00)",
		"Slave (0x01)",
	};

	svc_printf("  HCI_CMD_ACCEPT_CON %02X:%02X:%02X:%02X:%02X:%02X, role: %s\n",
		cp->bdaddr.b[5], cp->bdaddr.b[4], cp->bdaddr.b[3],
		cp->bdaddr.b[2], cp->bdaddr.b[1], cp->bdaddr.b[0],
		roles[cp->role]);

	/* Connection accepted to our fake device */
	if (con_req_sent && memcmp(&cp->bdaddr, &fake_dev_baddr, sizeof(bdaddr_t)) == 0) {
		svc_printf("Accepted CON for our fake device!\n");

		/* The Accept_Connection_Request command will cause the Command Status
		   event to be sent from the Host Controller when the Host Controller
		   begins setting up the connection */
		ret = enqueue_hci_event_command_status(HCI_CMD_ACCEPT_CON);
		if (ret)
			return ret;

		if (cp->role == HCI_ROLE_MASTER) {
			ret = enqueue_hci_event_role_change(cp->bdaddr, HCI_ROLE_MASTER);
			if (ret)
				return ret;
		}

		/* In addition, when the Link Manager determines the connection is established,
		 *  the Host Controllers on both Bluetooth devices that form the connection
		 *  will send a Connection Complete event to each Host */
		ret = enqueue_hci_event_con_compl(cp->bdaddr, 0);
		if (ret)
			return ret;

		/* The connection originated from the (fake) device, now we can start creating
		 * the ACL L2CAP HID control and interrupt channels (in that order). */
		svc_printf("L2CAP STARTING\n");
		ret = l2cap_send_psm_connect_req(L2CAP_PSM_HID_CNTL);
		svc_printf("L2CAP PSM HID connect req: %d\n", ret);
		ret = l2cap_send_psm_connect_req(L2CAP_PSM_HID_INTR);
		svc_printf("L2CAP PSM INTR connect req: %d\n", ret);

		*fwd_to_usb = 0;
	}

	return 0;
}

static int hci_check_send_connection_request(int *fwd_to_usb)
{
	int ret;
	int class_match = (hci_unit_class[0] == WIIMOTE_HCI_CLASS_0) &&
			  (hci_unit_class[1] == WIIMOTE_HCI_CLASS_1) &&
			  (hci_unit_class[2] == WIIMOTE_HCI_CLASS_2);

	/* If page scan is disabled the controller will not see this connection request. */
	if (class_match && hci_page_scan_enabled && !con_req_sent) {
		ret = enqueue_hci_event_con_req(fake_dev_baddr);
		if (ret)
			return ret;
		con_req_sent = 1;
		*fwd_to_usb = 0;
	}

	return 0;
}

static int handle_oh1_dev_control_message(u8 bmRequestType, u8 bRequest, u16 wValue,
					  u16 wIndex, u16 wLength, void *data, int *fwd_to_usb)
{
	int ret = 0;

	//if (kk++ < 5)
	//svc_printf("CTL: bmRequestType 0x%x, bRequest: 0x%x, "
	//	   "wValue: 0x%x, wIndex: 0x%x, wLength: 0x%x\n",
	//	   bmRequestType, bRequest, wValue, wIndex, wLength);
	//svc_printf("  data: 0x%x 0x%x\n", *(u32 *)data, *(u32 *)(data + 4));

	/* Default to yes (few command return fake data to BT SW stack) */
	*fwd_to_usb = 1;

	if (bRequest == EP_HCI_CTRL) {
		hci_cmd_hdr_t *cmd_hdr = data;
		u16 opcode = bswap16(cmd_hdr->opcode);
		u16 ocf = HCI_OCF(opcode);
		u16 ogf = HCI_OGF(opcode);

		//if (kk++ < 30)
		//svc_printf("EP_HCI_CTRL: opcode: 0x%x (ocf: 0x%x, ogf: 0x%x)\n", opcode, ocf, ogf);

		switch (opcode) {
		case HCI_CMD_CREATE_CON:
			svc_write("  HCI_CMD_CREATE_CON\n");
			break;
		case HCI_CMD_ACCEPT_CON:
			ret = handle_hci_cmd_accept_con(data, fwd_to_usb);
			break;
		case HCI_CMD_DISCONNECT:
			svc_write("  HCI_CMD_DISCONNECT\n");
			break;
		case HCI_CMD_INQUIRY:
			svc_write("  HCI_CMD_INQUIRY\n");
			break;
		case HCI_CMD_WRITE_SCAN_ENABLE: {
			static const char *scanning[] = {
				"HCI_NO_SCAN_ENABLE",
				"HCI_INQUIRY_SCAN_ENABLE",
				"HCI_PAGE_SCAN_ENABLE",
				"HCI_INQUIRY_AND_PAGE_SCAN_ENABLE",
			};
			hci_write_scan_enable_cp *cp = data + sizeof(hci_cmd_hdr_t);
			svc_printf("  HCI_CMD_WRITE_SCAN_ENABLE: 0x%x (%s)\n",
				cp->scan_enable, scanning[cp->scan_enable]);
			hci_page_scan_enabled = cp->scan_enable & HCI_PAGE_SCAN_ENABLE;
			ret = hci_check_send_connection_request(fwd_to_usb);
			break;
		}
		case HCI_CMD_WRITE_UNIT_CLASS: {
			hci_write_unit_class_cp *cp = data + sizeof(hci_cmd_hdr_t);
			svc_printf("  HCI_CMD_WRITE_UNIT_CLASS: 0x%x 0x%x 0x%x\n",
				cp->uclass[0], cp->uclass[1], cp->uclass[2]);
			hci_unit_class[0] = cp->uclass[0];
			hci_unit_class[1] = cp->uclass[1];
			hci_unit_class[2] = cp->uclass[2];
			ret = hci_check_send_connection_request(fwd_to_usb);
			break;
		}
		case HCI_CMD_WRITE_INQUIRY_SCAN_TYPE: {
			hci_write_inquiry_scan_type_cp *cp = data + sizeof(hci_cmd_hdr_t);
			svc_printf("  HCI_CMD_WRITE_INQUIRY_SCAN_TYPE: 0x%x\n", cp->type);
			break;
		}
		case HCI_CMD_WRITE_PAGE_SCAN_TYPE: {
			hci_write_page_scan_type_cp *cp = data + sizeof(hci_cmd_hdr_t);
			svc_printf("  HCI_CMD_WRITE_PAGE_SCAN_TYPE: 0x%x\n", cp->type);
			break;
		}
		case HCI_CMD_WRITE_INQUIRY_MODE: {
			hci_write_inquiry_mode_cp *cp = data + sizeof(hci_cmd_hdr_t);
			svc_printf("  HCI_CMD_WRITE_INQUIRY_MODE: 0x%x\n", cp->mode);
			break;
		}
		case HCI_CMD_SET_EVENT_FILTER: {
			hci_set_event_filter_cp *cp = data + sizeof(hci_cmd_hdr_t);
			svc_printf("  HCI_CMD_SET_EVENT_FILTER: 0x%x 0x%x\n",
				  cp->filter_type, cp->filter_condition_type);
			break;
		}
		default:
			//svc_printf("HCI CTRL: opcode: 0x%x (ocf: 0x%x, ogf: 0x%x)\n", opcode, ocf, ogf);
			break;
		}
	}

	return ret;
}

static int handle_oh1_dev_intr_message(u8 bEndpoint, u16 wLength, void *data, int *fwd_to_usb)
{
	int ret;
	//if (kk++ < 10)
	//svc_printf("INT: bEndpoint 0x%x, wLength: 0x%x\n", bEndpoint, wLength);
	//svc_printf("  data: 0x%x 0x%x\n", *(u32 *)data, *(u32 *)(data + 4));

	/* We are given a HCI buffer to fill */
	if (bEndpoint == EP_HCI_EVENT) {
		hci_event_hdr_t *event;
		ret = os_message_queue_receive(pending_hci_event_queueid, &event,
					       IOS_MESSAGE_NOBLOCK);
		if (ret == 0) {
			u16 length = sizeof(hci_event_hdr_t) + event->length;
			svc_printf("We have a pending HCI event, size: %d!\n", length);
			memcpy(data, event, length);
			Mem_Free(event);
			*fwd_to_usb = 0;
			return length;
		}
	}

	*fwd_to_usb = 1;

	return 0;
}

static int handle_oh1_dev_ioctlv(u32 cmd, ioctlv *vector, u32 inlen, u32 iolen, int *fwd_to_usb)
{
	int ret = 0;
	//svc_printf("  ioctlv: cmd 0x%x\n", cmd);
	//svc_printf("  ioctlv: num_in %d\n", inlen);
	//svc_printf("  ioctlv: num_io %d\n", iolen);
	//svc_write("\n");

	/* Invalidate cache */
	InvalidateVector(vector, inlen, iolen);

	switch (cmd) {
	case USBV0_IOCTLV_CTRLMSG: {
		u8 bmRequestType = *(u8 *)vector[0].data;
		u8 bRequest      = *(u8 *)vector[1].data;
		u16 wValue       = bswap16(*(u16 *)vector[2].data);
		u16 wIndex       = bswap16(*(u16 *)vector[3].data);
		u16 wLength      = bswap16(*(u16 *)vector[4].data);
		// u16 wUnk      = *(u8 *)vector[5].data;
		void *data       = vector[6].data;
		ret = handle_oh1_dev_control_message(bmRequestType, bRequest, wValue, wIndex,
						     wLength, data, fwd_to_usb);
		*fwd_to_usb = 1;
		break;
	}
	case USBV0_IOCTLV_BLKMSG: {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		ret = handle_oh1_dev_bulk_message(bEndpoint, wLength, data, fwd_to_usb);
		*fwd_to_usb = 1;
		break;
	}
	case USBV0_IOCTLV_INTRMSG: {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		ret = handle_oh1_dev_intr_message(bEndpoint, wLength, data, fwd_to_usb);
		break;
	}
	default:
		/* Forward unhandled/unknown ioctls to the OH1 module */
		svc_printf("Unhandled IOCTL: 0x%x\n", cmd);
		*fwd_to_usb = 1;
		break;
	}

	/* Flush cache */
	if (!fwd_to_usb)
		FlushVector(vector, inlen, iolen);

	return ret;
}

static void handle_oh1_dev_message(ipcmessage *message)
{
	int ret = 0;
	int fwd_to_usb = 1;

	//svc_printf("- OH1 msg cmd 0x%x, fd: %d\n", message->command, message->fd);

	switch (message->command) {
	case IOS_OPEN:
		break;
	case IOS_CLOSE:
		break;
	case IOS_READ:
		break;
	case IOS_WRITE:
		break;
	case IOS_SEEK:
		break;
	case IOS_IOCTLV: {
		ioctlv *vector = message->ioctlv.vector;
		u32     inlen  = message->ioctlv.num_in;
		u32     iolen  = message->ioctlv.num_io;
		u32     cmd    = message->ioctlv.command;
		ret = handle_oh1_dev_ioctlv(cmd, vector, inlen, iolen, &fwd_to_usb);
		break;
	}
	default:
		/* Unknown command */
		svc_printf("Unhandled IPC command: 0x%x\n", message->command);
		break;
	}

	/* Acknowledge message or send it to the original OH1 module */
	if (fwd_to_usb)
		os_message_queue_send(new_msg_queueid, (void *)message, 0);
	else {
		svc_printf("ACK: %d!\n", ret);
		os_message_queue_ack(message, ret);
	}
}

static int worker_thread(void *)
{
	int ret;
	svc_printf("Work thread start\n");

	/* Initialize memory heap */
	ret = Mem_Init(heapspace, sizeof(heapspace));
	if (ret < 0)
		return ret;

	ret = os_message_queue_create(pending_hci_event_queue,
				      ARRAY_SIZE(pending_hci_event_queue));
	if (ret < 0)
		return ret;
	pending_hci_event_queueid = ret;

	ret = os_message_queue_create(pending_acl_in_event_queue,
				      ARRAY_SIZE(pending_acl_in_event_queue));
	if (ret < 0)
		return ret;
	pending_acl_in_event_queueid = ret;

	/* Init global state */
	for (int i = 0; i < MAX_CHANNEL_MAP; i++)
		channels[i].active = false;

	while (1) {
		ipcmessage *message;

		/* Wait for message */
		ret = os_message_queue_receive(orig_msg_queueid, (void *)&message,
					       IOS_MESSAGE_BLOCK);
		if (ret != 0) {
			svc_printf("Msg queue recv err: %d\n", ret);
			break;
		} else if (message == (ipcmessage *)0xcafef00d) {
			continue;
		}

		handle_oh1_dev_message(message);
	}

	os_message_queue_destroy(pending_hci_event_queueid);
	os_message_queue_destroy(pending_acl_in_event_queueid);
	os_message_queue_destroy(new_msg_queueid);

	return 0;
}

static int OH1_IOS_ReceiveMessage_hook(int queueid, void *message, u32 flags)
{
	int ret;

	if (!thread_initialized) {
		/* Create a new queue to hijack OH1 module's queue */
		new_msg_queueid = os_message_queue_create(new_msg_queue,
							  ARRAY_SIZE(new_msg_queue));


		/* Create the thread that will handle requests to OH1 device */
		thread_id = os_thread_create((void *)worker_thread, NULL,
					     &thread_stack[THREAD_STACK_SIZE],
					     THREAD_STACK_SIZE, os_thread_get_priority(0)-1, 0);
		/* Resume thread */
		if (thread_id >= 0)
			os_thread_continue(thread_id);

		thread_initialized = 1;
	}

	/* Redirect BT SW stack to read messages from our new queue instead of OH1 BT dongle */
	if (queueid == orig_msg_queueid)
		queueid = new_msg_queueid;

	return os_message_queue_receive(queueid, message, flags);
}

static s32 Patch_OH1UsbModule(void)
{
	u32 addr;

	/* Check version */
	u32 bytes = *(u16 *)OH1_SYSCALL_RECEIVE_MESSAGE_ADDR1;
	if (bytes == 0x4778)
		addr = OH1_SYSCALL_RECEIVE_MESSAGE_ADDR1;
	else if (bytes == 0xbd00)
		addr = OH1_SYSCALL_RECEIVE_MESSAGE_ADDR2;
	else
		return -1;

	/* Get original /dev/usb/oh1 queueid */
	orig_msg_queueid = *(int *)OH1_DEV_OH1_QUEUEID_ADDR;

	/* Patch syscall handler to jump to our function */
	DCWrite32(addr    , 0x4B004718);
	DCWrite32(addr + 4, (u32)OH1_IOS_ReceiveMessage_hook);

	return 0;
}

static u8 conf_buffer[0x4000] ATTRIBUTE_ALIGN(32);
static struct conf_pads_setting conf_pads ATTRIBUTE_ALIGN(32);

int main(void)
{
	int ret;

	/* Print info */
	svc_write("Hello world from Starlet!\n");
#if 0
	int fd = os_open("/shared2/sys/SYSCONF", IPC_OPEN_READ);
	svc_printf("os_open(): %d\n", fd);

	ret = os_read(fd, conf_buffer, sizeof(conf_buffer));
	os_close(fd);

	ret = conf_get(conf_buffer, "BT.DINF", &conf_pads, sizeof(conf_pads));
	svc_printf("conf_get(): %d\n", ret);
	//for (int i = 0; i < 20; i+=4)
	//	svc_printf("  foo: 0x%x\n", *(u32*)(((u8*)&conf_pads) + i));
	svc_printf("  foo: 0x%x\n", *(u32*)((u8*)&conf_pads + 0));
	svc_printf("  foo: 0x%x\n", *(u32*)((u8*)&conf_pads + 4));
	//svc_printf("  foo: 0x%x\n", *(u32*)((u8*)&conf_pads + 8));
	svc_printf("  num_registered: %d\n", conf_pads.num_registered);
	svc_printf("  registered[0]: %s\n", conf_pads.registered[0].name);
	svc_printf("  active[0]: %s\n", conf_pads.active[0].name);
#endif
	/* System patchers */
	patcher patchers[] = {
		{Patch_OH1UsbModule, 0},
	};

	/* Initialize plugin */
	ret = IOS_InitSystem(patchers, sizeof(patchers));
	svc_printf("IOS_InitSystem(): %d\n", ret);

	return 0;
}

void __assert_func(const char *file, int line, const char *func, const char *failedexpr)
{
	svc_printf("assertion \"%s\" failed: file \"%s\", line %d%s%s\n",
		failedexpr, file, line, func ? ", function: " : "", func ? func : "");
	abort();
}

void abort()
{
	svc_write("\n\n**** ABORTED ****\n\n");
	while (1)
		os_thread_yield();
}
