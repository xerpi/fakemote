#ifndef EGC_USB_BACKEND_H
#define EGC_USB_BACKEND_H

#include "usb.h"

typedef struct egc_usb_backend_t egc_usb_backend_t;

typedef enum egc_usb_event_e {
    EGC_USB_EVENT_DEVICE_ADDED,
    EGC_USB_EVENT_DEVICE_REMOVED,
} egc_usb_event_e;

/* NOTE: it is allowed to call this callback from a separate thread, as long as
 * all events related to the same device will be invoked by the same thread.
 * The usb-game-controller library will also then invoke the methods in the
 * egc_usb_backend structure from the same thread. */
typedef int (*egc_usb_event_cb)(egc_usb_device_t *device, egc_usb_event_e event, ...);
/* Return false if the timer must be destroyed */
typedef bool (*egc_timer_cb)(egc_usb_device_t *device);

/* Interface for platform-specific USB backends. */
struct egc_usb_backend_t {
    /* Initializes the backend. The backend should call
     * egc_event_device_added() for each connected device. */
    int (*init)(egc_usb_event_cb event_handler);

    /* This should immediately return, and just return a pointer to data
     * already retrieved. */
    const egc_usb_devdesc_t *(*get_device_descriptor)(egc_usb_device_t *device);

    /* Add methods to return descriptors for configurations, HID, endpoints,
     * etc. - when needed. */

    /* The egc_usb_transfer structure is allocated by the backend and freed
     * after the callback has been invoked. */
    const egc_usb_transfer_t *(*ctrl_transfer_async)(egc_usb_device_t *device, u8 requesttype,
                                                     u8 request, u16 value, u16 index, void *data,
                                                     u16 length, egc_transfer_cb callback);
    const egc_usb_transfer_t *(*intr_transfer_async)(egc_usb_device_t *device, u8 endpoint,
                                                     void *data, u16 length,
                                                     egc_transfer_cb callback);
    const egc_usb_transfer_t *(*bulk_transfer_async)(egc_usb_device_t *device, u8 endpoint,
                                                     void *data, u16 length,
                                                     egc_transfer_cb callback);

    /* Returns < 0 if the timer cannot be set. In general, we should not assume
     * that the platform supports more than a timer per device at a time. */
    int (*set_timer)(egc_usb_device_t *device, int time_us, int repeat_time_us,
                     egc_timer_cb callback);
};

extern const egc_usb_backend_t _egc_usb_backend;

#endif
