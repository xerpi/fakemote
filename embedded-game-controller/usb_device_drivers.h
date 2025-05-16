#ifndef USB_DEVICE_DRIVERS_H
#define USB_DEVICE_DRIVERS_H

#include "usb_hid.h"

/* List of Vendor IDs */
#define SONY_VID 0x054c

struct device_id_t {
    u16 vid;
    u16 pid;
};

static inline bool usb_driver_is_compatible(u16 vid, u16 pid, const struct device_id_t *ids,
                                            int num)
{
    for (int i = 0; i < num; i++) {
        if (ids[i].vid == vid && ids[i].pid == pid)
            return true;
    }

    return false;
}

/* API for device drivers to receive and report controller events */
void fake_wiimote_set_extension(fake_wiimote_t *wiimote, enum wiimote_ext_e ext);
void fake_wiimote_report_input(fake_wiimote_t *wiimote, u16 buttons);
void fake_wiimote_report_accelerometer(fake_wiimote_t *wiimote, u16 acc_x, u16 acc_y, u16 acc_z);
void fake_wiimote_report_ir_dots(fake_wiimote_t *wiimote,
                                 struct ir_dot_t ir_dots[static IR_MAX_DOTS]);
void fake_wiimote_report_input_ext(fake_wiimote_t *wiimote, u16 buttons, const void *ext_data,
                                   u8 ext_size);

extern const usb_device_driver_t ds3_usb_device_driver;
extern const usb_device_driver_t ds4_usb_device_driver;
extern const usb_device_driver_t dr_usb_device_driver;

#endif
