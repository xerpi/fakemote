#ifndef EGC_PLATFORM_H
#define EGC_PLATFORM_H

#include "egc.h"
#include "usb_backend.h"

typedef enum egc_event_e {
    EGC_EVENT_DEVICE_ADDED,
    EGC_EVENT_DEVICE_REMOVED,
} egc_event_e;

typedef int (*egc_event_cb)(egc_input_device_t *device, egc_event_e event, ...);
/* Return false if the timer must be destroyed */
typedef bool (*egc_timer_cb)(egc_input_device_t *device);

typedef struct egc_platform_backend_t {
    egc_usb_backend_t usb;
    /* TODO: add BT backend */

    /* Initializes the backend. The backend should call
     * egc_event_device_added() for each connected device. */
    int (*init)(egc_event_cb event_handler);

    /* Allocate a struct for the device description */
    egc_device_description_t *(*alloc_desc)(egc_input_device_t *device);

    /* Returns < 0 if the timer cannot be set. In general, we should not assume
     * that the platform supports more than a timer per device at a time. */
    int (*set_timer)(egc_input_device_t *device, int time_us, int repeat_time_us,
                     egc_timer_cb callback);
    int (*report_input)(egc_input_device_t *device, const egc_input_state_t *state);

    /* All callbacks should be invoked in the context of this call */
    int (*handle_events)(void);
} egc_platform_backend_t;

extern const egc_platform_backend_t _egc_platform_backend;

#endif
