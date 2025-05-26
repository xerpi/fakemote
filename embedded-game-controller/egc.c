#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "driver_api.h"
#include "platform.h"
#include "usb.h"
#include "usb_backend.h"
#include "utils.h"

static const egc_device_driver_t *usb_device_drivers[] = {
    &ds3_usb_device_driver,
    &ds4_usb_device_driver,
    &dr_usb_device_driver,
};

static egc_input_device_cb s_device_added_cb = NULL;
static egc_input_device_cb s_device_removed_cb = NULL;
static void *s_callbacks_userdata = NULL;

static inline const egc_device_driver_t *get_usb_device_driver_for(u16 vid, u16 pid)
{
    for (int i = 0; i < ARRAY_SIZE(usb_device_drivers); i++) {
        if (usb_device_drivers[i]->probe(vid, pid))
            return usb_device_drivers[i];
    }

    return NULL;
}

/* API exposed to USB device drivers */
egc_device_description_t *egc_device_driver_alloc_desc(egc_input_device_t *device)
{
    return _egc_platform_backend.alloc_desc(device);
}

const egc_usb_transfer_t *egc_device_driver_issue_ctrl_transfer_async(egc_input_device_t *device,
                                                                      u8 requesttype, u8 request,
                                                                      u16 value, u16 index,
                                                                      void *data, u16 length,
                                                                      egc_transfer_cb callback)
{
    if (device->connection == EGC_CONNECTION_DISCONNECTED) return NULL;
    return _egc_platform_backend.usb.ctrl_transfer_async(device, requesttype, request, value, index,
                                                data, length, callback);
}

const egc_usb_transfer_t *egc_device_driver_issue_intr_transfer_async(egc_input_device_t *device,
                                                                      u8 endpoint, void *data,
                                                                      u16 length,
                                                                      egc_transfer_cb callback)
{
    if (device->connection == EGC_CONNECTION_DISCONNECTED) return NULL;
    return _egc_platform_backend.usb.intr_transfer_async(device, endpoint, data, length, callback);
}

static bool timer_cb_wrapper(egc_input_device_t *device)
{
    bool keep = device->driver->timer ? device->driver->timer(device) : false;
    return keep;
}

int egc_device_driver_set_timer(egc_input_device_t *device, int time_us, int repeat_time_us)
{
    return _egc_platform_backend.set_timer(device, time_us, repeat_time_us, timer_cb_wrapper);
}

int egc_device_driver_report_input(egc_input_device_t *device, const egc_input_state_t *state)
{
    return _egc_platform_backend.report_input(device, state);
}

u32 egc_device_driver_map_buttons(u32 buttons, int count, const egc_gamepad_button_e *map)
{
    u32 ret = 0;
    for (int i = 0; i < count; i++) {
        if (buttons & (1 << i)) {
            ret |= 1 << map[i];
        }
    }
    return ret;
}

int egc_input_device_resume(egc_input_device_t *device)
{
    LOG_DEBUG("%s\n", __func__);

    if (device->suspended) {
        /* FIXME: Doesn't work properly with DS3.
         * It doesn't report any data after suspend+resume... */
#if 0
		if (usb_hid_v5_suspend_resume(device->host_fd, device->dev_id, 1, 0) != IOS_OK)
			return IOS_ENOENT;
#endif
        device->suspended = false;
    }

    if (device->driver->init) {
        const egc_usb_devdesc_t *desc = _egc_platform_backend.usb.get_device_descriptor(device);
        return device->driver->init(device, desc->idVendor, desc->idProduct);
    }

    return 0;
}

int egc_input_device_suspend(egc_input_device_t *device)
{
    int ret = 0;

    LOG_DEBUG("%s\n", __func__);

    if (device->driver->disconnect)
        ret = device->driver->disconnect(device);

    /* Suspend the device */
#if 0
	usb_hid_v5_suspend_resume(device->host_fd, device->dev_id, 0, 0);
#endif
    device->suspended = true;

    return ret;
}

int egc_input_device_set_leds(egc_input_device_t *device, u32 led_state)
{
    LOG_DEBUG("%s\n", __func__);

    if (device->driver->set_leds)
        return device->driver->set_leds(device, led_state);

    return 0;
}

int egc_input_device_set_rumble(egc_input_device_t *device, u32 intensity)
{
    LOG_DEBUG("%s\n", __func__);

    if (device->driver->set_rumble)
        return device->driver->set_rumble(device, intensity > 0);

    return 0;
}

static int on_device_added(egc_input_device_t *device, u16 vid, u16 pid)
{
    const egc_device_driver_t *driver;

    /* Find if we have a driver for that VID/PID */
    driver = get_usb_device_driver_for(vid, pid);
    if (!driver)
        return -1;

    /* We have ownership, populate the device info */
    device->driver = driver;
    if (driver->init) {
        int rc = driver->init(device, vid, pid);
        if (rc < 0) return rc;
    }

    /* Inform the client */
    if (s_device_added_cb) s_device_added_cb(device, s_callbacks_userdata);
    return 0;
}

static int on_device_removed(egc_input_device_t *device)
{
    int rc = 0;

    /* Inform the client */
    if (s_device_removed_cb) s_device_removed_cb(device, s_callbacks_userdata);

    if (device->driver->disconnect)
        rc = device->driver->disconnect(device);

    return rc;
}

static int event_handler(egc_input_device_t *device, egc_event_e event, ...)
{
    va_list args;
    va_start(args, event);
    int rc = -1;

    if (event == EGC_EVENT_DEVICE_ADDED) {
        u16 vid = va_arg(args, int);
        u16 pid = va_arg(args, int);
        rc = on_device_added(device, vid, pid);
    } else if (event == EGC_EVENT_DEVICE_REMOVED) {
        rc = on_device_removed(device);
    }
    va_end(args);
    return rc;
}

int egc_initialize(egc_input_device_cb added_cb,
                   egc_input_device_cb removed_cb,
                   void *userdata)
{
    s_device_added_cb = added_cb;
    s_device_removed_cb = removed_cb;
    s_callbacks_userdata = userdata;
    int rc = _egc_platform_backend.init(event_handler);

    return rc;
}

int egc_input_device_set_suspended(egc_input_device_t *device, bool suspended)
{
    if (device->connection == EGC_CONNECTION_USB) {
        return _egc_platform_backend.usb.set_suspended ?
            _egc_platform_backend.usb.set_suspended(device, suspended) : -1;
    }
    return -1;
}

int egc_handle_events()
{
    return _egc_platform_backend.handle_events();
}
