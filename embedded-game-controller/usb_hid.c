#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "client.h"
#include "input_device.h"
#include "ipc.h"
#include "syscalls.h"
#include "usb.h"
#include "usb_backend.h"
#include "usb_device_drivers.h"
#include "usb_hid.h"
#include "utils.h"
#include "wiimote.h"

static const usb_device_driver_t *usb_device_drivers[] = {
    &ds3_usb_device_driver,
    &ds4_usb_device_driver,
    &dr_usb_device_driver,
};

static usb_input_device_t usb_devices[MAX_FAKE_WIIMOTES] ATTRIBUTE_ALIGN(32);

static inline usb_input_device_t *get_free_usb_device_slot(void)
{
    for (int i = 0; i < ARRAY_SIZE(usb_devices); i++) {
        if (usb_devices[i].usb == NULL)
            return &usb_devices[i];
    }

    return NULL;
}

static inline const usb_device_driver_t *get_usb_device_driver_for(u16 vid, u16 pid)
{
    for (int i = 0; i < ARRAY_SIZE(usb_device_drivers); i++) {
        if (usb_device_drivers[i]->probe(vid, pid))
            return usb_device_drivers[i];
    }

    return NULL;
}

/* API exposed to USB device drivers */
const egc_usb_transfer_t *usb_device_driver_issue_ctrl_transfer_async(usb_input_device_t *device,
                                                                      u8 requesttype, u8 request,
                                                                      u16 value, u16 index,
                                                                      void *data, u16 length,
                                                                      egc_transfer_cb callback)
{
    return _egc_usb_backend.ctrl_transfer_async(device->usb, requesttype, request, value, index,
                                                data, length, callback);
}

const egc_usb_transfer_t *usb_device_driver_issue_intr_transfer_async(usb_input_device_t *device,
                                                                      u8 endpoint, void *data,
                                                                      u16 length,
                                                                      egc_transfer_cb callback)
{
    return _egc_usb_backend.intr_transfer_async(device->usb, endpoint, data, length, callback);
}

static bool timer_cb_wrapper(egc_usb_device_t *usb_device)
{
    usb_input_device_t *device = egc_input_device_from_usb(usb_device);
    bool keep = device->driver->timer ? device->driver->timer(device) : false;
    return keep;
}

int usb_device_driver_set_timer(usb_input_device_t *device, int time_us, int repeat_time_us)
{
    return _egc_usb_backend.set_timer(device->usb, time_us, repeat_time_us, timer_cb_wrapper);
}

static int usb_device_ops_resume(void *usrdata, fake_wiimote_t *wiimote)
{
    usb_input_device_t *device = usrdata;

    LOG_DEBUG("usb_device_ops_resume\n");

    if (device->suspended) {
        /* FIXME: Doesn't work properly with DS3.
         * It doesn't report any data after suspend+resume... */
#if 0
		if (usb_hid_v5_suspend_resume(device->host_fd, device->dev_id, 1, 0) != IOS_OK)
			return IOS_ENOENT;
#endif
        device->suspended = false;
    }

    /* Store assigned fake Wiimote */
    device->wiimote = wiimote;

    if (device->driver->init) {
        const egc_usb_devdesc_t *desc = _egc_usb_backend.get_device_descriptor(device->usb);
        return device->driver->init(device, desc->idVendor, desc->idProduct);
    }

    return 0;
}

static int usb_device_ops_suspend(void *usrdata)
{
    int ret = 0;
    usb_input_device_t *device = usrdata;

    LOG_DEBUG("usb_device_ops_suspend\n");

    if (device->driver->disconnect)
        ret = device->driver->disconnect(device);

    /* Suspend the device */
#if 0
	usb_hid_v5_suspend_resume(device->host_fd, device->dev_id, 0, 0);
#endif
    device->suspended = true;

    return ret;
}

static int usb_device_ops_set_leds(void *usrdata, int leds)
{
    int slot;
    usb_input_device_t *device = usrdata;

    LOG_DEBUG("usb_device_ops_set_leds\n");

    if (device->driver->slot_changed) {
        slot = __builtin_ffs(leds);
        return device->driver->slot_changed(device, slot);
    }

    return 0;
}

static int usb_device_ops_set_rumble(void *usrdata, bool rumble_on)
{
    usb_input_device_t *device = usrdata;

    LOG_DEBUG("usb_device_ops_set_rumble\n");

    if (device->driver->set_rumble)
        return device->driver->set_rumble(device, rumble_on);

    return 0;
}

static bool usb_device_ops_report_input(void *usrdata)
{
    usb_input_device_t *device = usrdata;

    // LOG_DEBUG("usb_device_ops_report_input\n");

    return device->driver->report_input(device);
}

static const input_device_ops_t input_device_usb_ops = {
    .resume = usb_device_ops_resume,
    .suspend = usb_device_ops_suspend,
    .set_leds = usb_device_ops_set_leds,
    .set_rumble = usb_device_ops_set_rumble,
    .report_input = usb_device_ops_report_input,
};

static int on_device_added(egc_usb_device_t *usb, u16 vid, u16 pid)
{
    usb_input_device_t *device;
    const usb_device_driver_t *driver;

    /* Find if we have a driver for that VID/PID */
    driver = get_usb_device_driver_for(vid, pid);
    if (!driver)
        return -1;

    /* Get an empty device slot */
    device = get_free_usb_device_slot();
    if (!device)
        return -1;

    /* Get a fake Wiimote from the manager */
    if (!input_devices_add(device, &input_device_usb_ops, &device->input_device))
        return -1;

    /* We have ownership, populate the device info */
    device->usb = usb;
    device->driver = driver;
    /* We will get a fake Wiimote assigneed at the init() callback */
    device->wiimote = NULL;
    device->suspended = false;
    egc_usb_device_set_priv(usb, device);
    return 0;
}

static int on_device_removed(egc_usb_device_t *usb)
{
    usb_input_device_t *device = egc_input_device_from_usb(usb);
    int rc = 0;

    if (device->driver->disconnect)
        rc = device->driver->disconnect(device);

    /* Tell the fake Wiimote manager we got an input device removal */
    input_devices_remove(device->input_device);
    device->usb = NULL;
    return rc;
}

static int event_handler(egc_usb_device_t *device, egc_usb_event_e event, ...)
{
    va_list args;
    va_start(args, event);
    int rc = -1;

    if (event == EGC_USB_EVENT_DEVICE_ADDED) {
        u16 vid = va_arg(args, int);
        u16 pid = va_arg(args, int);
        rc = on_device_added(device, vid, pid);
    } else if (event == EGC_USB_EVENT_DEVICE_REMOVED) {
        rc = on_device_removed(device);
    }
    va_end(args);
    return rc;
}

int usb_hid_init(void)
{
    int rc = _egc_usb_backend.init(event_handler);

    return rc;
}
