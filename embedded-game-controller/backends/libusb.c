#include <assert.h>
#include <libusb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "utils.h"

/* Maximum number of connected USB controllers. Increase this if needed. */
#define MAX_ACTIVE_DEVICES 8

typedef struct {
    /* This must be the first member, since we use it for casting */
    egc_input_device_t pub;
    libusb_device_handle *handle;
    egc_device_description_t desc;
    egc_usb_devdesc_t usbdesc;
    /* Timer ID (-1 if unset), its address used as timer cookie */
    int64_t timer_us;
    int64_t repeat_timer_us;
    egc_timer_cb timer_callback;
} lu_device_t;

typedef struct {
    egc_usb_transfer_t t;
    struct libusb_transfer *usb;
    egc_transfer_cb callback;
    u8 buffer[128];
} lu_transfer_t;

static libusb_context *s_libusb_ctx = NULL;
static lu_device_t s_devices[MAX_ACTIVE_DEVICES];
static egc_event_cb s_event_handler;

static inline lu_device_t *lu_device_from_input_device(egc_input_device_t *input_device)
{
    return (lu_device_t *)input_device;
}

static inline lu_transfer_t *lu_transfer_from_libusb(struct libusb_transfer *t)
{
    return t->user_data;
}

static inline lu_transfer_t *lu_transfer_from_egc(egc_usb_transfer_t *t)
{
    return (lu_transfer_t *)((u8*)t - offsetof(lu_transfer_t, t));
}

static int64_t timespec_to_us(const struct timespec *ts)
{
    return ts->tv_sec * 1000000 + ts->tv_nsec / 1000;
}

static inline lu_device_t *get_free_device_slot(void)
{
    for (int i = 0; i < ARRAY_SIZE(s_devices); i++) {
        if (s_devices[i].pub.connection == EGC_CONNECTION_DISCONNECTED)
            return &s_devices[i];
    }

    return NULL;
}

static void transfer_cb(struct libusb_transfer *transfer)
{
    lu_transfer_t *t = lu_transfer_from_libusb(transfer);
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        t->t.status = EGC_USB_TRANSFER_STATUS_COMPLETED;
    } else {
        t->t.status = EGC_USB_TRANSFER_STATUS_ERROR;
    }
    if (t->callback) t->callback(&t->t);
    free(t);
}

static const egc_usb_transfer_t *lu_ctrl_transfer_async(egc_input_device_t *input_device, u8 requesttype,
                                                         u8 request, u16 value, u16 index,
                                                         void *data, u16 length,
                                                         egc_transfer_cb callback)
{
    lu_device_t *device = lu_device_from_input_device(input_device);
    lu_transfer_t *t;
    u8 *buffer;

    t = malloc(sizeof(lu_transfer_t));
    if (!t) return NULL;

    buffer = t->buffer;
    t->usb = libusb_alloc_transfer(0);

    libusb_fill_control_setup(buffer, requesttype, request, value, index, length);
    memcpy(buffer + 8, data, length);

    libusb_fill_control_transfer(t->usb, device->handle, buffer, transfer_cb, t, 3000);
    t->t.device = input_device;
    t->t.transfer_type = EGC_USB_TRANSFER_CONTROL;
    t->t.status = EGC_USB_TRANSFER_STATUS_UNSET;
    t->t.endpoint = requesttype;
    t->t.length = length;
    t->t.data = buffer + 8;
    t->callback = callback;
    int rc = libusb_submit_transfer(t->usb);
    if (rc != LIBUSB_SUCCESS) {
        LOG_INFO("Transfer failed: %d\n", rc);
        libusb_free_transfer(t->usb);
        free(t);
        return NULL;
    }

    return &t->t;
}

static const egc_usb_transfer_t *lu_intr_transfer_async(egc_input_device_t *input_device, u8 endpoint,
                                                         void *data, u16 length,
                                                         egc_transfer_cb callback)
{
    lu_device_t *device = lu_device_from_input_device(input_device);
    lu_transfer_t *t;
    u8 *buffer;

    t = malloc(sizeof(lu_transfer_t) + 8 + length);
    if (!t) return NULL;

    buffer = t->buffer;
    t->usb = libusb_alloc_transfer(0);

    if (data)
        memcpy(buffer, data, length);
    if (endpoint & LIBUSB_ENDPOINT_IN) {
        length = sizeof(t->buffer);
    }

    libusb_fill_interrupt_transfer(t->usb, device->handle, endpoint, buffer, length, transfer_cb, t, 3000);

    t->t.device = input_device;
    t->t.transfer_type = EGC_USB_TRANSFER_INTERRUPT;
    t->t.endpoint = endpoint;
    t->t.length = length;
    t->t.data = buffer;
    t->callback = callback;
    int rc = libusb_submit_transfer(t->usb);
    if (rc != LIBUSB_SUCCESS) {
        LOG_INFO("Transfer failed: %d\n", rc);
        libusb_free_transfer(t->usb);
        free(t);
        return NULL;
    }
    return &t->t;
}

static int lu_set_timer(egc_input_device_t *input_device, int time_us, int repeat_time_us,
                        egc_timer_cb callback)
{
    lu_device_t *device = lu_device_from_input_device(input_device);

    if (time_us > 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        int64_t now = timespec_to_us(&ts);
        device->timer_us = now + time_us;
    } else {
        device->timer_us = 0;
    }
    device->repeat_timer_us = repeat_time_us;
    device->timer_callback = callback;
    return 0;
}

static int lu_report_input(egc_input_device_t *device, const egc_input_state_t *state)
{
    memcpy(&device->state, state, sizeof(*state));
    return 0;
}

static int on_device_added(libusb_context *ctx, libusb_device *dev,
                           libusb_hotplug_event event, void *user_data)
{
    struct libusb_device_descriptor desc;
    lu_device_t *device;

    int rc = libusb_get_device_descriptor(dev, &desc);
    if (rc != LIBUSB_SUCCESS) return 0;

    LOG_DEBUG("Device attached: %04x:%04x\n", desc.idVendor, desc.idProduct);

    /* Get an empty device slot */
    device = get_free_device_slot();
    if (!device) return 0;

    rc = libusb_open(dev, &device->handle);
    if (rc != LIBUSB_SUCCESS) return 0;

    libusb_set_auto_detach_kernel_driver(device->handle, 1);
    rc = libusb_claim_interface(device->handle, 0);
    if (rc != LIBUSB_SUCCESS) {
        LOG_INFO("Failed to claim USB interface (%d)\n", rc);
        return 0;
    }
    memcpy(&device->usbdesc, &desc, sizeof(device->usbdesc));
    device->desc.vendor_id = desc.idVendor;
    device->desc.product_id = desc.idProduct;
    device->timer_us = 0;
    device->repeat_timer_us = 0;
    device->timer_callback = NULL;
    device->pub.connection = EGC_CONNECTION_USB;

    s_event_handler(&device->pub, EGC_EVENT_DEVICE_ADDED, desc.idVendor, desc.idProduct);
    return 0;
}

static int on_device_removed(libusb_context *ctx, libusb_device *dev,
                             libusb_hotplug_event event, void *user_data)
{
    lu_device_t *device;

    LOG_DEBUG("Device removed\n");
    for (int i = 0; i < ARRAY_SIZE(s_devices); i++) {
        device = &s_devices[i];
        if (device->pub.connection == EGC_CONNECTION_USB &&
            libusb_get_device(device->handle) == dev) {
            s_event_handler(&device->pub, EGC_EVENT_DEVICE_REMOVED);
            libusb_close(device->handle);
            device->handle = NULL;
            device->pub.connection = EGC_CONNECTION_DISCONNECTED;
            break;
        }
    }
    return 0;
}

static int lu_init(egc_event_cb event_handler)
{
    int rc;

    s_event_handler = event_handler;

    rc = libusb_init_context(&s_libusb_ctx, NULL, 0);
    if (rc != LIBUSB_SUCCESS) return rc;

    rc = libusb_hotplug_register_callback(s_libusb_ctx, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
                                          LIBUSB_HOTPLUG_ENUMERATE,
                                          LIBUSB_HOTPLUG_MATCH_ANY,
                                          LIBUSB_HOTPLUG_MATCH_ANY,
                                          LIBUSB_HOTPLUG_MATCH_ANY,
                                          on_device_added, NULL, NULL);
    if (LIBUSB_SUCCESS != rc) {
        libusb_exit(NULL);
        return rc;
    }

    rc = libusb_hotplug_register_callback(s_libusb_ctx, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                                          LIBUSB_HOTPLUG_ENUMERATE,
                                          LIBUSB_HOTPLUG_MATCH_ANY,
                                          LIBUSB_HOTPLUG_MATCH_ANY,
                                          LIBUSB_HOTPLUG_MATCH_ANY,
                                          on_device_removed, NULL, NULL);
    if (LIBUSB_SUCCESS != rc) {
        libusb_exit(NULL);
        return rc;
    }

    return 0;
}

static egc_device_description_t *lu_alloc_desc(egc_input_device_t *input_device)
{
    lu_device_t *device = lu_device_from_input_device(input_device);
    input_device->desc = &device->desc;
    return &device->desc;
}

static const egc_usb_devdesc_t *lu_get_device_descriptor(egc_input_device_t *input_device)
{
    lu_device_t *device = lu_device_from_input_device(input_device);
    return &device->usbdesc;
}

static int lu_handle_events()
{
    struct timeval tv = { 0, 0};
    libusb_handle_events_timeout_completed(s_libusb_ctx, &tv, NULL);

    /* Check for expired timers */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now = timespec_to_us(&ts);
    for (int i = 0; i < ARRAY_SIZE(s_devices); i++) {
        lu_device_t *device = &s_devices[i];
        if (device->pub.connection == EGC_CONNECTION_DISCONNECTED)
            continue;
        if (device->timer_us <= 0) continue;

        if (now < device->timer_us) continue;

        /* timer has expired, invoke the callback */
        bool keep = device->timer_callback(&device->pub);
        if (keep) {
            /* Set the repeat timer */
            device->timer_us = now + device->repeat_timer_us;
        } else {
            device->timer_us = 0;
            device->repeat_timer_us = 0;
        }
    }
    return 0;
}

const egc_platform_backend_t _egc_platform_backend = {
    .usb = {
        .get_device_descriptor = lu_get_device_descriptor,
        .ctrl_transfer_async = lu_ctrl_transfer_async,
        .intr_transfer_async = lu_intr_transfer_async,
    },
    .init = lu_init,
    .alloc_desc = lu_alloc_desc,
    .set_timer = lu_set_timer,
    .report_input = lu_report_input,
    .handle_events = lu_handle_events,
};
