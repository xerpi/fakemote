#include <string.h>
#include "fakedev.h"
#include "hci_state.h"
#include "syscalls.h"
#include "utils.h"

/* Wiimote definitions */
#define WIIMOTE_HCI_CLASS_0 0x00
#define WIIMOTE_HCI_CLASS_1 0x04
#define WIIMOTE_HCI_CLASS_2 0x48

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
	u16 hci_con_handle;
	fakedev_baseband_state_e baseband_state;
	fakedev_acl_state_e acl_state;
} fakedev_t;

static fakedev_t fakedev = {
	.bdaddr = {.b = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}},
	.baseband_state = FAKEDEV_BASEBAND_STATE_INACTIVE,
	.acl_state = FAKEDEV_ACL_STATE_INACTIVE,
};

//static bool fakedev_is_bdaddr_registered(bdaddr_t *)

static void fakedev_check_send_connection_request(void)
{
	bool ret;

	if (fakedev.baseband_state == FAKEDEV_BASEBAND_STATE_REQUEST_CONNECTION) {
		ret = hci_request_connection(&fakedev.bdaddr, WIIMOTE_HCI_CLASS_0,
					     WIIMOTE_HCI_CLASS_1, WIIMOTE_HCI_CLASS_2,
					     HCI_LINK_ACL);
		/* After a connection request is visible to the controller switch to inactive */
		if (ret)
			fakedev.baseband_state = FAKEDEV_BASEBAND_STATE_INACTIVE;
	}
}

void fakedev_init(void)
{
	fakedev.baseband_state = FAKEDEV_BASEBAND_STATE_REQUEST_CONNECTION;
}
int kk = 0;
void fakedev_tick_devices(void)
{
	if (kk++ == 1)
		fakedev_check_send_connection_request();
}

bool fakedev_handle_hci_cmd_accept_con(const bdaddr_t *bdaddr, u8 role)
{
	int ret;

	/* Check if this bdaddr belongs to a fake device */
	if (memcmp(bdaddr, &fakedev.bdaddr, sizeof(bdaddr_t) != 0))
		return false;

	/* Connection accepted to our fake device */
	svc_printf("Connection accepted for our fake device!\n");

	/* The Accept_Connection_Request command will cause the Command Status
	   event to be sent from the Host Controller when the Host Controller
	   begins setting up the connection */

	ret = enqueue_hci_event_command_status(HCI_CMD_ACCEPT_CON);
	assert(ret == IOS_OK);

	fakedev.baseband_state = FAKEDEV_BASEBAND_STATE_COMPLETE;
	fakedev.hci_con_handle = hci_con_handle_virt_alloc();
	svc_printf("Our fake device got HCI con_handle: 0x%x\n", fakedev.hci_con_handle);

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

	//svc_printf("aaaaaaaaaaaaaa\naaaaaaaaaaaaaaaaaaa\n\n");

	//ret = l2cap_send_psm_connect_req(fakedev.bdaddr, L2CAP_PSM_HID_CNTL);
	//svc_printf("HID CNTL connect req: %d\n", ret);
	//svc_printf("L2CAP PSM HID connect req: %d\n", ret);
	//ret = l2cap_send_psm_connect_req(fakedev.bdaddr, L2CAP_PSM_HID_INTR);

	return true;
}
