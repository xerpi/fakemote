#ifndef EGC_USB_BACKEND_H
#define EGC_USB_BACKEND_H

#include "usb.h"

typedef struct egc_usb_backend_t egc_usb_backend_t;

/* Interface for platform-specific USB backends. */
struct egc_usb_backend_t {
    /* This should immediately return, and just return a pointer to data
     * already retrieved. */
    const egc_usb_devdesc_t *(*get_device_descriptor)(egc_input_device_t *device);

    /* Add methods to return descriptors for configurations, HID, endpoints,
     * etc. - when needed. */

    /* The egc_usb_transfer structure is allocated by the backend and freed
     * after the callback has been invoked. */
    const egc_usb_transfer_t *(*ctrl_transfer_async)(egc_input_device_t *device, u8 requesttype,
                                                     u8 request, u16 value, u16 index, void *data,
                                                     u16 length, egc_transfer_cb callback);
    const egc_usb_transfer_t *(*intr_transfer_async)(egc_input_device_t *device, u8 endpoint,
                                                     void *data, u16 length,
                                                     egc_transfer_cb callback);
    const egc_usb_transfer_t *(*bulk_transfer_async)(egc_input_device_t *device, u8 endpoint,
                                                     void *data, u16 length,
                                                     egc_transfer_cb callback);
};

#endif
