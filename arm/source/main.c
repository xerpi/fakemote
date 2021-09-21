#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "conf.h"
#include "ipc.h"
#include "hci_state.h"
#include "mem.h"
#include "syscalls.h"
#include "vsprintf.h"
#include "tools.h"
#include "types.h"
#include "hci.h"
#include "l2cap.h"
#include "utils.h"

/* OH1 module hook information */
#define OH1_IOS_ReceiveMessage_ADDR1 0x138b365c
#define OH1_IOS_ResourceReply_ADDR1  0x138b36f4
#define OH1_IOS_ReceiveMessage_ADDR2 0x138b366c
#define OH1_IOS_ResourceReply_ADDR2  0x138b3704
#define OH1_DEV_OH1_QUEUEID_ADDR     0x138b5004

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

#define PERIODC_TIMER_PERIOD	500 * 1000
#define MAX_CHANNEL_MAP		32
#define THREAD_STACK_SIZE	4096

typedef struct {
	bool active;
	u16 psm;
	u16 local_cid;
	u16 remote_cid;
} channel_map_t;

/* Heap for dynamic memory */
static u8 heapspace[4 * 1024] ATTRIBUTE_ALIGN(32);

/* Global state */
char *moduleName = "TST";
static int orig_msg_queueid;
static int periodic_timer_id;
static int periodic_timer_cookie __attribute__((used));

static u8 usb_intr_msg_handed_to_oh1_ioctlv_0_data ATTRIBUTE_ALIGN(32) = EP_HCI_EVENT;
static u16 usb_intr_msg_handed_to_oh1_ioctlv_1_data ATTRIBUTE_ALIGN(32);
static ioctlv usb_intr_msg_handed_to_oh1_ioctlvs[3] = {
	{&usb_intr_msg_handed_to_oh1_ioctlv_0_data, sizeof(usb_intr_msg_handed_to_oh1_ioctlv_0_data)},
	{&usb_intr_msg_handed_to_oh1_ioctlv_1_data, sizeof(usb_intr_msg_handed_to_oh1_ioctlv_1_data)},
	{NULL, 0} /* Filled dynamically */
};
static ipcmessage usb_intr_msg_handed_to_oh1 ATTRIBUTE_ALIGN(32) = {
	.command = IOS_IOCTLV,
	.result = IOS_OK,
	.fd = 0, /* Filled dynamically */
	.ioctlv = {
		.command = USBV0_IOCTLV_INTRMSG,
		.num_in = 2,
		.num_io = 1,
		.vector = usb_intr_msg_handed_to_oh1_ioctlvs
	}
};

static u8 usb_bulk_in_msg_handed_to_oh1_ioctlv_0_data ATTRIBUTE_ALIGN(32) = EP_ACL_DATA_IN;
static u16 usb_bulk_in_msg_handed_to_oh1_ioctlv_1_data ATTRIBUTE_ALIGN(32);
static ioctlv usb_bulk_in_msg_handed_to_oh1_ioctlvs[3] = {
	{&usb_bulk_in_msg_handed_to_oh1_ioctlv_0_data, sizeof(usb_bulk_in_msg_handed_to_oh1_ioctlv_0_data)},
	{&usb_bulk_in_msg_handed_to_oh1_ioctlv_1_data, sizeof(usb_bulk_in_msg_handed_to_oh1_ioctlv_1_data)},
	{NULL, 0} /* Filled dynamically */
};
static ipcmessage usb_bulk_in_msg_handed_to_oh1 ATTRIBUTE_ALIGN(32) = {
	.command = IOS_IOCTLV,
	.result = IOS_OK,
	.fd = 0, /* Filled dynamically */
	.ioctlv = {
		.command = USBV0_IOCTLV_BLKMSG,
		.num_in = 2,
		.num_io = 1,
		.vector = usb_bulk_in_msg_handed_to_oh1_ioctlvs
	}
};

static ipcmessage *ready_usb_intr_msg_queue_data[8] ATTRIBUTE_ALIGN(32);
static int ready_usb_intr_msg_queue_id;
static ipcmessage *pending_usb_intr_msg_queue_data[8] ATTRIBUTE_ALIGN(32);
static int pending_usb_intr_msg_queue_id;

static ipcmessage *ready_usb_bulk_in_msg_queue_data[8] ATTRIBUTE_ALIGN(32);
static int ready_usb_bulk_in_msg_queue_id;
static ipcmessage *pending_usb_bulk_in_msg_queue_data[8] ATTRIBUTE_ALIGN(32);
static int pending_usb_bulk_in_msg_queue_id;

/* Fake devices */
// 00:21:BD:2D:57:FF Name: Nintendo RVL-CNT-01

typedef enum {
	FAKEDEV_BASEBAND_STATE_INACTIVE,
	FAKEDEV_BASEBAND_STATE_REQUEST_CONNECTION,
	FAKEDEV_BASEBAND_STATE_COMPLETE
} fakedev_baseband_state_e;

typedef enum {
	FAKEDEV_ACL_STATE_INACTIVE,
	FAKEDEV_ACL_STATE_LINKING,
	FAKEDEV_ACL_STATE_LINKED
} fakedev_acl_state_e;

typedef enum {
	FAKEDEV_FOO
} fakedev_hci_connection_status_e;

typedef struct {
	bdaddr_t bdaddr;
	//u16 hci_con_handle;
	fakedev_baseband_state_e baseband_state;
	fakedev_acl_state_e acl_state;
} fakedev_t;

static fakedev_t fakedev = {
	.bdaddr = {.b = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}},
	.baseband_state = FAKEDEV_BASEBAND_STATE_INACTIVE,
	.acl_state = FAKEDEV_ACL_STATE_INACTIVE,
};
static int fake_sync_pressed_sent = 1; // Disable for now
static channel_map_t channels[MAX_CHANNEL_MAP];

#define CON_HANDLE (0x100 + 1)

/* Function prototypes */

static int ensure_init_oh1_context(void);
static int handle_bulk_message_to_oh1_dev(ipcmessage *recv_msg, ipcmessage **ret_msg, u8 bEndpoint,
				       u16 wLength, void *data, int *fwd_to_usb);
static int handle_bulk_intr_pending_message(ipcmessage *recv_msg, ipcmessage **ret_msg,
						  u16 recv_msg_len, int ready_queue_id,
						  int pending_queue_id, ipcmessage *handed_down_msg,
						  int *fwd_to_usb);
static int handle_bulk_intr_ready_message(ipcmessage *ready_msg, int retval, int pending_queue_id,
					  int ready_queue_id);

/* Channel bookkeeping */

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

/* Fakedev */
#if 0
static int hci_check_send_connection_request(int *fwd_to_usb)
{
	int ret;
	int class_match = (hci_unit_class[0] == WIIMOTE_HCI_CLASS_0) &&
			  (hci_unit_class[1] == WIIMOTE_HCI_CLASS_1) &&
			  (hci_unit_class[2] == WIIMOTE_HCI_CLASS_2);

	/* If page scan is disabled the controller will not see this connection request. */
	if (!class_match || !(hci_page_scan_enable & HCI_PAGE_SCAN_ENABLE))
		return 0;

	/* If we want to request a connection, now it's a good time */
	if (fakedev.baseband_state == FAKEDEV_BASEBAND_STATE_REQUEST_CONNECTION) {
		ret = enqueue_hci_event_con_req(fakedev.bdaddr);
		if (ret != IOS_OK)
			return ret;
		fakedev.baseband_state = FAKEDEV_BASEBAND_STATE_INACTIVE;
	}

	return 0;
}
#endif

static void fakedev_tick_devices(void)
{
	//svc_printf("fakedev_tick_devices\n");
}

//static bool fakedev_is_bdaddr_registered(bdaddr_t *)

/* L2CAP helpers */

static ipcmessage *alloc_hci_acl_msg(void **acl_payload, bdaddr_t baddr, u16 acl_payload_size, u16 *total_size)
{
	ipcmessage *msg;
	hci_acldata_hdr_t *hdr;
	u16 payload_size = sizeof(hci_acldata_hdr_t) + acl_payload_size;
	u16 con_handle = CON_HANDLE; // TODO: Get from baddr

	*total_size = sizeof(ipcmessage) + 3 * sizeof(ioctlv) + payload_size;

	msg = Mem_Alloc(*total_size);
	if (!msg)
		return NULL;

	msg->ioctlv.vector = (void *)((u8 *)msg + sizeof(ipcmessage));
	msg->ioctlv.vector[2].data = (void *)((u8 *)msg->ioctlv.vector + 3 * sizeof(ioctlv));
	msg->ioctlv.vector[2].len = payload_size;
	hdr = msg->ioctlv.vector[2].data;
	hdr->con_handle = htole16(HCI_MK_CON_HANDLE(con_handle, HCI_PACKET_START, HCI_POINT2POINT));
	hdr->length = htole16(acl_payload_size);
	*acl_payload = (u8 *)hdr + sizeof(hci_acldata_hdr_t);

	return msg;
}

#if 0
static int enqueue_hci_event_con_req(bdaddr_t bdaddr)
{
	ipcmessage *msg;
	hci_event_hdr_t *hdr;
	hci_con_req_ep *con;
	u16 total_size;

	msg = alloc_hci_event_msg(&hdr, sizeof(hci_con_req_ep), &total_size);
	if (!msg)
		return IOS_ENOMEM;

	/* Fill event data */
	hdr->event = HCI_EVENT_CON_REQ;
	hdr->length = sizeof(hci_con_req_ep);
	con = (void *)((u8 *)hdr + sizeof(hci_event_hdr_t));
	con->bdaddr = bdaddr;
	con->uclass[0] = WIIMOTE_HCI_CLASS_0;
	con->uclass[1] = WIIMOTE_HCI_CLASS_1;
	con->uclass[2] = WIIMOTE_HCI_CLASS_2;
	con->link_type = HCI_LINK_ACL;

	return handle_bulk_intr_ready_message(msg, total_size, pending_usb_intr_msg_queue_id,
					      ready_usb_intr_msg_queue_id);
}
#endif

static ipcmessage *alloc_l2cap_msg(void **l2cap_payload, bdaddr_t baddr, u16 dcid, u16 size, u16 *total_size)
{
	ipcmessage *msg;
	l2cap_hdr_t *hdr;

	msg = alloc_hci_acl_msg((void **)&hdr, baddr, sizeof(l2cap_hdr_t) + size, total_size);
	if (!msg)
		return NULL;

	hdr->length = htole16(size);
	hdr->dcid = htole16(dcid);
	*l2cap_payload = (u8 *)hdr + sizeof(l2cap_hdr_t);

	return msg;
}

static ipcmessage *alloc_l2cap_cmd_msg(void **l2cap_cmd_payload, bdaddr_t baddr, u8 code, u8 ident,
				       u16 size, u16 *total_size)
{
	int ret;
	ipcmessage *msg;
	l2cap_cmd_hdr_t *hdr;

	msg = alloc_l2cap_msg((void **)&hdr, baddr, L2CAP_SIGNAL_CID,
			      sizeof(l2cap_cmd_hdr_t) + size, total_size);
	if (!msg)
		return NULL;

	hdr->code = code;
	hdr->ident = ident;
	hdr->length = htole16(size);
	*l2cap_cmd_payload = (u8 *)hdr + sizeof(l2cap_cmd_hdr_t);

	return msg;
}

static int l2cap_send_psm_connect_req(bdaddr_t bdaddr, u16 psm)
{
	ipcmessage *msg;
	l2cap_con_req_cp *cr;
	channel_map_t *chn;
	u16 total_size;

	chn = channels_get_free_slot();
	if (!chn)
		return IOS_ENOMEM;
	chn->psm = psm;
	chn->local_cid = generate_l2cap_channel_id();

	msg = alloc_l2cap_cmd_msg((void **)&cr, bdaddr, L2CAP_CONNECT_REQ, L2CAP_CONNECT_REQ,
				  sizeof(cr), &total_size);
	if (!msg) {
		chn->active = false;
		return IOS_ENOMEM;
	}
	cr->psm = htole16(psm);
	cr->scid = htole16(chn->local_cid);

	return handle_bulk_intr_ready_message(msg, total_size, pending_usb_bulk_in_msg_queue_id,
					      ready_usb_bulk_in_msg_queue_id);

}

/* HCI event enqueue helpers */

static ipcmessage *alloc_hci_event_msg(hci_event_hdr_t **hdr, u16 event_size, u16 *total_size)
{
	ipcmessage *msg;
	u16 payload_size = sizeof(hci_event_hdr_t) + event_size;
	*total_size = sizeof(ipcmessage) + 3 * sizeof(ioctlv) + payload_size;

	msg = Mem_Alloc(*total_size);
	if (!msg)
		return NULL;

	msg->ioctlv.vector = (void *)((u8 *)msg + sizeof(ipcmessage));
	msg->ioctlv.vector[2].data = (void *)((u8 *)msg->ioctlv.vector + 3 * sizeof(ioctlv));
	msg->ioctlv.vector[2].len = payload_size;
	*hdr = msg->ioctlv.vector[2].data;

	return msg;
}

static int enqueue_hci_event_con_req(bdaddr_t bdaddr)
{
	ipcmessage *msg;
	hci_event_hdr_t *hdr;
	hci_con_req_ep *con;
	u16 total_size;

	msg = alloc_hci_event_msg(&hdr, sizeof(hci_con_req_ep), &total_size);
	if (!msg)
		return IOS_ENOMEM;

	/* Fill event data */
	hdr->event = HCI_EVENT_CON_REQ;
	hdr->length = sizeof(hci_con_req_ep);
	con = (void *)((u8 *)hdr + sizeof(hci_event_hdr_t));
	con->bdaddr = bdaddr;
	con->uclass[0] = WIIMOTE_HCI_CLASS_0;
	con->uclass[1] = WIIMOTE_HCI_CLASS_1;
	con->uclass[2] = WIIMOTE_HCI_CLASS_2;
	con->link_type = HCI_LINK_ACL;

	return handle_bulk_intr_ready_message(msg, total_size, pending_usb_intr_msg_queue_id,
					      ready_usb_intr_msg_queue_id);
}

static int enqueue_hci_event_command_status(u16 opcode)
{
	ipcmessage *msg;
	hci_event_hdr_t *hdr;
	hci_command_status_ep *stat;
	u16 total_size;

	msg = alloc_hci_event_msg(&hdr, sizeof(hci_con_req_ep), &total_size);
	if (!msg)
		return IOS_ENOMEM;

	/* Fill event data */
	stat = (void *)hdr + sizeof(hci_event_hdr_t);
	hdr->event = HCI_EVENT_COMMAND_STATUS;
	hdr->length = sizeof(hci_command_status_ep);
	stat->status = 0;
	stat->num_cmd_pkts = 1;
	stat->opcode = htole16(opcode);

	return handle_bulk_intr_ready_message(msg, total_size, pending_usb_intr_msg_queue_id,
					      ready_usb_intr_msg_queue_id);
}

static int enqueue_hci_event_con_compl(bdaddr_t bdaddr, u8 status)
{
	ipcmessage *msg;
	hci_event_hdr_t *hdr;
	hci_con_compl_ep *compl;
	u16 total_size;

	msg = alloc_hci_event_msg(&hdr, sizeof(hci_con_compl_ep), &total_size);
	if (!msg)
		return IOS_ENOMEM;

	/* Fill event data */
	compl = (void *)hdr + sizeof(hci_event_hdr_t);
	hdr->event = HCI_EVENT_CON_COMPL;
	hdr->length = sizeof(hci_con_compl_ep);
	compl->status = status;
	compl->con_handle = htole16(CON_HANDLE); // TODO: assign bdaddr to compl handle
	compl->bdaddr = bdaddr;
	compl->link_type = HCI_LINK_ACL;
	compl->encryption_mode = HCI_ENCRYPTION_MODE_NONE;

	return handle_bulk_intr_ready_message(msg, total_size, pending_usb_intr_msg_queue_id,
					      ready_usb_intr_msg_queue_id);
}

static int enqueue_hci_event_role_change(bdaddr_t bdaddr, u8 role)
{
	ipcmessage *msg;
	hci_event_hdr_t *hdr;
	hci_role_change_ep *chg;
	u16 total_size;

	msg = alloc_hci_event_msg(&hdr, sizeof(hci_role_change_ep), &total_size);
	if (!msg)
		return IOS_ENOMEM;

	/* Fill event data */
	chg = (void *)hdr + sizeof(hci_event_hdr_t);
	hdr->event = HCI_EVENT_ROLE_CHANGE;
	hdr->length = sizeof(hci_role_change_ep);
	chg->status = 0;
	chg->bdaddr = bdaddr;
	chg->role = role;

	return handle_bulk_intr_ready_message(msg, total_size, pending_usb_intr_msg_queue_id,
					      ready_usb_intr_msg_queue_id);
}

void handle_message_reply_from_oh1_dev(ipcmessage *msg, int retval)
{
	ioctlv *vector = msg->ioctlv.vector;

	//svc_printf("handle_message_reply_from_oh1_dev %p, %d\n", msg, retval);

	if (msg->command != IOS_IOCTLV)
		return;

	assert(msg->ioctlv.num_in == 2);
	assert(msg->ioctlv.num_io == 1);

	if (msg->ioctlv.command == USBV0_IOCTLV_BLKMSG) {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		if (bEndpoint == EP_ACL_DATA_IN)
			hci_state_handle_acl_data_in(data, wLength);
	} else if (msg->ioctlv.command == USBV0_IOCTLV_INTRMSG) {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		if (bEndpoint == EP_HCI_EVENT)
			hci_state_handle_hci_event(data, wLength);
	}
}

static int handle_oh1_dev_ioctlv(ipcmessage *recv_msg, ipcmessage **ret_msg, u32 cmd,
				 ioctlv *vector, u32 inlen, u32 iolen, int *fwd_to_usb)
{
	int ret = 0;

	/* Invalidate cache */
	InvalidateVector(vector, inlen, iolen);

	switch (cmd) {
	case USBV0_IOCTLV_CTRLMSG: {
		u8 bRequest      = *(u8 *)vector[1].data;
		u16 wLength      = le16toh(*(u16 *)vector[4].data);
		void *data       = vector[6].data;
		/* Ignore non-HCI control messages... */
		if (bRequest == EP_HCI_CTRL)
			ret = hci_state_handle_hci_command(data, wLength, fwd_to_usb);
		break;
	}
	case USBV0_IOCTLV_BLKMSG: {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		if (bEndpoint == EP_ACL_DATA_OUT) {
			/* This is the ACL datapath from CPU to device (Wiimote) */
			ret = hci_state_handle_acl_data_out(data, wLength, fwd_to_usb);
		} else if (bEndpoint == EP_ACL_DATA_IN) {
			/* We are given an ACL buffer to fill */
			ret = handle_bulk_intr_pending_message(recv_msg, ret_msg, wLength,
							       ready_usb_bulk_in_msg_queue_id,
							       pending_usb_bulk_in_msg_queue_id,
							       &usb_bulk_in_msg_handed_to_oh1,
							       fwd_to_usb);
		}
		break;
	}
	case USBV0_IOCTLV_INTRMSG: {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		if (bEndpoint == EP_HCI_EVENT) {
			/* We are given a HCI buffer to fill */
			ret = handle_bulk_intr_pending_message(recv_msg, ret_msg, wLength,
							       ready_usb_intr_msg_queue_id,
							       pending_usb_intr_msg_queue_id,
							       &usb_intr_msg_handed_to_oh1,
							       fwd_to_usb);
		}
		break;
	}
	default:
		/* Unhandled/unknown ioctls are forwarded to the OH1 module */
		svc_printf("Unhandled IOCTL: 0x%x\n", cmd);
		break;
	}

	return ret;
}

/* PendingQ / ReadyQ helpers */

static int configure_usb_bulk_intr_msg_handed_to_oh1(ipcmessage *handed_down_msg, u32 fd,
						     u16 min_wLength)
{
	u16 *p_wLength = handed_down_msg->ioctlv.vector[1].data;
	u32 *p_len = &handed_down_msg->ioctlv.vector[2].len;
	void **p_data = &handed_down_msg->ioctlv.vector[2].data;

	/* If the message has no data allocated, or it's smaller than the required... */
	if (*p_data == NULL || *p_wLength < min_wLength) {
		if (*p_data != NULL)
			Mem_Free(*p_data);

		*p_data = Mem_Alloc(min_wLength);
		if (*p_data == NULL)
			return IOS_ENOMEM;
		*p_wLength = min_wLength;
		*p_len = *p_wLength;
	}

	handed_down_msg->fd = fd;

	os_sync_before_read(*p_data, *p_len);

	return IOS_OK;
}

static inline void copy_ipcmessage_bulk_intr_ioctlv_data(ipcmessage *dst, const ipcmessage *src, int len)
{
	void *src_data = src->ioctlv.vector[2].data;
	void *dst_data = dst->ioctlv.vector[2].data;

	memcpy(dst_data, src_data, len);
	os_sync_after_write(dst_data, len);
}

static int handle_bulk_intr_pending_message(ipcmessage *recv_msg, ipcmessage **ret_msg,
					    u16 recv_msg_len, int ready_queue_id,
					    int pending_queue_id, ipcmessage *handed_down_msg,
					    int *fwd_to_usb)
{
	int ret;
	int is_ours;
	ipcmessage *ready_msg;

	/* Fast-path: check if we already have a message ready to be delivered */
	ret = os_message_queue_receive(ready_queue_id, &ready_msg, IOS_MESSAGE_NOBLOCK);
	if (ret == IOS_OK) {
		/* Check if it's a message we injected ourselves */
		is_ours = ((uintptr_t)ready_msg >= (uintptr_t)heapspace &&
		           (uintptr_t)ready_msg < ((uintptr_t)heapspace + sizeof(heapspace)));
		/* Copy message data */
		copy_ipcmessage_bulk_intr_ioctlv_data(recv_msg, ready_msg, ready_msg->result);
		/* Call the message ack handler if we didn't inject it, before ACKing it */
		if (!is_ours)
			handle_message_reply_from_oh1_dev(recv_msg, ready_msg->result);
		/* Finally, we can ACK the message! */
		ret = os_message_queue_ack(recv_msg, ready_msg->result);
		assert(ret == IOS_OK);
		/* If it was a message we injected ourselves, we have to deallocate it */
		if (is_ours)
			Mem_Free(ready_msg);
		/* Receive another message from the /dev resource without returning from the hook */
		*fwd_to_usb = 0;
	} else {
		/* Hand down to OH1 a copy of the message to fill it from the real USB data */
		ret = configure_usb_bulk_intr_msg_handed_to_oh1(handed_down_msg, recv_msg->fd,
								recv_msg_len);
		if (ret == IOS_OK) {
			*ret_msg = handed_down_msg;
			/* Push the received message to the PendingQ */
			ret = os_message_queue_send(pending_queue_id,
						    recv_msg, IOS_MESSAGE_NOBLOCK);
		}
	}

	return ret;
}

static int handle_bulk_intr_ready_message(ipcmessage *ready_msg, int retval, int pending_queue_id,
					  int ready_queue_id)
{
	int ret;
	int is_ours;
	ipcmessage *pending_msg;

	/* Fast-path: check if we have a PendingQ message to fill */
	ret = os_message_queue_receive(pending_queue_id, &pending_msg, IOS_MESSAGE_NOBLOCK);
	if (ret == IOS_OK) {
		/* Check if it's a message we injected ourselves */
		is_ours = ((uintptr_t)ready_msg >= (uintptr_t)heapspace &&
		           (uintptr_t)ready_msg < ((uintptr_t)heapspace + sizeof(heapspace)));
		/* Copy message data */
		copy_ipcmessage_bulk_intr_ioctlv_data(pending_msg, ready_msg, retval);
		/* Call the message ack handler if we didn't inject it, before ACKing it */
		if (!is_ours)
			handle_message_reply_from_oh1_dev(pending_msg, retval);
		/* Finally, we can ACK the message! */
		ret = os_message_queue_ack(pending_msg, retval);
		assert(ret == IOS_OK);
		/* If it was a message we injected ourselves, we have to deallocate it */
		if (is_ours)
			Mem_Free(ready_msg);
	} else {
		/* Push message to ReadyQ.
		 * We store the return value/size to the "result" field */
		ready_msg->result = retval;
		ret = os_message_queue_send(ready_queue_id, ready_msg, IOS_MESSAGE_NOBLOCK);
	}

	return ret;
}

/* Hooked functions */

static int OH1_IOS_ReceiveMessage_hook(int queueid, ipcmessage **ret_msg, u32 flags)
{
	int ret;
	ipcmessage *recv_msg;
	int fwd_to_usb;

	/* We don't care about other queues... */
	if (queueid != orig_msg_queueid)
		return os_message_queue_receive(queueid, (void *)ret_msg, flags);

	ensure_init_oh1_context();

	while (1) {
		ret = os_message_queue_receive(queueid, (void *)&recv_msg, flags);
		if (ret != IOS_OK) {
			svc_printf("Msg queue recv err: %d\n", ret);
			break;
		} else if (recv_msg == (ipcmessage *)0xcafef00d) {
			*ret_msg = (ipcmessage *)0xcafef00d;
			break;
		} else if (recv_msg == (void *)&periodic_timer_cookie) {
			fakedev_tick_devices();
			fwd_to_usb = 0;
		} else {

			*ret_msg = NULL;
			/* Default to yes (few commands return fake data to BT SW stack) */
			fwd_to_usb = 1;

			switch (recv_msg->command) {
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
				ioctlv *vector = recv_msg->ioctlv.vector;
				u32     inlen  = recv_msg->ioctlv.num_in;
				u32     iolen  = recv_msg->ioctlv.num_io;
				u32     cmd    = recv_msg->ioctlv.command;
				ret = handle_oh1_dev_ioctlv(recv_msg, ret_msg, cmd, vector,
							    inlen, iolen, &fwd_to_usb);
				break;
			}
			default:
				/* Unknown command */
				svc_printf("Unhandled IPC command: 0x%x\n", recv_msg->command);
				break;
			}
		}

		/* Break the loop and return from the hook if we want
		 * to deliver the message to the BT USB dongle */
		if (fwd_to_usb) {
			/* Just send the original message we received if we don't
			 * want to hand down a "private" message to OH1 */
			if (*ret_msg == NULL)
				*ret_msg = recv_msg;
			break;
		}
	}

	return ret;
}

static int OH1_IOS_ResourceReply_hook(ipcmessage *ready_msg, int retval)
{
	int ret;

	/* Only consider a message "ready" (valid) if it contains data (retval > 0) */
	if (retval >= 0) {
		if (ready_msg == &usb_intr_msg_handed_to_oh1) {
			ensure_init_oh1_context();
			ret = handle_bulk_intr_ready_message(ready_msg, retval,
							     pending_usb_intr_msg_queue_id,
							     ready_usb_intr_msg_queue_id);
			return ret;
		} else if (ready_msg == &usb_bulk_in_msg_handed_to_oh1) {
			ensure_init_oh1_context();
			ret = handle_bulk_intr_ready_message(ready_msg, retval,
							     pending_usb_bulk_in_msg_queue_id,
							     ready_usb_bulk_in_msg_queue_id);
			return ret;
		}
	}

	return os_message_queue_ack(ready_msg, retval);
}

static int ensure_init_oh1_context(void)
{
	static int initialized = 0;
	int ret;

	if (!initialized) {
		/* Message queues can only be used on the process they were created in */
		ret = os_message_queue_create(ready_usb_intr_msg_queue_data,
					      ARRAY_SIZE(ready_usb_intr_msg_queue_data));
		if (ret < 0)
			return ret;
		ready_usb_intr_msg_queue_id = ret;

		ret = os_message_queue_create(pending_usb_intr_msg_queue_data,
					      ARRAY_SIZE(pending_usb_intr_msg_queue_data));
		if (ret < 0)
			return ret;
		pending_usb_intr_msg_queue_id = ret;

		ret = os_message_queue_create(ready_usb_bulk_in_msg_queue_data,
					      ARRAY_SIZE(ready_usb_bulk_in_msg_queue_data));
		if (ret < 0)
			return ret;
		ready_usb_bulk_in_msg_queue_id = ret;

		ret = os_message_queue_create(pending_usb_bulk_in_msg_queue_data,
					      ARRAY_SIZE(pending_usb_bulk_in_msg_queue_data));
		if (ret < 0)
			return ret;
		pending_usb_bulk_in_msg_queue_id = ret;

		periodic_timer_id = os_create_timer(PERIODC_TIMER_PERIOD, PERIODC_TIMER_PERIOD,
						    orig_msg_queueid, (u32)&periodic_timer_cookie);
		if (ret < 0)
			return ret;

		initialized = 1;
	}

	return 0;
}

static s32 Patch_OH1UsbModule(void)
{
	u32 addr_recv;
	u32 addr_reply;

	/* Check version */
	u32 bytes = *(u16 *)OH1_IOS_ReceiveMessage_ADDR1;
	if (bytes == 0x4778) {
		addr_recv = OH1_IOS_ReceiveMessage_ADDR1;
		addr_reply = OH1_IOS_ResourceReply_ADDR1;
	} else if (bytes == 0xbd00) {
		addr_recv = OH1_IOS_ReceiveMessage_ADDR2;
		addr_reply = OH1_IOS_ResourceReply_ADDR2;
	} else {
		return IOS_ENOENT;
	}

	/* Get original /dev/usb/oh1 queueid */
	orig_msg_queueid = *(int *)OH1_DEV_OH1_QUEUEID_ADDR;

	/* Patch IOS_ReceiveMessage syscall wrapper to jump to our function */
	DCWrite32(addr_recv, 0x4B004718);
	DCWrite32(addr_recv + 4, (u32)OH1_IOS_ReceiveMessage_hook);

	/* Patch IOS_ResourceReply syscall wrapper to jump to our function */
	DCWrite32(addr_reply, 0x4B004718);
	DCWrite32(addr_reply + 4, (u32)OH1_IOS_ResourceReply_hook);

	return 0;
}

int main(void)
{
	int ret;

	/* Print info */
	svc_write("Hello world from Starlet!\n");

	/* Initialize memory heap */
	ret = Mem_Init(heapspace, sizeof(heapspace));
	if (ret < 0)
		return ret;

	/* Initialize global state */
	hci_state_init();

	for (int i = 0; i < ARRAY_SIZE(channels); i++)
		channels[i].active = false;

	fakedev.baseband_state = FAKEDEV_BASEBAND_STATE_REQUEST_CONNECTION;
/*
	static u8 conf_buffer[0x4000] ATTRIBUTE_ALIGN(32);
	static struct conf_pads_setting conf_pads ATTRIBUTE_ALIGN(32);

	int fd = os_open("/shared2/sys/SYSCONF", IOS_OPEN_RW);
	os_read(fd, conf_buffer, sizeof(conf_buffer));

	ret = conf_get(conf_buffer, "BT.DINF", &conf_pads, sizeof(conf_pads));
	svc_printf("conf_get(): %d\n", ret);
	svc_printf("  num_registered: %d\n", conf_pads.num_registered);
	svc_printf("  registered[0]: %s\n", conf_pads.registered[0].name);
	svc_printf("  registered[1]: %s\n", conf_pads.registered[1].name);

	for (int i = 0; i < 6; i++)
		conf_pads.registered[1].bdaddr[i] = fakedev.bdaddr.b[5 - i];
	conf_pads.num_registered = 2;
	strcpy(conf_pads.registered[1].name, "UwU");
	conf_set(conf_buffer, "BT.DINF", &conf_pads, sizeof(conf_pads));
	os_write(fd, conf_buffer, sizeof(conf_buffer));
	os_close(fd);
*/
	/* System patchers */
	patcher patchers[] = {
		{Patch_OH1UsbModule, 0},
	};

	/* Initialize plugin with patchers */
	ret = IOS_InitSystem(patchers, sizeof(patchers));
	svc_printf("IOS_InitSystem(): %d\n", ret);

	return 0;
}

void my_assert_func(const char *file, int line, const char *func, const char *failedexpr)
{
	svc_printf("assertion \"%s\" failed: file \"%s\", line %d%s%s\n",
		failedexpr, file, line, func ? ", function: " : "", func ? func : "");
}
