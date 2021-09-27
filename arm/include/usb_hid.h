#ifndef USB_HID_H
#define USB_HID_H

#include "ipc.h"
#include "types.h"
#include "fake_wiimote_mgr.h"

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
} usb_input_device_t;

typedef struct usb_device_driver_t {
	u16 vid;
	u16 pid;
	int (*init)(usb_input_device_t *device);
	int (*disconnect)(usb_input_device_t *device);
	int (*slot_changed)(usb_input_device_t *device, u8 slot);
	int (*usb_async_resp)(usb_input_device_t *device);
} usb_device_driver_t;

int usb_hid_init(void);

/* Used by USB device drivers */
int usb_device_driver_issue_write_ctrl(usb_input_device_t *device, u8 requesttype, u8 request,
				       u16 value, u16 index, void *data, u16 length);
int usb_device_driver_issue_read_intr(usb_input_device_t *device, void *data, u16 length);
int usb_device_driver_issue_write_intr(usb_input_device_t *device, void *data, u16 length);

int usb_device_driver_issue_read_ctrl_async(usb_input_device_t *device, u8 requesttype, u8 request,
				            u16 value, u16 index);

int usb_device_driver_issue_read_intr_async(usb_input_device_t *device);
int usb_device_driver_issue_write_intr_async(usb_input_device_t *device);

#endif
