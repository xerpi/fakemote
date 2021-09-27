#ifndef USB_DEVICE_DRIVERS_H
#define USB_DEVICE_DRIVERS_H

#include "usb_hid.h"

#define DS4_VID 0x054C
#define DS4_PID 0x05C4

extern int ds4_driver_ops_init(usb_input_device_t *device);
extern int ds4_driver_ops_disconnect(usb_input_device_t *device);
extern int ds4_driver_ops_slot_changed(usb_input_device_t *device, u8 slot);
extern int ds4_driver_ops_usb_intr_in_resp(usb_input_device_t *device);

#endif
