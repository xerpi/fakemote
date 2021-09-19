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

#define MAX_CHANNEL_MAP	4
#define THREAD_STACK_SIZE 4096

typedef struct {
	bool active;
	u16 local_cid;
	u16 psm;
	u16 remote_cid;
} channel_map_t;

/* Global state */
char *moduleName = "TST";
static u8 thread_stack[THREAD_STACK_SIZE] ATTRIBUTE_ALIGN(32);
static int thread_id;
static int thread_initialized = 0;
static ipcmessage *new_msg_queue[8] ATTRIBUTE_ALIGN(32);
static int new_msg_queueid;
static int orig_msg_queueid;
static channel_map_t channels[MAX_CHANNEL_MAP];

/* HCI status */
static u8 hci_unit_class[HCI_CLASS_SIZE];
static int hci_page_scan_enabled = 0;
static int con_req_sent = 0;

static channel_map_t *channels_get(u16 local_cid)
{
	for (int i = 0; i < MAX_CHANNEL_MAP; i++) {
		if (channels[i].active && (channels[i].local_cid == local_cid))
			return &channels[i];
	}
	return NULL;
}

static bool channels_register(u16 local_cid, u16 remote_cid)
{
	for (int i = 0; i < MAX_CHANNEL_MAP; i++) {
		if (!channels[i].active) {
			channels[i].local_cid = local_cid;
			channels[i].remote_cid = remote_cid;
			channels[i].active = true;
			return true;
		}
	}
	return false;
}

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

static int handle_acl_data_in_l2cap(u16 acl_con_handle, u16 dcid, void *data, u32 size)
{
	//svc_printf("  l2cap  in: len: 0x%x, dcid: 0x%x, data: 0x%08x\n",
	//	size, dcid, *(u32 *)data);

	if (dcid == L2CAP_SIGNAL_CID) {
		return handle_l2cap_signal_channel_in(data, size);
	}

	return 0;

	channel_map_t *chn = channels_get(dcid);
	if (!chn)
		return 0;

	if (chn->psm == L2CAP_PSM_HID_CNTL)
		svc_printf("PSM HID CNTL\n");
	else if (chn->psm == L2CAP_PSM_HID_INTR)
		return handle_hid_intr(data, size);

	return 0;
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

static int handle_oh1_dev_bulk_message(u8 bEndpoint, u16 wLength, void *data)
{
	//svc_printf("BLK: bEndpoint 0x%x, wLength: 0x%x\n", bEndpoint, wLength);
	//svc_printf("  data: 0x%x 0x%x\n", *(u32 *)data, *(u32 *)(data + 4));

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

	if (bEndpoint == EP_ACL_DATA_OUT) {
		//svc_printf("< ACL OUT: hdl: 0x%x, len: 0x%x\n",
		//	   acl_con_handle, acl_length);

		/* This is the ACL datapath from CPU to Wiimote */
		return handle_acl_data_out_l2cap(acl_con_handle, l2cap_dcid,
						 l2cap_payload, l2cap_length);
	} else if (bEndpoint == EP_ACL_DATA_IN) {
	} else if (bEndpoint == EP_ACL_DATA_IN) {
		//svc_printf("> ACL  IN: hdl: 0x%x, len: 0x%x\n",
		//	   acl_con_handle, acl_length);

		/* We are given an ACL buffer to fill */
		return handle_acl_data_in_l2cap(acl_con_handle, l2cap_dcid,
						l2cap_payload, l2cap_length);
	}

	return 0;
}

static int handle_oh1_dev_control_message(u8 bmRequestType, u8 bRequest, u16 wValue,
					  u16 wIndex, u16 wLength, void *data)
{
	//if (kk++ < 5)
	//svc_printf("CTL: bmRequestType 0x%x, bRequest: 0x%x, "
	//	   "wValue: 0x%x, wIndex: 0x%x, wLength: 0x%x\n",
	//	   bmRequestType, bRequest, wValue, wIndex, wLength);
	//svc_printf("  data: 0x%x 0x%x\n", *(u32 *)data, *(u32 *)(data + 4));

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
		case HCI_CMD_ACCEPT_CON: {
			hci_accept_con_cp *cp = data + sizeof(hci_cmd_hdr_t);
			static const char *roles[] = {
				"Master (0x00)",
				"Slave (0x01)",
			};
			svc_printf("  HCI_CMD_ACCEPT_CON %02X:%02X:%02X:%02X:%02X:%02X, role: %s\n",
				cp->bdaddr.b[5], cp->bdaddr.b[4], cp->bdaddr.b[3],
				cp->bdaddr.b[2], cp->bdaddr.b[1], cp->bdaddr.b[0],
				roles[cp->role]);
			break;
		}
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
			break;
		}
		case HCI_CMD_WRITE_UNIT_CLASS: {
			hci_write_unit_class_cp *cp = data + sizeof(hci_cmd_hdr_t);
			svc_printf("  HCI_CMD_WRITE_UNIT_CLASS: 0x%x 0x%x 0x%x\n",
				cp->uclass[0], cp->uclass[1], cp->uclass[2]);
			hci_unit_class[0] = cp->uclass[0];
			hci_unit_class[1] = cp->uclass[1];
			hci_unit_class[2] = cp->uclass[2];
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

	return 0;
}

static int handle_oh1_dev_intr_message(u8 bEndpoint, u16 wLength, void *data, int *fwd_to_usb)
{
	//if (kk++ < 10)
	//svc_printf("INT: bEndpoint 0x%x, wLength: 0x%x\n", bEndpoint, wLength);
	//svc_printf("  data: 0x%x 0x%x\n", *(u32 *)data, *(u32 *)(data + 4));

	/* We are given a HCI buffer to fill */
	if (bEndpoint == EP_HCI_EVENT) {
		int class_match = (hci_unit_class[0] == WIIMOTE_HCI_CLASS_0) &&
				  (hci_unit_class[1] == WIIMOTE_HCI_CLASS_1) &&
				  (hci_unit_class[2] == WIIMOTE_HCI_CLASS_2);
		static int sync_pressed = 0;
#if 0
		if (class_match && !sync_pressed) {
			// When the red sync button is pressed, a HCI event is generated:
			//   > HCI Event: Vendor (0xff) plen 1
			//   08
			// This causes the emulated software to perform a BT inquiry and connect to found Wiimotes.
			// When the red sync button is held for 10 seconds, a HCI event with payload 09 is sent.
			hci_event_hdr_t *hdr = data;
			hdr->event = HCI_EVENT_VENDOR;
			hdr->length = sizeof(u8);
			u8 *payload = data + sizeof(hci_event_hdr_t);
			payload[0] = 0x08;
			svc_printf("Faking SYNC button press HCI event\n");
			*fwd_to_usb = 0;
			sync_pressed = 1;
			return sizeof(hci_event_hdr_t) + hdr->length;
		}
#endif
		/* If page scan is disabled the controller will not see this connection request. */
		if (class_match && hci_page_scan_enabled && !con_req_sent) {
			hci_event_hdr_t *hdr = data;
			hdr->event = HCI_EVENT_CON_REQ;
			hdr->length = sizeof(hci_con_req_ep);
			hci_con_req_ep *ep = data + sizeof(hci_event_hdr_t);
			// 00:21:BD:2D:57:FF Name: Nintendo RVL-CNT-01
			/*ep->bdaddr.b[0] = 0x00;
			ep->bdaddr.b[1] = 0x21;
			ep->bdaddr.b[2] = 0xBD;
			ep->bdaddr.b[3] = 0x2D;
			ep->bdaddr.b[4] = 0x57;
			ep->bdaddr.b[5] = 0xFF;*/
			ep->bdaddr.b[5] = 0x00;
			ep->bdaddr.b[4] = 0x11;
			ep->bdaddr.b[3] = 0x22;
			ep->bdaddr.b[2] = 0x33;
			ep->bdaddr.b[1] = 0x44;
			ep->bdaddr.b[0] = 0x55;
			ep->uclass[0] = WIIMOTE_HCI_CLASS_0;
			ep->uclass[1] = WIIMOTE_HCI_CLASS_1;
			ep->uclass[2] = WIIMOTE_HCI_CLASS_2;
			ep->link_type = HCI_LINK_ACL;

			svc_printf("HCI EVENT CON REQ SENT\n");
			*fwd_to_usb = 0;
			con_req_sent = 1;
			return sizeof(hci_event_hdr_t) + hdr->length;
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
		ret = handle_oh1_dev_control_message(bmRequestType, bRequest, wValue, wIndex, wLength, data);
		*fwd_to_usb = 1;
		break;
	}
	case USBV0_IOCTLV_BLKMSG: {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		ret = handle_oh1_dev_bulk_message(bEndpoint, wLength, data);
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

	while (1) {
		ipcmessage *message;

		/* Wait for message */
		ret = os_message_queue_receive(orig_msg_queueid, (void *)&message, 0);
		if (ret != 0) {
			svc_printf("Msg queue recv err: %d\n", ret);
			break;
		} else if (message == (ipcmessage *)0xcafef00d) {
			continue;
		}

		handle_oh1_dev_message(message);
	}

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

	/* Redirect OH1 module to read messages from our new queue */
	if (queueid == orig_msg_queueid)
		ret = os_message_queue_receive(new_msg_queueid, message, flags);
	else
		ret = os_message_queue_receive(queueid, message, flags);

	return ret;
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

	/* Init global state */
	for (int i = 0; i < MAX_CHANNEL_MAP; i++)
		channels[i].active = false;

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
