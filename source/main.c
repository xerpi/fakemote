#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "conf.h"
#include "fake_wiimote_mgr.h"
#include "globals.h"
#include "injmessage.h"
#include "ipc.h"
#include "hci.h"
#include "hci_state.h"
#include "l2cap.h"
#include "mem.h"
#include "syscalls.h"
#include "tools.h"
#include "types.h"
#include "usb_hid.h"
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

/* Wii's BT USB dongle configuration */
#define EP_HCI_CTRL	0x00
#define EP_HCI_EVENT	0x81
#define EP_ACL_DATA_IN	0x82
#define EP_ACL_DATA_OUT	0x02

/* Private definitions */

/* The Real Wiimmote sends report every ~5ms (200 Hz). */
#define PERIODC_TIMER_PERIOD		5 * 1000
#define HAND_DOWN_MSG_DATA_SIZE		4096

/* Global variables */
u8 g_sensor_bar_position_top;

/* Required by cios-lib... */
char *moduleName = "TST";

/* Queue ID created by OH1 that receives ipcmessages from /dev/usb/oh1 */
int orig_msg_queueid;

/* Periodic timer with large period to tick fakedevices to check their state */
static int periodic_timer_id;
static int periodic_timer_cookie;

/* ipcmessages used when we return from IOS_ReceiveMessage hook to communicate with the USB BT dongle */
static u8 usb_intr_hand_down_msg_data[HAND_DOWN_MSG_DATA_SIZE] ATTRIBUTE_ALIGN(32);
static u8 usb_intr_hand_down_msg_ioctlv_0_data = EP_HCI_EVENT;
static u16 usb_intr_hand_down_msg_ioctlv_1_data;
static ioctlv usb_intr_hand_down_msg_ioctlvs[3] = {
	{&usb_intr_hand_down_msg_ioctlv_0_data, sizeof(usb_intr_hand_down_msg_ioctlv_0_data)},
	{&usb_intr_hand_down_msg_ioctlv_1_data, sizeof(usb_intr_hand_down_msg_ioctlv_1_data)},
	{&usb_intr_hand_down_msg_data,          sizeof(usb_intr_hand_down_msg_data)}
};
static ipcmessage usb_intr_hand_down_msg = {
	.command = IOS_IOCTLV,
	.result = IOS_OK,
	.fd = 0, /* Filled dynamically */
	.ioctlv = {
		.command = USBV0_IOCTLV_INTRMSG,
		.num_in = 2,
		.num_io = 1,
		.vector = usb_intr_hand_down_msg_ioctlvs
	}
};
static bool usb_intr_hand_down_msg_pending = false;

static u8 usb_bulk_in_hand_down_msg_data[HAND_DOWN_MSG_DATA_SIZE] ATTRIBUTE_ALIGN(32);
static u8 usb_bulk_in_hand_down_msg_ioctlv_0_data = EP_ACL_DATA_IN;
static u16 usb_bulk_in_hand_down_msg_ioctlv_1_data;
static ioctlv usb_bulk_in_hand_down_msg_ioctlvs[3] = {
	{&usb_bulk_in_hand_down_msg_ioctlv_0_data, sizeof(usb_bulk_in_hand_down_msg_ioctlv_0_data)},
	{&usb_bulk_in_hand_down_msg_ioctlv_1_data, sizeof(usb_bulk_in_hand_down_msg_ioctlv_1_data)},
	{&usb_bulk_in_hand_down_msg_data,          sizeof(usb_bulk_in_hand_down_msg_data)}
};
static ipcmessage usb_bulk_in_hand_down_msg = {
	.command = IOS_IOCTLV,
	.result = IOS_OK,
	.fd = 0, /* Filled dynamically */
	.ioctlv = {
		.command = USBV0_IOCTLV_BLKMSG,
		.num_in = 2,
		.num_io = 1,
		.vector = usb_bulk_in_hand_down_msg_ioctlvs
	}
};
static bool usb_bulk_in_hand_down_msg_pending = false;

static void *ready_usb_intr_msg_queue_data[8];
static int ready_usb_intr_msg_queue_id;
static ipcmessage *pending_usb_intr_msg_queue_data[8];
static int pending_usb_intr_msg_queue_id;

static void *ready_usb_bulk_in_msg_queue_data[16];
static int ready_usb_bulk_in_msg_queue_id;
static ipcmessage *pending_usb_bulk_in_msg_queue_data[16];
static int pending_usb_bulk_in_msg_queue_id;

/* Function prototypes */

static int ensure_initalized(void);
static int handle_bulk_intr_pending_message(ipcmessage *recv_msg, u16 size, ipcmessage **ret_msg,
					    int ready_queue_id, int pending_queue_id,
					    ipcmessage *hand_down_msg, bool *hand_down_msg_pending,
					    bool *fwd_to_usb);
static int handle_bulk_intr_ready_message(void *ready_msg, int pending_queue_id, int ready_queue_id);

/* Message injection helpers */

int inject_msg_to_usb_intr_ready_queue(void *msg)
{
	return handle_bulk_intr_ready_message(msg, pending_usb_intr_msg_queue_id,
					      ready_usb_intr_msg_queue_id);
}

int inject_msg_to_usb_bulk_in_ready_queue(void *msg)
{
	return handle_bulk_intr_ready_message(msg, pending_usb_bulk_in_msg_queue_id,
					      ready_usb_bulk_in_msg_queue_id);
}

/* Main IOCTLV handler */

static int handle_oh1_dev_ioctlv(ipcmessage *recv_msg, ipcmessage **ret_msg, u32 cmd,
				 ioctlv *vector, u32 inlen, u32 iolen, bool *fwd_to_usb)
{
	int ret = 0;
	void *data;
	u16 wLength;
	u8 bEndpoint, bRequest;

	/* Invalidate cache */
	InvalidateVector(vector, inlen, iolen);

	switch (cmd) {
	case USBV0_IOCTLV_CTRLMSG: {
		bRequest = *(u8 *)vector[1].data;
		if (bRequest == EP_HCI_CTRL) {
			wLength = le16toh(*(u16 *)vector[4].data);
			data    = vector[6].data;
			hci_state_handle_hci_cmd_from_host(data, wLength, fwd_to_usb);
			/* If we don't have to hand it down, we can already ACK it */
			if (!*fwd_to_usb)
				ret = os_message_queue_ack(recv_msg, wLength);
		}
		break;
	}
	case USBV0_IOCTLV_BLKMSG: {
		bEndpoint = *(u8 *)vector[0].data;
		if (bEndpoint == EP_ACL_DATA_OUT) {
			/* This is the ACL datapath from CPU to device (Wiimote) */
			wLength = *(u16 *)vector[1].data;
			data    = vector[2].data;
			hci_state_handle_acl_data_out_request_from_host(data, wLength, fwd_to_usb);
			/* If we don't have to hand it down, we can already ACK it */
			if (!*fwd_to_usb) {
				ret = os_message_queue_ack(recv_msg, wLength);
			}
		} else if (bEndpoint == EP_ACL_DATA_IN) {
			/* We are given an ACL buffer to fill */
			wLength = *(u16 *)vector[1].data;
			ret = handle_bulk_intr_pending_message(recv_msg, wLength, ret_msg,
							       ready_usb_bulk_in_msg_queue_id,
							       pending_usb_bulk_in_msg_queue_id,
							       &usb_bulk_in_hand_down_msg,
							       &usb_bulk_in_hand_down_msg_pending,
							       fwd_to_usb);
		}
		break;
	}
	case USBV0_IOCTLV_INTRMSG: {
		bEndpoint = *(u8 *)vector[0].data;
		if (bEndpoint == EP_HCI_EVENT) {
			wLength = *(u16 *)vector[1].data;
			/* We are given a HCI buffer to fill */
			ret = handle_bulk_intr_pending_message(recv_msg, wLength, ret_msg,
							       ready_usb_intr_msg_queue_id,
							       pending_usb_intr_msg_queue_id,
							       &usb_intr_hand_down_msg,
							       &usb_intr_hand_down_msg_pending,
							       fwd_to_usb);
		}
		break;
	}
	default:
		/* Unhandled/unknown ioctls are forwarded to the OH1 module */
		DEBUG("Unhandled IOCTL: 0x%x\n", cmd);
		break;
	}

	return ret;
}

/* PendingQ / ReadyQ helpers */

static inline void copy_data_to_ipcmessage(ipcmessage *dst, const void *src, u16 len)
{
	void *dst_data = dst->ioctlv.vector[2].data;

	memcpy(dst_data, src, len);
	os_sync_after_write(dst_data, len);
}

static inline void configure_hand_down_msg(ipcmessage *msg, int fd, u16 wLength)
{
	assert(wLength <= HAND_DOWN_MSG_DATA_SIZE);

	*(u16 *)msg->ioctlv.vector[1].data = wLength;
	msg->ioctlv.vector[2].len = wLength;
	msg->fd = fd;

	os_sync_before_read(msg->ioctlv.vector[2].data, wLength);
}

static inline int copy_and_ack_ipcmessage(ipcmessage *pend_msg, void *ready_msg)
{
	int retval;
	void *ready_data;

	if (is_message_injected(ready_msg)) {
		ready_data = ((injmessage *)ready_msg)->data;
		retval = ((injmessage *)ready_msg)->size;
		copy_data_to_ipcmessage(pend_msg, ready_data, retval);
		/* If it was a message we injected ourselves, we have to deallocate it */
		injmessage_free(ready_msg);
	} else {
		ready_data = ((ipcmessage *)ready_msg)->ioctlv.vector[2].data;
		retval = ((ipcmessage *)ready_msg)->result;
		/* If retval is positive, it contains the data size, an error otherwise */
		if (retval > 0)
			copy_data_to_ipcmessage(pend_msg, ready_data, retval);
	}

	/* Finally, we can ACK the message! */
	return os_message_queue_ack(pend_msg, retval);
}

static int handle_bulk_intr_pending_message(ipcmessage *pend_msg, u16 size, ipcmessage **ret_msg,
					    int ready_queue_id, int pending_queue_id,
					    ipcmessage *hand_down_msg, bool *hand_down_msg_pending,
					    bool *fwd_to_usb)
{
	int ret;
	void *ready_msg;

	/* Fast-path: check if we already have a message ready to be delivered */
	ret = os_message_queue_receive(ready_queue_id, &ready_msg, IOS_MESSAGE_NOBLOCK);
	if (ret == IOS_OK) {
		ret = copy_and_ack_ipcmessage(pend_msg, ready_msg);
		/* We have already ACKed it, we don't have to hand it down to OH1 */
		*fwd_to_usb = false;
	} else {
		/* Push the received message to the PendingQ */
		ret = os_message_queue_send(pending_queue_id, pend_msg, IOS_MESSAGE_NOBLOCK);
		if ((ret == IOS_OK) && !*hand_down_msg_pending) {
			/* Hand down to OH1 a copy of the message for it to fill it from real USB data */
			configure_hand_down_msg(hand_down_msg, pend_msg->fd, size);
			*ret_msg = hand_down_msg;
			*hand_down_msg_pending = true;
		} else {
			/* We already have a hand down message to OH1 USB pending... */
			*fwd_to_usb = false;
		}
	}

	return ret;
}

static int handle_bulk_intr_ready_message(void *ready_msg, int pending_queue_id, int ready_queue_id)
{
	int ret;
	ipcmessage *pend_msg;

	/* Fast-path: check if we have a PendingQ message to fill */
	ret = os_message_queue_receive(pending_queue_id, &pend_msg, IOS_MESSAGE_NOBLOCK);
	if (ret == IOS_OK) {
		ret = copy_and_ack_ipcmessage(pend_msg, ready_msg);
	} else {
		/* Push message to ReadyQ. We store the return value/size to the "result" field */
		ret = os_message_queue_send(ready_queue_id, ready_msg, IOS_MESSAGE_NOBLOCK);
	}

	return ret;
}

/* Hooked functions */

static int OH1_IOS_ReceiveMessage_hook(int queueid, ipcmessage **ret_msg, u32 flags)
{
	int ret;
	uintptr_t recv_data;
	ipcmessage *recv_msg;
	bool fwd_to_usb;

	/* We don't care about other queues... */
	if (queueid != orig_msg_queueid)
		return os_message_queue_receive(queueid, (void *)ret_msg, flags);

	ensure_initalized();

	while (1) {
		ret = os_message_queue_receive(queueid, &recv_data, flags);
		if (ret != IOS_OK) {
			DEBUG("Message queue recv err: %d\n", ret);
			break;
		} else if (recv_data == 0xcafef00d) {
			*ret_msg = (ipcmessage *)0xcafef00d;
			break;
		} else if (recv_data == (uintptr_t)&periodic_timer_cookie) {
			input_devices_tick();
			fake_wiimote_mgr_tick_devices();
			fwd_to_usb = false;
		} else {
			recv_msg = (ipcmessage *)recv_data;
			*ret_msg = NULL;
			/* Default to forward message to OH1 */
			fwd_to_usb = true;

			if (recv_msg->command == IOS_IOCTLV) {
				ioctlv *vector = recv_msg->ioctlv.vector;
				u32     inlen  = recv_msg->ioctlv.num_in;
				u32     iolen  = recv_msg->ioctlv.num_io;
				u32     cmd    = recv_msg->ioctlv.command;
				ret = handle_oh1_dev_ioctlv(recv_msg, ret_msg, cmd, vector,
							    inlen, iolen, &fwd_to_usb);
			}
		}

		/* Break the loop and return from the hook if we want
		 * to deliver the message to the BT USB dongle */
		if (fwd_to_usb) {
			/* Just send the original message we received if we don't
			 * want to hand down an injected message to OH1 */
			if (*ret_msg == NULL)
				*ret_msg = (ipcmessage *)recv_data;
			break;
		}
	}

	return ret;
}

static int OH1_IOS_ResourceReply_hook(ipcmessage *ready_msg, int retval)
{
	int ret;
	ioctlv *vector;
	void *data;

	if (ready_msg == &usb_intr_hand_down_msg) {
		usb_intr_hand_down_msg_pending = 0;
		ensure_initalized();
		assert(ready_msg->command == IOS_IOCTLV);
		assert(ready_msg->ioctlv.command == USBV0_IOCTLV_INTRMSG);
		/* Let the HCI tracker know about this HCI event response coming from OH1 */
		if (retval > 0) {
			vector = ready_msg->ioctlv.vector;
			data = vector[2].data;
			hci_state_handle_hci_event_from_controller(data, retval);
		}
		ready_msg->result = retval;
		ret = handle_bulk_intr_ready_message(ready_msg, pending_usb_intr_msg_queue_id,
						     ready_usb_intr_msg_queue_id);
		return ret;
	} else if (ready_msg == &usb_bulk_in_hand_down_msg) {
		usb_bulk_in_hand_down_msg_pending = 0;
		ensure_initalized();
		vector = ready_msg->ioctlv.vector;
		assert(ready_msg->command == IOS_IOCTLV);
		assert(ready_msg->ioctlv.command == USBV0_IOCTLV_BLKMSG);
		/* Let the HCI tracker know about this HCI ACL IN response coming from OH1 */
		if (retval > 0) {
			vector = ready_msg->ioctlv.vector;
			data = vector[2].data;
			hci_state_handle_acl_data_in_response_from_controller(data, retval);
		}
		ready_msg->result = retval;
		ret = handle_bulk_intr_ready_message(ready_msg, pending_usb_bulk_in_msg_queue_id,
						     ready_usb_bulk_in_msg_queue_id);
		return ret;
	}

	return os_message_queue_ack(ready_msg, retval);
}

static int ensure_initalized(void)
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

		/* Initialize global state */
		injmessage_init_heap();
		hci_state_reset();
		input_devices_init();
		fake_wiimote_mgr_init();
		usb_hid_init();

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

static int read_conf(u8 conf_buffer[static 0x4000])
{
	int fd, ret;

	fd = os_open("/shared2/sys/SYSCONF", IOS_OPEN_READ);
	if (fd < 0)
		return fd;

	ret = os_read(fd, conf_buffer, 0x4000);

	os_close(fd);

	return ret;
}

static int write_conf(u8 conf_buffer[static 0x4000])
{
	int fd, ret;

	fd = os_open("/shared2/sys/SYSCONF", IOS_OPEN_WRITE);
	if (fd < 0)
		return fd;

	ret = os_write(fd, conf_buffer, 0x4000);

	os_close(fd);

	return ret;
}

static int patch_conf_bt_dinf(u8 conf_buffer[static CONF_SIZE])
{
	static struct conf_pads_setting conf_pads;
	bdaddr_t bdaddr;
	int ret;
	int paired_count;
	int start, count;

	/* Get paired Wiimote configuration */
	ret = conf_get(conf_buffer, "BT.DINF", &conf_pads, sizeof(conf_pads));
	DEBUG("conf_get(): %d\n", ret);
	if (ret != sizeof(conf_pads))
		return IOS_EINVAL;
	DEBUG("  num_registered: %d\n", conf_pads.num_registered);

	/* Check how many "Fake Wiimotes" are paired */
	paired_count = 0;
	for (int i = 0; i < conf_pads.num_registered; i++) {
		DEBUG("  registered[%d]: \"%s\"\n", i, conf_pads.registered[i].name);
		/* Check if the bdaddr matches */
		baswap(&bdaddr, &conf_pads.registered[i].bdaddr);
		if (bacmp(&bdaddr, &FAKE_WIIMOTE_BDADDR(paired_count)) == 0)
			paired_count++;
	}
	DEBUG("Found %d paired \"Fake Wiimotes\"\n", paired_count);

	/* Give at least the last two entries (out of 10) for fake Wiimotes */
	if (paired_count >= 2)
		return 0;
	count = 2 - paired_count;
	start = MIN2(conf_pads.num_registered, CONF_PAD_MAX_REGISTERED - 2);

	for (int i = 0; i < count; i++) {
		/* Copy MAC address */
		baswap(&conf_pads.registered[start + i].bdaddr, &FAKE_WIIMOTE_BDADDR(i));
		/* Generate and copy the name:
		 *   Wii memcmps the name with "Nintendo RVL-CNT-01" and size 19 */
		snprintf(conf_pads.registered[start + i].name,
			 sizeof(conf_pads.registered[start + i].name),
			 "Nintendo RVL-CNT-01 (Fake Wiimote %d)", i);
	}
	conf_pads.num_registered = start + count;

	/* Write new paired Wiimote configuration back to the buffer */
	conf_set(conf_buffer, "BT.DINF", &conf_pads, sizeof(conf_pads));

	/* Write updated SYSCONF back */
	ret = write_conf(conf_buffer);
	if (ret != CONF_SIZE)
		return IOS_EINVAL;

	return 0;
}

int main(void)
{
	static u8 conf_buffer[CONF_SIZE];
	int ret;

	/* Print info */
	svc_write("$IOSVersion: FAKEMOTE:  " __DATE__ " " __TIME__ " 64M "
		  TOSTRING(FAKEMOTE_MAJOR) "."
		  TOSTRING(FAKEMOTE_MINOR) "."
		  TOSTRING(FAKEMOTE_PATCH) "-"
		  TOSTRING(FAKEMOTE_HASH) " $\n");

	/* Read SYSCONF */
	ret = read_conf(conf_buffer);
	if (ret != CONF_SIZE)
		return IOS_EINVAL;

	/* Read IR sensor bar position */
	ret = conf_get(conf_buffer, "BT.BAR", &g_sensor_bar_position_top,
		       sizeof(g_sensor_bar_position_top));
	if (ret != sizeof(g_sensor_bar_position_top))
		return IOS_EINVAL;

	/* Patch SYSCONF's BT.DINF */
	ret = patch_conf_bt_dinf(conf_buffer);
	if (ret < 0)
		return ret;

	/* System patchers */
	patcher patchers[] = {
		{Patch_OH1UsbModule, 0},
	};

	/* Initialize plugin with patchers */
	ret = IOS_InitSystem(patchers, sizeof(patchers));
	DEBUG("IOS_InitSystem(): %d\n", ret);

	return ret;
}

void my_assert_func(const char *file, int line, const char *func, const char *failedexpr)
{
	printf("assertion \"%s\" failed: file \"%s\", line %d%s%s\n",
		failedexpr, file, line, func ? ", function: " : "", func ? func : "");
}
