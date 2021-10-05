#ifndef USB_HID_H
#define USB_HID_H

#include "ipc.h"
#include "types.h"
#include "fake_wiimote_mgr.h"

#define USB_INPUT_DEVICE_PRIVATE_DATA_SIZE 8

typedef struct usb_device_driver_t usb_device_driver_t;

typedef struct {
	bool valid;
	/* Used to communicate with Wii's USB module */
	int host_fd;
	u32 dev_id;
	/* Driver that handles this device */
	const usb_device_driver_t *driver;
	/* Assigned fake Wiimote */
	fake_wiimote_t *wiimote;
	/* Notification message we get when we receive a USB async respone */
	areply usb_async_resp_msg;
	/* Buffer where we store the USB async respones */
	u8 usb_async_resp[128] ATTRIBUTE_ALIGN(32);
	/* Bytes for private data (usage up to the device driver) */
	u8 private_data[USB_INPUT_DEVICE_PRIVATE_DATA_SIZE] ATTRIBUTE_ALIGN(4);
} usb_input_device_t;

typedef struct usb_device_driver_t {
	u16 vid;
	u16 pid;
	int (*init)(usb_input_device_t *device);
	int (*disconnect)(usb_input_device_t *device);
	int (*slot_changed)(usb_input_device_t *device, u8 slot);
	int (*set_rumble)(usb_input_device_t *device, bool rumble_on);
	int (*usb_async_resp)(usb_input_device_t *device);
} usb_device_driver_t;

int usb_hid_init(void);

/* Used by USB device drivers */
int usb_device_driver_issue_ctrl_transfer(usb_input_device_t *device, u8 requesttype, u8 request,
					  u16 value, u16 index, void *data, u16 length);
int usb_device_driver_issue_intr_transfer(usb_input_device_t *device, int out, void *data, u16 length);
int usb_device_driver_issue_ctrl_transfer_async(usb_input_device_t *device, u8 requesttype,
						u8 request, u16 value, u16 index, void *data, u16 length);
int usb_device_driver_issue_intr_transfer_async(usb_input_device_t *device, int out, void *data, u16 length);

#endif
