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

#define MAX_CHANNEL_MAP	32
#define THREAD_STACK_SIZE 4096

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

static int ensure_init_oh1_context(void);

/* PendingQ / ReadytQ helpers */

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

static inline u16 copy_ipcmessage_bulk_intr_ioctlv_data(ipcmessage *dst, const ipcmessage *src,
							bool is_intr)
{
	u32 copy_len;
	void *src_data = src->ioctlv.vector[2].data;
	void *dst_data = dst->ioctlv.vector[2].data;

	if (is_intr) {
		hci_event_hdr_t *event = src_data;
		copy_len = sizeof(hci_event_hdr_t) + bswap16(event->length);
	} else {
		hci_acldata_hdr_t *hdr = src_data;
		copy_len = sizeof(hci_acldata_hdr_t) + bswap16(hdr->length);
	}

	os_sync_before_read(src_data, copy_len);
	memcpy(dst_data, src_data, copy_len);
	os_sync_after_write(dst_data, copy_len);

	return copy_len;
}

static int enqueue_recv_bulk_intr_pending_message(ipcmessage *recv_msg, ipcmessage **ret_msg,
						  u16 recv_msg_len, int ready_queue_id,
						  int pending_queue_id, ipcmessage *handed_down_msg,
						  bool is_intr)
{
	int ret;
	ipcmessage *ready_msg;

	/* Fast-path: check if we already have a message ready to be delivered */
	ret = os_message_queue_receive(ready_queue_id, &ready_msg, IOS_MESSAGE_NOBLOCK);
	if (ret == IOS_OK) {
		u16 ack_size = copy_ipcmessage_bulk_intr_ioctlv_data(recv_msg, ready_msg, is_intr);
		ret = os_message_queue_ack(recv_msg, ack_size);
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

/* Message handlers */

static int handle_oh1_dev_bulk_message(ipcmessage *recv_msg, ipcmessage **ret_msg, u8 bEndpoint,
				       u16 wLength, void *data)
{
	int ret = 0;
	//svc_printf("BLK: bEndpoint 0x%x, wLength: 0x%x\n", bEndpoint, wLength);
	//svc_printf("  data: 0x%x 0x%x\n", *(u32 *)data, *(u32 *)(data + 4));

	if (bEndpoint == EP_ACL_DATA_OUT) {
		/* HCI ACL data packet header */
		/*hci_acldata_hdr_t *acl_hdr = data;
		u16 acl_con_handle = HCI_CON_HANDLE(bswap16(acl_hdr->con_handle));
		u16 acl_length     = bswap16(acl_hdr->length);
		u8 *acl_payload    = data + sizeof(hci_acldata_hdr_t);*/

		/* L2CAP header */
		/*l2cap_hdr_t *l2cap_hdr = (void *)acl_payload;
		u16 l2cap_length  = bswap16(l2cap_hdr->length);
		u16 l2cap_dcid    = bswap16(l2cap_hdr->dcid);
		u8 *l2cap_payload = acl_payload + sizeof(l2cap_hdr_t);

		svc_printf("< ACL OUT: hdl: 0x%x, len: 0x%x\n", acl_con_handle, acl_length);*/

		/* This is the ACL datapath from CPU to device (Wiimote) */
		//ret = handle_acl_data_out_l2cap(acl_con_handle, l2cap_dcid,
		//				 l2cap_payload, l2cap_length);
	} else if (bEndpoint == EP_ACL_DATA_IN) {
		//svc_printf("> ACL  IN: len: 0x%x\n", wLength);
		/* We are given an ACL buffer to fill */
		ret = enqueue_recv_bulk_intr_pending_message(recv_msg, ret_msg, wLength,
							     ready_usb_bulk_in_msg_queue_id,
							     pending_usb_bulk_in_msg_queue_id,
							     &usb_bulk_in_msg_handed_to_oh1,
							     false);
	}

	return ret;
}

static int handle_oh1_dev_intr_message(ipcmessage *recv_msg, ipcmessage **ret_msg, u8 bEndpoint,
				       u16 wLength, void *data)
{
	int ret = 0;

	//svc_printf("INT: bEndpoint 0x%x, wLength: 0x%x\n", bEndpoint, wLength);

	/* We are given a HCI buffer to fill */
	if (bEndpoint == EP_HCI_EVENT) {
		ret = enqueue_recv_bulk_intr_pending_message(recv_msg, ret_msg, wLength,
							     ready_usb_intr_msg_queue_id,
							     pending_usb_intr_msg_queue_id,
							     &usb_intr_msg_handed_to_oh1,
							     true);
	}

	return ret;
}

static int handle_oh1_dev_ioctlv(ipcmessage *recv_msg, ipcmessage **ret_msg,
				 u32 cmd, ioctlv *vector, u32 inlen, u32 iolen)
{
	int ret = 0;
	//svc_printf("  ioctlv: cmd 0x%x\n", cmd);

	/* Invalidate cache */
	InvalidateVector(vector, inlen, iolen);

	switch (cmd) {
#if 0
	case USBV0_IOCTLV_CTRLMSG: {
		u8 bmRequestType = *(u8 *)vector[0].data;
		u8 bRequest      = *(u8 *)vector[1].data;
		u16 wValue       = bswap16(*(u16 *)vector[2].data);
		u16 wIndex       = bswap16(*(u16 *)vector[3].data);
		u16 wLength      = bswap16(*(u16 *)vector[4].data);
		// u16 wUnk      = *(u8 *)vector[5].data;
		void *data       = vector[6].data;
		ret = handle_oh1_dev_control_message(recv_msg, ret_msg, bmRequestType, bRequest,
						     wValue, wIndex, wLength, data);
		break;
	}
#endif
	case USBV0_IOCTLV_BLKMSG: {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		ret = handle_oh1_dev_bulk_message(recv_msg, ret_msg, bEndpoint, wLength, data);
		break;
	}
	case USBV0_IOCTLV_INTRMSG: {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		ret = handle_oh1_dev_intr_message(recv_msg, ret_msg, bEndpoint, wLength, data);
		break;
	}
	default:
		/* Unhandled/unknown ioctls are forwarded to the OH1 module */
		//svc_printf("Unhandled IOCTL: 0x%x\n", cmd);
		break;
	}

	return ret;
}

static int OH1_IOS_ReceiveMessage_hook(int queueid, ipcmessage **ret_msg, u32 flags)
{
	int ret;
	ipcmessage *recv_msg;

	ret = os_message_queue_receive(queueid, (void *)&recv_msg, flags);
	if (ret != IOS_OK)
		return ret;

	/* We don't care about other queues... */
	if (queueid != orig_msg_queueid) {
		*ret_msg = recv_msg;
		return ret;
	}

	ensure_init_oh1_context();
	*ret_msg = NULL;

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
		ret = handle_oh1_dev_ioctlv(recv_msg, ret_msg, cmd, vector, inlen, iolen);
		break;
	}
	default:
		/* Unknown command */
		svc_printf("Unhandled IPC command: 0x%x\n", recv_msg->command);
		break;
	}

	if (*ret_msg == NULL)
		*ret_msg = recv_msg;

	return ret;
}

static int enqueue_recv_bulk_intr_ready_message(ipcmessage *ready_msg, int pending_queue_id,
						int ready_queue_id, bool is_intr)
{
	int ret;
	ipcmessage *pending_msg;

	/* Fast-path: check if we have a PendingQ message to fill */
	ret = os_message_queue_receive(pending_queue_id, &pending_msg, IOS_MESSAGE_NOBLOCK);
	if (ret == IOS_OK) {
		/* Copy message data */
		u16 size = copy_ipcmessage_bulk_intr_ioctlv_data(pending_msg, ready_msg, is_intr);
		ret = os_message_queue_ack(pending_msg, size);
	} else {
		/* Push message to ReadyQ */
		ret = os_message_queue_send(ready_queue_id, ready_msg, IOS_MESSAGE_NOBLOCK);
	}

	return ret;
}

static int OH1_IOS_ResourceReply_hook(ipcmessage *ready_msg, int retval)
{
	int ret;

	/* Only consider a message "ready" (valid) if it contains data (retval > 0) */
	if (retval > 0) {
		if (ready_msg == &usb_intr_msg_handed_to_oh1) {
			ensure_init_oh1_context();
			ret = enqueue_recv_bulk_intr_ready_message(ready_msg,
								   pending_usb_intr_msg_queue_id,
								   ready_usb_intr_msg_queue_id,
								   true);
			return ret;
		} else if (ready_msg == &usb_bulk_in_msg_handed_to_oh1) {
			ensure_init_oh1_context();
			ret = enqueue_recv_bulk_intr_ready_message(ready_msg,
								   pending_usb_bulk_in_msg_queue_id,
								   ready_usb_bulk_in_msg_queue_id,
								   false);
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
