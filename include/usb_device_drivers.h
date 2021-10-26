#ifndef USB_DEVICE_DRIVERS_H
#define USB_DEVICE_DRIVERS_H

#include "usb_hid.h"

#define SONY_VID	0x054c

struct device_id_t {
	u16 vid;
	u16 pid;
};

static inline bool usb_driver_is_comaptible(u16 vid, u16 pid, const struct device_id_t *ids, int num)
{
	for (int i = 0; i < num; i++) {
		if (ids[i].vid == vid && ids[i].pid == pid)
			return true;
	}

	return false;
}

extern const usb_device_driver_t ds3_usb_device_driver;
extern const usb_device_driver_t ds4_usb_device_driver;

#endif
