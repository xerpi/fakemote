#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ipc.h"
#include "syscalls.h"
#include "usb.h"
#include "usb_backend.h"
#include "utils.h"

/* Maximum number of connected USB controllers. Increase this if needed. */
#define MAX_ACTIVE_DEVICES 2

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
#define USB_MAX_DEVICES      32
#define MAX_ACTIVE_TRANSFERS 2

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

struct egc_usb_device_t {
    void *priv;
    /* VID and PID */
    /* TODO: remove, read them from "desc" */
    u16 vid;
    u16 pid;
    /* Used to communicate with Wii's USB module */
    int host_fd;
    u32 dev_id;
    /* Timer ID (-1 if unset), its address used as timer cookie */
    int timer_id;
    egc_timer_cb timer_callback;
    egc_usb_devdesc_t desc;
};

typedef struct egc_usb_device_entry_t {
    s32 device_id;
    u16 vid;
    u16 pid;
    u32 token;
} egc_usb_device_entry_t;

typedef struct {
    u8 buffer[128] ATTRIBUTE_ALIGN(32);
    areply reply;
    struct egc_usb_transfer_t t;
    egc_transfer_cb callback;
} ogc_transfer_t;

static egc_usb_device_t usb_devices[MAX_ACTIVE_DEVICES] ATTRIBUTE_ALIGN(32);
static egc_usb_device_entry_t device_change_devices[USB_MAX_DEVICES] ATTRIBUTE_ALIGN(32);
static ogc_transfer_t s_transfers[MAX_ACTIVE_TRANSFERS] ATTRIBUTE_ALIGN(32);
static int host_fd = -1;
static u8 worker_thread_stack[1024] ATTRIBUTE_ALIGN(32);
static u32 queue_data[32] ATTRIBUTE_ALIGN(32);
static int queue_id = -1;
static egc_usb_event_cb s_event_handler;

/* Async notification messages */
static areply notification_messages[2] = { 0 };
#define MESSAGE_DEVCHANGE    &notification_messages[0]
#define MESSAGE_ATTACHFINISH &notification_messages[1]

static inline egc_usb_device_t *get_usb_device_for_dev_id(u32 dev_id)
{
    for (int i = 0; i < ARRAY_SIZE(usb_devices); i++) {
        if (usb_devices[i].dev_id == dev_id)
            return &usb_devices[i];
    }

    return NULL;
}

static inline egc_usb_device_t *get_free_usb_device_slot(void)
{
    for (int i = 0; i < ARRAY_SIZE(usb_devices); i++) {
        if (usb_devices[i].dev_id == 0)
            return &usb_devices[i];
    }

    return NULL;
}

static ogc_transfer_t *get_free_transfer(void)
{
    for (int i = 0; i < ARRAY_SIZE(s_transfers); i++) {
        if (!s_transfers[i].t.device)
            return &s_transfers[i];
    }

    return NULL;
}

static inline bool is_usb_device_connected(u32 dev_id)
{
    return get_usb_device_for_dev_id(dev_id) != NULL;
}

static int usb_hid_v5_get_descriptors(int host_fd, u32 dev_id, egc_usb_devdesc_t *udd)
{
    u32 inbuf[8] ATTRIBUTE_ALIGN(32);
    u8 outbuf[96] ATTRIBUTE_ALIGN(32);

    /* Setup buffer */
    memset(inbuf, 0, sizeof(inbuf));
    inbuf[0] = dev_id;
    inbuf[2] = 0;

    /* Get device parameters */
    int rc =
        os_ioctl(host_fd, USBV5_IOCTL_GETDEVPARAMS, inbuf, sizeof(inbuf), outbuf, sizeof(outbuf));
    if (udd)
        memcpy(udd, outbuf, sizeof(*udd));
    return rc;
}

static inline void build_ctrl_transfer(struct usb_hid_v5_transfer *transfer, ogc_transfer_t *t,
                                       u8 bmRequest, u16 wValue, u16 wIndex)
{
    memset(transfer, 0, sizeof(*transfer));
    transfer->dev_id = t->t.device->dev_id;
    transfer->ctrl.bmRequestType = t->t.endpoint;
    transfer->ctrl.bmRequest = bmRequest;
    transfer->ctrl.wValue = wValue;
    transfer->ctrl.wIndex = wIndex;
}

static inline int usb_hid_v5_ctrl_transfer_async(ogc_transfer_t *t, u8 bmRequest, u16 wValue,
                                                 u16 wIndex, int queue_id)
{
    struct usb_hid_v5_transfer transfer ATTRIBUTE_ALIGN(32);
    ioctlv vectors[2];
    int out = !(t->t.endpoint & EGC_USB_ENDPOINT_IN);

    build_ctrl_transfer(&transfer, t, bmRequest, wValue, wIndex);

    vectors[0].data = &transfer;
    vectors[0].len = sizeof(transfer);
    vectors[1].data = t->t.data;
    vectors[1].len = t->t.length;

    return os_ioctlv_async(t->t.device->host_fd, USBV5_IOCTL_CTRLMSG, 1 + out, 1 - out, vectors,
                           queue_id, &t->reply);
}

static inline int usb_hid_v5_intr_transfer_async(ogc_transfer_t *t, int queue_id)
{
    struct usb_hid_v5_transfer transfer ATTRIBUTE_ALIGN(32);
    ioctlv vectors[2];

    int out = !(t->t.endpoint & EGC_USB_ENDPOINT_IN);
    memset(&transfer, 0, sizeof(transfer));
    transfer.dev_id = t->t.device->dev_id;
    transfer.intr.out = out;

    vectors[0].data = &transfer;
    vectors[0].len = sizeof(transfer);
    vectors[1].data = t->t.data;
    vectors[1].len = t->t.length;

    return os_ioctlv_async(t->t.device->host_fd, USBV5_IOCTL_INTRMSG, 1 + out, 1 - out, vectors,
                           queue_id, &t->reply);
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

static const egc_usb_transfer_t *ogc_ctrl_transfer_async(egc_usb_device_t *device, u8 requesttype,
                                                         u8 request, u16 value, u16 index,
                                                         void *data, u16 length,
                                                         egc_transfer_cb callback)
{
    ogc_transfer_t *t = get_free_transfer();
    if (!t)
        return NULL;

    assert(length <= sizeof(t->buffer));
    if (length > 0) {
        memcpy(t->buffer, data, length);
    } else if (requesttype & EGC_USB_ENDPOINT_IN) {
        length = sizeof(t->buffer);
    }
    t->t.device = device;
    t->t.transfer_type = EGC_USB_TRANSFER_CONTROL;
    t->t.endpoint = requesttype;
    t->t.length = length;
    t->t.data = t->buffer;
    t->callback = callback;
    int rc = usb_hid_v5_ctrl_transfer_async(t, request, value, index, queue_id);
    if (rc < 0) {
        t->t.device = NULL; /* Mark as unused */
        t = NULL;
    }
    return &t->t;
}

static const egc_usb_transfer_t *ogc_intr_transfer_async(egc_usb_device_t *device, u8 endpoint,
                                                         void *data, u16 length,
                                                         egc_transfer_cb callback)
{
    ogc_transfer_t *t = get_free_transfer();
    if (!t)
        return NULL;

    assert(length <= sizeof(t->buffer));
    if (length > 0) {
        memcpy(t->buffer, data, length);
    } else if (endpoint & EGC_USB_ENDPOINT_IN) {
        length = sizeof(t->buffer);
    }
    t->t.device = device;
    t->t.transfer_type = EGC_USB_TRANSFER_INTERRUPT;
    t->t.endpoint = endpoint;
    t->t.length = length;
    t->t.data = t->buffer;
    t->callback = callback;
    int rc = usb_hid_v5_intr_transfer_async(t, queue_id);
    if (rc < 0) {
        t->t.device = NULL; /* Mark as unused */
        return NULL;
    }
    return &t->t;
}

static int ogc_set_timer(egc_usb_device_t *device, int time_us, int repeat_time_us,
                         egc_timer_cb callback)
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
    device->timer_callback = callback;
    return rc;
}

static void handle_device_change_reply(int host_fd, areply *reply)
{
    egc_usb_device_t *device;
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
        if (device->dev_id == 0)
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

            s_event_handler(device, EGC_USB_EVENT_DEVICE_REMOVED);
            /* Set this device as not valid */
            memset(device, 0, sizeof(*device));
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
        ret = usb_hid_v5_get_descriptors(host_fd, dev_id, &device->desc);
        if (ret != IOS_OK) {
            usb_hid_v5_release(host_fd, dev_id);
            continue;
        }

        /* We have ownership, populate the device info */
        device->vid = vid;
        device->pid = pid;
        device->host_fd = host_fd;
        device->dev_id = dev_id;
        device->timer_id = -1;
        device->timer_callback = NULL;

        ret = s_event_handler(device, EGC_USB_EVENT_DEVICE_ADDED, vid, pid);
        if (ret != 0) {
            usb_hid_v5_release(host_fd, dev_id);
            continue;
        }
    }

    ret = os_ioctl_async(host_fd, USBV5_IOCTL_ATTACHFINISH, NULL, 0, NULL, 0, queue_id,
                         MESSAGE_ATTACHFINISH);
    LOG_DEBUG("ioctl(ATTACHFINISH): %d\n", ret);
}

static int usb_hid_worker(void *arg)
{
    u32 ver[8] ATTRIBUTE_ALIGN(32);
    egc_usb_device_t *device;
    int ret;

    LOG_DEBUG("usb_hid_worker thread started\n");

    for (int i = 0; i < ARRAY_SIZE(usb_devices); i++)
        usb_devices[i].dev_id = 0;

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
        void *message;

        /* Wait for a message from USB devices */
        ret = os_message_queue_receive(queue_id, &message, IOS_MESSAGE_BLOCK);
        if (ret != IOS_OK)
            continue;

        if (message == MESSAGE_DEVCHANGE) {
            handle_device_change_reply(host_fd, message);
        } else if (message == MESSAGE_ATTACHFINISH) {
            ret =
                os_ioctl_async(host_fd, USBV5_IOCTL_GETDEVICECHANGE, NULL, 0, device_change_devices,
                               sizeof(device_change_devices), queue_id, MESSAGE_DEVCHANGE);
        } else if ((areply *)message >= &s_transfers[0].reply &&
                   (areply *)message <= &s_transfers[ARRAY_SIZE(s_transfers) - 1].reply) {
            /* It's a transfer reply */
            for (int i = 0; i < ARRAY_SIZE(s_transfers); i++) {
                ogc_transfer_t *transfer = &s_transfers[i];
                if ((areply *)message == &transfer->reply) {
                    if (transfer->t.device->dev_id != 0 && transfer->callback) {
                        if (transfer->reply.result >= 0) {
                            transfer->t.length = transfer->reply.result;
                            transfer->t.status = EGC_USB_TRANSFER_STATUS_COMPLETED;
                        } else {
                            transfer->t.length = 0;
                            transfer->t.status = EGC_USB_TRANSFER_STATUS_ERROR;
                        }
                        transfer->callback(&transfer->t);
                    }
                    /* Mark the transfer as unused */
                    transfer->t.status = EGC_USB_TRANSFER_STATUS_UNSET;
                    transfer->t.device = NULL;
                    break;
                }
            }
        } else {
            /* Find if this is a timer */
            for (int i = 0; i < ARRAY_SIZE(usb_devices); i++) {
                device = &usb_devices[i];
                if (device->dev_id == 0)
                    continue;
                if (message == &device->timer_id) {
                    bool keep = device->timer_callback(device);
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

static int ogc_init(egc_usb_event_cb event_handler)
{
    int ret;

    s_event_handler = event_handler;

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

static const egc_usb_devdesc_t *ogc_get_device_descriptor(egc_usb_device_t *device)
{
    return &device->desc;
}

const egc_usb_backend_t _egc_usb_backend = {
    .init = ogc_init,
    .get_device_descriptor = ogc_get_device_descriptor,
    .ctrl_transfer_async = ogc_ctrl_transfer_async,
    .intr_transfer_async = ogc_intr_transfer_async,
    .set_timer = ogc_set_timer,
};
