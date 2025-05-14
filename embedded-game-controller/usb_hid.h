#ifndef USB_HID_H
#define USB_HID_H

#include "types.h"
#include "usb.h"
#include "wiimote.h"

#define USB_INPUT_DEVICE_PRIVATE_DATA_SIZE 64

typedef struct fake_wiimote_t fake_wiimote_t;
typedef struct input_device_t input_device_t;
typedef struct egc_usb_device_t egc_usb_device_t;

typedef struct usb_device_driver_t usb_device_driver_t;

typedef struct {
    bool suspended;
    /* Used to communicate with USB module */
    egc_usb_device_t *usb;
    /* Driver that handles this device */
    const usb_device_driver_t *driver;
    /* Assigned fake Wiimote */
    fake_wiimote_t *wiimote;
    /* Assigned input device */
    input_device_t *input_device;
    /* Bytes for private data (usage up to the device driver) */
    u8 private_data[USB_INPUT_DEVICE_PRIVATE_DATA_SIZE] ATTRIBUTE_ALIGN(4);
} usb_input_device_t;

/* Assume that the very first member of struct egc_usb_device_t is a pointer to
 * the private data. */
typedef struct {
    void *priv;
} egc_usb_device_header_t;

static inline usb_input_device_t *egc_input_device_from_usb(egc_usb_device_t *device)
{
    return ((egc_usb_device_header_t *)device)->priv;
}

static inline void egc_usb_device_set_priv(egc_usb_device_t *device, void *priv)
{
    ((egc_usb_device_header_t *)device)->priv = priv;
}

typedef struct usb_device_driver_t {
    bool (*probe)(u16 vid, u16 pid);
    int (*init)(usb_input_device_t *device, u16 vid, u16 pid);
    int (*disconnect)(usb_input_device_t *device);
    int (*slot_changed)(usb_input_device_t *device, u8 slot);
    int (*set_rumble)(usb_input_device_t *device, bool rumble_on);
    bool (*report_input)(usb_input_device_t *device);
    bool (*timer)(usb_input_device_t *device);
} usb_device_driver_t;

int usb_hid_init(void);

/* Used by USB device drivers */
/* TODO: add egc_ prefix to all APIs */
const egc_usb_transfer_t *usb_device_driver_issue_ctrl_transfer_async(usb_input_device_t *device,
                                                                      u8 requesttype, u8 request,
                                                                      u16 value, u16 index,
                                                                      void *data, u16 length,
                                                                      egc_transfer_cb callback);
const egc_usb_transfer_t *usb_device_driver_issue_intr_transfer_async(usb_input_device_t *device,
                                                                      u8 endpoint, void *data,
                                                                      u16 length,
                                                                      egc_transfer_cb callback);
int usb_device_driver_set_timer(usb_input_device_t *device, int time_us, int repeat_time_us);

#endif
