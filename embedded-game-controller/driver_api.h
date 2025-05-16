#ifndef EGC_DRIVER_API_H
#define EGC_DRIVER_API_H

#include "egc.h"
#include "usb.h"

typedef struct egc_input_device_t egc_input_device_t;
typedef struct egc_device_driver_t egc_device_driver_t;

struct egc_device_driver_t {
    bool (*probe)(u16 vid, u16 pid);
    int (*init)(egc_input_device_t *device, u16 vid, u16 pid);
    int (*disconnect)(egc_input_device_t *device);
    int (*set_leds)(egc_input_device_t *device, u32 leds);
    int (*set_rumble)(egc_input_device_t *device, bool rumble_on);
    bool (*timer)(egc_input_device_t *device);
};

typedef struct {
    u16 vid;
    u16 pid;
} egc_device_id_t;

static inline bool egc_device_driver_is_compatible(u16 vid, u16 pid, const egc_device_id_t *ids,
                                                   int num)
{
    for (int i = 0; i < num; i++) {
        if (ids[i].vid == vid && ids[i].pid == pid)
            return true;
    }

    return false;
}

egc_device_description_t *egc_device_driver_alloc_desc(egc_input_device_t *device);
const egc_usb_transfer_t *egc_device_driver_issue_ctrl_transfer_async(egc_input_device_t *device,
                                                                      u8 requesttype, u8 request,
                                                                      u16 value, u16 index,
                                                                      void *data, u16 length,
                                                                      egc_transfer_cb callback);
const egc_usb_transfer_t *egc_device_driver_issue_intr_transfer_async(egc_input_device_t *device,
                                                                      u8 endpoint, void *data,
                                                                      u16 length,
                                                                      egc_transfer_cb callback);
int egc_device_driver_set_timer(egc_input_device_t *device, int time_us, int repeat_time_us);

/* The driver should not update the `state` field in the egc_input_device_t
 * structure directly, because this might lead to thread synchronisation issues
 * (the driver could be running in a different thread than the client),
 * therefore it should deliver its updates via this method. */
int egc_device_driver_report_input(egc_input_device_t *device, const egc_input_state_t *state);

/* Remaps device buttons from the `buttons` parameter into a bitfield of
 * egc_gamepad_button_e, according to the given map.
 * The bits from 0 to `count-1` of `buttons` are checked and, if set, their
 * index is used to lookup the corresponding egc_gamepad_button_e value within
 * `map`. */
u32 egc_device_driver_map_buttons(u32 buttons, int count, const egc_gamepad_button_e *map);

/* Utility functions for remapping values */
static inline s16 egc_u8_to_s16(u8 value)
{
    return (value << 8 | value) - 0x8000;
}

extern const egc_device_driver_t ds3_usb_device_driver;
extern const egc_device_driver_t ds4_usb_device_driver;
extern const egc_device_driver_t dr_usb_device_driver;

#endif
