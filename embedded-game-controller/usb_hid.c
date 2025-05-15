#include <stdio.h>
#include <string.h>

#include "client.h"
#include "input_device.h"
#include "ipc.h"
#include "syscalls.h"
#include "usb.h"
#include "usb_device_drivers.h"
#include "usb_hid.h"
#include "utils.h"
#include "wiimote.h"

/* USBv5 USB_HID IOCTL commands */
#define USBV5_IOCTL_GETVERSION      0
#define USBV5_IOCTL_GETDEVICECHANGE 1
#define USBV5_IOCTL_SHUTDOWN        2
#define USBV5_IOCTL_GETDEVPARAMS    3
#define USBV5_IOCTL_ATTACH          4
#define USBV5_IOCTL_RELEASE         5
#define USBV5_IOCTL_ATTACHFINISH    6
#define USBV5_IOCTL_SETALTERNATE    7
#define USBV5_IOCTL_SUSPEND_RESUME  16
#define USBV5_IOCTL_CANCELENDPOINT  17
#define USBV5_IOCTL_CTRLMSG         18
#define USBV5_IOCTL_INTRMSG         19
#define USBV5_IOCTL_ISOMSG          20 /* Not available in USB_HID */
#define USBV5_IOCTL_BULKMSG         21 /* Not available in USB_HID */

/* Constants */
#define USB_MAX_DEVICES 32

/* USBv5 HID message structure */
struct usb_hid_v5_transfer {
    u32 dev_id;
    u32 zero;

    union {
        struct {
            u8 bmRequestType;
            u8 bmRequest;
            u16 wValue;
            u16 wIndex;
        } ctrl;

        struct {
            u32 out;
        } intr;

        u32 data[14];
    };
} ATTRIBUTE_PACKED;
static_assert(sizeof(struct usb_hid_v5_transfer) == 64);

static const usb_device_driver_t *usb_device_drivers[] = {
    &ds3_usb_device_driver,
    &ds4_usb_device_driver,
    &dr_usb_device_driver,
};

static usb_input_device_t usb_devices[MAX_FAKE_WIIMOTES] ATTRIBUTE_ALIGN(32);
static usb_device_entry device_change_devices[USB_MAX_DEVICES] ATTRIBUTE_ALIGN(32);
static int host_fd = -1;
static u8 worker_thread_stack[1024] ATTRIBUTE_ALIGN(32);
static u32 queue_data[32] ATTRIBUTE_ALIGN(32);
static int queue_id = -1;

/* Async notification messages */
static areply notification_messages[2] = { 0 };
#define MESSAGE_DEVCHANGE    &notification_messages[0]
#define MESSAGE_ATTACHFINISH &notification_messages[1]

static inline usb_input_device_t *get_usb_device_for_dev_id(u32 dev_id)
{
    for (int i = 0; i < ARRAY_SIZE(usb_devices); i++) {
        if (usb_devices[i].valid && (usb_devices[i].dev_id == dev_id))
            return &usb_devices[i];
    }

    return NULL;
}

static inline usb_input_device_t *get_free_usb_device_slot(void)
{
    for (int i = 0; i < ARRAY_SIZE(usb_devices); i++) {
        if (!usb_devices[i].valid)
            return &usb_devices[i];
    }

    return NULL;
}

static inline bool is_usb_device_connected(u32 dev_id)
{
    return get_usb_device_for_dev_id(dev_id) != NULL;
}

static inline const usb_device_driver_t *get_usb_device_driver_for(u16 vid, u16 pid)
{
    for (int i = 0; i < ARRAY_SIZE(usb_device_drivers); i++) {
        if (usb_device_drivers[i]->probe(vid, pid))
            return usb_device_drivers[i];
    }

    return NULL;
}

static int usb_hid_v5_get_descriptors(int host_fd, u32 dev_id, usb_devdesc *udd)
{
    u32 inbuf[8] ATTRIBUTE_ALIGN(32);
    u8 outbuf[96] ATTRIBUTE_ALIGN(32);

    /* Setup buffer */
    memset(inbuf, 0, sizeof(inbuf));
    inbuf[0] = dev_id;
    inbuf[2] = 0;

    /* Get device parameters */
    return os_ioctl(host_fd, USBV5_IOCTL_GETDEVPARAMS, inbuf, sizeof(inbuf), outbuf,
                    sizeof(outbuf));
}

static inline void build_ctrl_transfer(struct usb_hid_v5_transfer *transfer, int dev_id,
                                       u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex)
{
    memset(transfer, 0, sizeof(*transfer));
    transfer->dev_id = dev_id;
    transfer->ctrl.bmRequestType = bmRequestType;
    transfer->ctrl.bmRequest = bmRequest;
    transfer->ctrl.wValue = wValue;
    transfer->ctrl.wIndex = wIndex;
}

static inline void build_intr_transfer(struct usb_hid_v5_transfer *transfer, int dev_id, int out)
{
    memset(transfer, 0, sizeof(*transfer));
    transfer->dev_id = dev_id;
    transfer->intr.out = out;
}

static inline int usb_hid_v5_ctrl_transfer(int host_fd, int dev_id, u8 bmRequestType, u8 bmRequest,
                                           u16 wValue, u16 wIndex, u16 wLength, void *rpData)
{
    struct usb_hid_v5_transfer transfer ATTRIBUTE_ALIGN(32);
    ioctlv vectors[2];
    int out = !(bmRequestType & USB_ENDPOINT_IN);

    build_ctrl_transfer(&transfer, dev_id, bmRequestType, bmRequest, wValue, wIndex);

    vectors[0].data = &transfer;
    vectors[0].len = sizeof(transfer);
    vectors[1].data = rpData;
    vectors[1].len = wLength;

    return os_ioctlv(host_fd, USBV5_IOCTL_CTRLMSG, 1 + out, 1 - out, vectors);
}

static inline int usb_hid_v5_ctrl_transfer_async(int host_fd, int dev_id, u8 bmRequestType,
                                                 u8 bmRequest, u16 wValue, u16 wIndex, u16 wLength,
                                                 void *rpData, int queue_id, void *message)
{
    struct usb_hid_v5_transfer transfer ATTRIBUTE_ALIGN(32);
    ioctlv vectors[2];
    int out = !(bmRequestType & USB_ENDPOINT_IN);

    build_ctrl_transfer(&transfer, dev_id, bmRequestType, bmRequest, wValue, wIndex);

    vectors[0].data = &transfer;
    vectors[0].len = sizeof(transfer);
    vectors[1].data = rpData;
    vectors[1].len = wLength;

    return os_ioctlv_async(host_fd, USBV5_IOCTL_CTRLMSG, 1 + out, 1 - out, vectors, queue_id,
                           message);
}

static inline int usb_hid_v5_intr_transfer(int host_fd, int dev_id, int out, u16 wLength,
                                           void *rpData)
{
    struct usb_hid_v5_transfer transfer ATTRIBUTE_ALIGN(32);
    ioctlv vectors[2];

    build_intr_transfer(&transfer, dev_id, out);

    vectors[0].data = &transfer;
    vectors[0].len = sizeof(transfer);
    vectors[1].data = rpData;
    vectors[1].len = wLength;

    return os_ioctlv(host_fd, USBV5_IOCTL_INTRMSG, 1 + out, 1 - out, vectors);
}

static inline int usb_hid_v5_intr_transfer_async(int host_fd, int dev_id, int out, u16 wLength,
                                                 void *rpData, int queue_id, void *message)
{
    struct usb_hid_v5_transfer transfer ATTRIBUTE_ALIGN(32);
    ioctlv vectors[2];

    build_intr_transfer(&transfer, dev_id, out);

    vectors[0].data = &transfer;
    vectors[0].len = sizeof(transfer);
    vectors[1].data = rpData;
    vectors[1].len = wLength;

    return os_ioctlv_async(host_fd, USBV5_IOCTL_INTRMSG, 1 + out, 1 - out, vectors, queue_id,
                           message);
}

static int usb_hid_v5_attach(int host_fd, u32 dev_id)
{
    u32 buf[8] ATTRIBUTE_ALIGN(32);

    memset(buf, 0, sizeof(buf));
    buf[0] = dev_id;

    return os_ioctl(host_fd, USBV5_IOCTL_ATTACH, buf, sizeof(buf), NULL, 0);
}

static int usb_hid_v5_release(int host_fd, u32 dev_id)
{
    u32 buf[8] ATTRIBUTE_ALIGN(32);

    memset(buf, 0, sizeof(buf));
    buf[0] = dev_id;

    return os_ioctl(host_fd, USBV5_IOCTL_RELEASE, buf, sizeof(buf), NULL, 0);
}

static int usb_hid_v5_suspend_resume(int host_fd, int dev_id, int resumed, u32 unk)
{
    u32 buf[8] ATTRIBUTE_ALIGN(32);

    memset(buf, 0, sizeof(buf));
    buf[0] = dev_id;
    buf[2] = resumed;

    return os_ioctl(host_fd, USBV5_IOCTL_SUSPEND_RESUME, buf, sizeof(buf), NULL, 0);
}

/* API exposed to USB device drivers */
int usb_device_driver_issue_ctrl_transfer(usb_input_device_t *device, u8 requesttype, u8 request,
                                          u16 value, u16 index, void *data, u16 length)
{
    return usb_hid_v5_ctrl_transfer(device->host_fd, device->dev_id, requesttype, request, value,
                                    index, length, data);
}

int usb_device_driver_issue_intr_transfer(usb_input_device_t *device, int out, void *data,
                                          u16 length)
{
    return usb_hid_v5_intr_transfer(device->host_fd, device->dev_id, out, length, data);
}

int usb_device_driver_issue_ctrl_transfer_async(usb_input_device_t *device, u8 requesttype,
                                                u8 request, u16 value, u16 index, void *data,
                                                u16 length)
{
    return usb_hid_v5_ctrl_transfer_async(device->host_fd, device->dev_id, requesttype, request,
                                          value, index, length, data, queue_id,
                                          &device->usb_async_resp_msg);
}

int usb_device_driver_issue_intr_transfer_async(usb_input_device_t *device, int out, void *data,
                                                u16 length)
{
    return usb_hid_v5_intr_transfer_async(device->host_fd, device->dev_id, out, length, data,
                                          queue_id, &device->usb_async_resp_msg);
}

int usb_device_driver_set_timer(usb_input_device_t *device, int time_us, int repeat_time_us)
{
    int rc;
    if (device->timer_id > 0) {
        os_stop_timer(device->timer_id);
        rc = os_restart_timer(device->timer_id, time_us, repeat_time_us);
    } else {
        device->timer_id =
            os_create_timer(time_us, repeat_time_us, queue_id, (s32)&device->timer_id);
        rc = device->timer_id;
    }
    return rc;
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

    if (device->driver->init)
        return device->driver->init(device, device->vid, device->pid);

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

static void handle_device_change_reply(int host_fd, areply *reply)
{
    usb_devdesc udd;
    usb_input_device_t *device;
    const usb_device_driver_t *driver;
    u16 vid, pid;
    u32 dev_id;
    int ret;
    bool found;

    LOG_DEBUG("Device change, #Attached devices: %d\n", reply->result);

    if (reply->result < 0)
        return;

    /* First look for disconnections */
    for (int i = 0; i < ARRAY_SIZE(usb_devices); i++) {
        device = &usb_devices[i];
        if (!device->valid)
            continue;

        found = false;
        for (int j = 0; j < reply->result; j++) {
            if (device->dev_id == device_change_devices[j].device_id) {
                found = true;
                break;
            }
        }

        /* Oops, it got disconnected */
        if (!found) {
            LOG_DEBUG("Device with VID: 0x%04x, PID: 0x%04x, dev_id: 0x%x got disconnected\n",
                      device->vid, device->pid, device->dev_id);

            if (device->driver->disconnect)
                ret = device->driver->disconnect(device);
            /* Tell the fake Wiimote manager we got an input device removal */
            input_devices_remove(device->input_device);
            /* Set this device as not valid */
            device->valid = false;
        }
    }

    /* Now look for new connections */
    for (int i = 0; i < reply->result; i++) {
        vid = device_change_devices[i].vid;
        pid = device_change_devices[i].pid;
        dev_id = device_change_devices[i].device_id;
        LOG_DEBUG("[%d] VID: 0x%04x, PID: 0x%04x, dev_id: 0x%x\n", i, vid, pid, dev_id);

        /* Check if we already have that device (same dev_id) connected */
        if (is_usb_device_connected(dev_id))
            continue;

        /* Find if we have a driver for that VID/PID */
        driver = get_usb_device_driver_for(vid, pid);
        if (!driver)
            continue;

        /* Get an empty device slot */
        device = get_free_usb_device_slot();
        if (!device)
            break;

        /* Now we can attach it to take ownership! */
        ret = usb_hid_v5_attach(host_fd, dev_id);
        if (ret != IOS_OK)
            continue;

        /* We must resume the USB device before interacting with it */
        ret = usb_hid_v5_suspend_resume(host_fd, dev_id, 1, 0);
        if (ret != IOS_OK) {
            usb_hid_v5_release(host_fd, dev_id);
            continue;
        }

        /* We must read the USB device descriptor before interacting with the device */
        ret = usb_hid_v5_get_descriptors(host_fd, dev_id, &udd);
        if (ret != IOS_OK) {
            usb_hid_v5_release(host_fd, dev_id);
            continue;
        }

        /* Get a fake Wiimote from the manager */
        if (!input_devices_add(device, &input_device_usb_ops, &device->input_device)) {
            usb_hid_v5_release(host_fd, dev_id);
            continue;
        }

        /* We have ownership, populate the device info */
        device->vid = vid;
        device->pid = pid;
        device->host_fd = host_fd;
        device->dev_id = dev_id;
        device->driver = driver;
        /* We will get a fake Wiimote assigneed at the init() callback */
        device->wiimote = NULL;
        device->suspended = false;
        device->valid = true;
        device->timer_id = -1;
    }

    ret = os_ioctl_async(host_fd, USBV5_IOCTL_ATTACHFINISH, NULL, 0, NULL, 0, queue_id,
                         MESSAGE_ATTACHFINISH);
    LOG_DEBUG("ioctl(ATTACHFINISH): %d\n", ret);
}

static int usb_hid_worker(void *arg)
{
    u32 ver[8] ATTRIBUTE_ALIGN(32);
    usb_input_device_t *device;
    int ret;

    LOG_DEBUG("usb_hid_worker thread started\n");

    for (int i = 0; i < ARRAY_SIZE(usb_devices); i++)
        usb_devices[i].valid = false;

    /* USB_HID supports 16 handles, libogc uses handle 0, so we use handle 15...*/
    ret = os_open("/dev/usb/hid", 15);
    if (ret < 0)
        return ret;
    host_fd = ret;

    ret = os_ioctl(host_fd, USBV5_IOCTL_GETVERSION, NULL, 0, ver, sizeof(ver));
    if (ret < 0)
        return ret;

    /* We only support USBv5 for now */
    if (ver[0] != 0x50001)
        return IOS_EINVAL;

    ret = os_ioctl_async(host_fd, USBV5_IOCTL_GETDEVICECHANGE, NULL, 0, device_change_devices,
                         sizeof(device_change_devices), queue_id, MESSAGE_DEVCHANGE);

    while (1) {
        areply *message;

        /* Wait for a message from USB devices */
        ret = os_message_queue_receive(queue_id, (void *)&message, IOS_MESSAGE_BLOCK);
        if (ret != IOS_OK)
            continue;

        if (message == MESSAGE_DEVCHANGE) {
            handle_device_change_reply(host_fd, message);
        } else if (message == MESSAGE_ATTACHFINISH) {
            ret =
                os_ioctl_async(host_fd, USBV5_IOCTL_GETDEVICECHANGE, NULL, 0, device_change_devices,
                               sizeof(device_change_devices), queue_id, MESSAGE_DEVCHANGE);
        } else {
            /* Find if this is a timer or the reply to a USB async
             * req issued by a device driver */
            for (int i = 0; i < ARRAY_SIZE(usb_devices); i++) {
                device = &usb_devices[i];
                if (!device->valid)
                    continue;
                if (message == &device->usb_async_resp_msg && device->driver->usb_async_resp) {
                    device->driver->usb_async_resp(device);
                } else if (message == (areply *)&device->timer_id && device->driver->timer) {
                    bool keep = device->driver->timer(device);
                    if (!keep) {
                        os_destroy_timer(device->timer_id);
                        device->timer_id = -1;
                    }
                }
            }
        }
    }

    return 0;
}

int usb_hid_init(void)
{
    int ret;

    ret = os_message_queue_create(queue_data, ARRAY_SIZE(queue_data));
    if (ret < 0)
        return ret;
    queue_id = ret;

    /* Worker USB HID thread that receives and dispatches async events */
    ret = os_thread_create(usb_hid_worker, NULL, &worker_thread_stack[sizeof(worker_thread_stack)],
                           sizeof(worker_thread_stack), 0, 0);
    if (ret < 0)
        return ret;
    os_thread_continue(ret);

    return 0;
}
