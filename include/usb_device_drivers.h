#ifndef USB_DEVICE_DRIVERS_H
#define USB_DEVICE_DRIVERS_H

#include "usb_hid.h"

#define SONY_VID  0x054c
#define DS3_PID   0x0268
#define DS4_PID   0x05c4
#define DS4_2_PID 0x09cc

extern int ds3_driver_ops_init(usb_input_device_t *device);
extern int ds3_driver_ops_disconnect(usb_input_device_t *device);
extern int ds3_driver_ops_slot_changed(usb_input_device_t *device, u8 slot);
extern int ds3_driver_ops_usb_async_resp(usb_input_device_t *device);

extern int ds4_driver_ops_init(usb_input_device_t *device);
extern int ds4_driver_ops_disconnect(usb_input_device_t *device);
extern int ds4_driver_ops_slot_changed(usb_input_device_t *device, u8 slot);
extern int ds4_driver_ops_set_rumble(usb_input_device_t *device, bool rumble_on);
extern int ds4_driver_ops_usb_async_resp(usb_input_device_t *device);

#endif
