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
#define le16toh bswap16
#define htole16 bswap16
#define printf svc_printf
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#ifdef assert
#undef assert
#endif
#define assert(exp) ( (exp) ? (void)0 : my_assert_func(__FILE__, __LINE__, __FUNCTION__, #exp))

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
#define MAX_HCI_CONNECTIONS	32
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

/* Snooped HCI state (requested by SW BT stack) */
static u8 hci_unit_class[HCI_CLASS_SIZE];
static u8 hci_page_scan_enable = 0;

/* Simulated HCI state */
static struct {
	bool valid;
	u16 virt; /* The one we return to the BT SW stack */
	u16 phys; /* The one the BT dongle uses */
} hci_virt_con_handle_map_table[MAX_HCI_CONNECTIONS];

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

static void my_assert_func(const char *file, int line, const char *func, const char *failedexpr);
static int ensure_init_oh1_context(void);
static int handle_bulk_message_to_oh1_dev(ipcmessage *recv_msg, ipcmessage **ret_msg, u8 bEndpoint,
				       u16 wLength, void *data, int *fwd_to_usb);
static int handle_bulk_intr_pending_message(ipcmessage *recv_msg, ipcmessage **ret_msg,
						  u16 recv_msg_len, int ready_queue_id,
						  int pending_queue_id, ipcmessage *handed_down_msg,
						  int *fwd_to_usb);
static int handle_bulk_intr_ready_message(ipcmessage *ready_msg, int retval, int pending_queue_id,
					  int ready_queue_id);

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
	svc_printf("fakedev_tick_devices\n");
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

/* HCI command handlers */

static int handle_hci_cmd_accept_con(void *data, int *fwd_to_usb)
{
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
	if (memcmp(&cp->bdaddr, &fakedev.bdaddr, sizeof(bdaddr_t) != 0))
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

	if (fakedev.acl_state == FAKEDEV_ACL_STATE_LINKING) {
		//svc_printf("L2CAP STARTING\n");
		//ret = l2cap_send_psm_connect_req(fakedev.bdaddr, L2CAP_PSM_HID_CNTL);
		//svc_printf("HID CNTL connect req: %d\n", ret);
		//svc_printf("L2CAP PSM HID connect req: %d\n", ret);
		//ret = l2cap_send_psm_connect_req(fakedev.bdaddr, L2CAP_PSM_HID_INTR);
		//svc_printf("L2CAP PSM INTR connect req: %d\n", ret);

		//fakedev.acl_state = FAKEDEV_ACL_STATE_LINKED;

		//*fwd_to_usb = 0;
	}

	return 0;
}

/* Messages replies from OH1 handlers */

static void handle_intr_message_reply_from_oh1_dev(ipcmessage *msg, u8 bEndpoint,
						   u16 wLength, void *data)
{
	bool ret;
	bool modified = false;
	hci_event_hdr_t *hdr = data;
	void *payload = (void *)((u8 *)hdr + sizeof(hci_event_hdr_t));
	assert(bEndpoint == EP_HCI_EVENT);

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

void handle_message_reply_from_oh1_dev(ipcmessage *msg, int retval)
{
	//svc_printf("handle_message_reply_from_oh1_dev %p, %d\n", msg, retval);

	if (msg->command != IOS_IOCTLV)
		return;

	ioctlv *vector = msg->ioctlv.vector;
	u32     inlen  = msg->ioctlv.num_in;
	u32     iolen  = msg->ioctlv.num_io;
	u32     cmd    = msg->ioctlv.command;

	switch (cmd) {
	case USBV0_IOCTLV_CTRLMSG: {
		u8 bmRequestType = *(u8 *)vector[0].data;
		u8 bRequest      = *(u8 *)vector[1].data;
		u16 wValue       = le16toh(*(u16 *)vector[2].data);
		u16 wIndex       = le16toh(*(u16 *)vector[3].data);
		u16 wLength      = le16toh(*(u16 *)vector[4].data);
		// u16 wUnk      = *(u8 *)vector[5].data;
		void *data       = vector[6].data;
		//svc_printf("  ctl\n");
		//handle_control_message_to_oh1_dev(bmRequestType, bRequest, wValue,
		//				     wIndex, wLength, data, fwd_to_usb);
		break;
	}
	case USBV0_IOCTLV_BLKMSG: {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		//svc_printf("  bulk\n");
		//handle_bulk_message_to_oh1_dev(recv_msg, ret_msg, bEndpoint, wLength, data, fwd_to_usb);
		break;
	}
	case USBV0_IOCTLV_INTRMSG: {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		handle_intr_message_reply_from_oh1_dev(msg, bEndpoint, wLength, data);
		break;
	}
	}
}

/* Messages requests to OH1 handlers */

static int handle_control_message_to_oh1_dev(u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex,
					  u16 wLength, void *data, int *fwd_to_usb)
{
	int ret = 0;

	//if (kk++ < 5)
	//svc_printf("CTL: bmRequestType 0x%x, bRequest: 0x%x, "
	//	   "wValue: 0x%x, wIndex: 0x%x, wLength: 0x%x\n",
	//	   bmRequestType, bRequest, wValue, wIndex, wLength);
	//svc_printf("  data: 0x%x 0x%x\n", *(u32 *)data, *(u32 *)(data + 4));

	/* Ignore non-HCI control messages... */
	if (bRequest != EP_HCI_CTRL)
		return 0;

	hci_cmd_hdr_t *cmd_hdr = data;
	u16 opcode = le16toh(cmd_hdr->opcode);
	u16 ocf = HCI_OCF(opcode);
	u16 ogf = HCI_OGF(opcode);

	//if (kk++ < 30)
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

static int handle_acl_data_out_l2cap(void *data, u32 size, int *fwd_to_usb)
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
				fakedev.acl_state = FAKEDEV_ACL_STATE_INACTIVE;
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

static int handle_bulk_message_to_oh1_dev(ipcmessage *recv_msg, ipcmessage **ret_msg, u8 bEndpoint,
				       u16 wLength, void *data, int *fwd_to_usb)
{
	int ret = 0;
	//svc_printf("BLK: bEndpoint 0x%x, wLength: 0x%x\n", bEndpoint, wLength);
	//svc_printf("  data: 0x%x 0x%x 0x%x 0x%x 0x%x\n", *(u32 *)data, *(u32 *)(data + 4), *(u32 *)(data + 8), *(u32 *)(data + 12), *(u32 *)(data + 16));

	if (bEndpoint == EP_ACL_DATA_OUT) {
		/* This is the ACL datapath from CPU to device (Wiimote) */
		ret = handle_acl_data_out_l2cap(data, wLength, fwd_to_usb);
	} else if (bEndpoint == EP_ACL_DATA_IN) {
		//svc_printf("> ACL  IN: len: 0x%x\n", wLength);
		/* We are given an ACL buffer to fill */
		ret = handle_bulk_intr_pending_message(recv_msg, ret_msg, wLength,
						       ready_usb_bulk_in_msg_queue_id,
						       pending_usb_bulk_in_msg_queue_id,
						       &usb_bulk_in_msg_handed_to_oh1,
						       fwd_to_usb);
	}

	return ret;
}

static int handle_intr_message_to_oh1_dev(ipcmessage *recv_msg, ipcmessage **ret_msg, u8 bEndpoint,
				       u16 wLength, int *fwd_to_usb)
{
	int ret = 0;

	//svc_printf("INT: bEndpoint 0x%x, wLength: 0x%x\n", bEndpoint, wLength);

	/* We are given a HCI buffer to fill */
	if (bEndpoint == EP_HCI_EVENT) {
		ret = handle_bulk_intr_pending_message(recv_msg, ret_msg, wLength,
						       ready_usb_intr_msg_queue_id,
						       pending_usb_intr_msg_queue_id,
						       &usb_intr_msg_handed_to_oh1,
						       fwd_to_usb);
	}

	return ret;
}

static int handle_oh1_dev_ioctlv(ipcmessage *recv_msg, ipcmessage **ret_msg, u32 cmd,
				 ioctlv *vector, u32 inlen, u32 iolen, int *fwd_to_usb)
{
	int ret = 0;
	//svc_printf("  ioctlv: cmd 0x%x\n", cmd);

	/* Invalidate cache */
	InvalidateVector(vector, inlen, iolen);

	switch (cmd) {
	case USBV0_IOCTLV_CTRLMSG: {
		u8 bmRequestType = *(u8 *)vector[0].data;
		u8 bRequest      = *(u8 *)vector[1].data;
		u16 wValue       = le16toh(*(u16 *)vector[2].data);
		u16 wIndex       = le16toh(*(u16 *)vector[3].data);
		u16 wLength      = le16toh(*(u16 *)vector[4].data);
		// u16 wUnk      = *(u8 *)vector[5].data;
		void *data       = vector[6].data;
		ret = handle_control_message_to_oh1_dev(bmRequestType, bRequest, wValue,
						     wIndex, wLength, data, fwd_to_usb);
		break;
	}
	case USBV0_IOCTLV_BLKMSG: {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		ret = handle_bulk_message_to_oh1_dev(recv_msg, ret_msg, bEndpoint, wLength, data, fwd_to_usb);
		break;
	}
	case USBV0_IOCTLV_INTRMSG: {
		u8 bEndpoint = *(u8 *)vector[0].data;
		u16 wLength  = *(u16 *)vector[1].data;
		void *data   = vector[2].data;
		ret = handle_intr_message_to_oh1_dev(recv_msg, ret_msg, bEndpoint, wLength, fwd_to_usb);
		break;
	}
	default:
		/* Unhandled/unknown ioctls are forwarded to the OH1 module */
		//svc_printf("Unhandled IOCTL: 0x%x\n", cmd);
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
	for (int i = 0; i < ARRAY_SIZE(channels); i++)
		channels[i].active = false;

	for (int i = 0; i < ARRAY_SIZE(hci_virt_con_handle_map_table); i++)
		hci_virt_con_handle_map_table[i].valid = false;

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

static void my_assert_func(const char *file, int line, const char *func, const char *failedexpr)
{
	svc_printf("assertion \"%s\" failed: file \"%s\", line %d%s%s\n",
		failedexpr, file, line, func ? ", function: " : "", func ? func : "");
}
