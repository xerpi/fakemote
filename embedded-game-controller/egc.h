#ifndef EGC_H
#define EGC_H

#include "egc_types.h"

#include <assert.h>

#define EGC_MAX_ACCELEROMETERS 2
#define EGC_MAX_TOUCH_POINTS 2

typedef struct egc_input_device_t egc_input_device_t;
typedef struct egc_device_driver_t egc_device_driver_t;

/* Let's reserve the higher values for special functions */
#define EGC_RUMBLE_MAX 0x7fffffff
#define EGC_RUMBLE_OFF 0

typedef enum {
    EGC_DEVICE_TYPE_GAMEPAD,
    EGC_DEVICE_TYPE_GUITAR,
    EGC_DEVICE_TYPE_DRUMS,
} egc_device_type_e;

/* Any similarity with SDL3's SDL_GamepadButton is completely *not* accidental. */
typedef enum {
    EGC_GAMEPAD_BUTTON_SOUTH,           /**< Bottom face button (e.g. Xbox A button) */
    EGC_GAMEPAD_BUTTON_EAST,            /**< Right face button (e.g. Xbox B button) */
    EGC_GAMEPAD_BUTTON_WEST,            /**< Left face button (e.g. Xbox X button) */
    EGC_GAMEPAD_BUTTON_NORTH,           /**< Top face button (e.g. Xbox Y button) */
    EGC_GAMEPAD_BUTTON_BACK,
    EGC_GAMEPAD_BUTTON_GUIDE,
    EGC_GAMEPAD_BUTTON_START,
    EGC_GAMEPAD_BUTTON_LEFT_STICK,
    EGC_GAMEPAD_BUTTON_RIGHT_STICK,
    EGC_GAMEPAD_BUTTON_LEFT_SHOULDER,
    EGC_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    EGC_GAMEPAD_BUTTON_DPAD_UP,
    EGC_GAMEPAD_BUTTON_DPAD_DOWN,
    EGC_GAMEPAD_BUTTON_DPAD_LEFT,
    EGC_GAMEPAD_BUTTON_DPAD_RIGHT,
    EGC_GAMEPAD_BUTTON_MISC1,           /**< Additional button (e.g. Xbox Series X share button, PS5 microphone button, Nintendo Switch Pro capture button, Amazon Luna microphone button, Google Stadia capture button) */
    EGC_GAMEPAD_BUTTON_RIGHT_PADDLE1,   /**< Upper or primary paddle, under your right hand (e.g. Xbox Elite paddle P1) */
    EGC_GAMEPAD_BUTTON_LEFT_PADDLE1,    /**< Upper or primary paddle, under your left hand (e.g. Xbox Elite paddle P3) */
    EGC_GAMEPAD_BUTTON_RIGHT_PADDLE2,   /**< Lower or secondary paddle, under your right hand (e.g. Xbox Elite paddle P2) */
    EGC_GAMEPAD_BUTTON_LEFT_PADDLE2,    /**< Lower or secondary paddle, under your left hand (e.g. Xbox Elite paddle P4) */
    EGC_GAMEPAD_BUTTON_TOUCHPAD,        /**< PS4/PS5 touchpad button */
    EGC_GAMEPAD_BUTTON_MISC2,           /**< Additional button */
    EGC_GAMEPAD_BUTTON_MISC3,           /**< Additional button */
    EGC_GAMEPAD_BUTTON_MISC4,           /**< Additional button */
    EGC_GAMEPAD_BUTTON_MISC5,           /**< Additional button */
    EGC_GAMEPAD_BUTTON_MISC6,           /**< Additional button */
    EGC_GAMEPAD_BUTTON_COUNT
} egc_gamepad_button_e;
/* Each enum value corresponds to a bit in a 32 bit-wide field */
static_assert(EGC_GAMEPAD_BUTTON_COUNT <= 32);

typedef enum {
    EGC_GAMEPAD_AXIS_LEFTX,
    EGC_GAMEPAD_AXIS_LEFTY,
    EGC_GAMEPAD_AXIS_RIGHTX,
    EGC_GAMEPAD_AXIS_RIGHTY,
    EGC_GAMEPAD_AXIS_LEFT_TRIGGER,
    EGC_GAMEPAD_AXIS_RIGHT_TRIGGER,
    EGC_GAMEPAD_AXIS_COUNT
} egc_gamepad_axis_e;

/* Resolution is 4096, therefore the range is +/- 8g */
#define EGC_ACCELEROMETER_RES_PER_G 4096

typedef struct {
    s16 x;
    s16 y;
    s16 z;
} ATTRIBUTE_PACKED egc_accelerometer_t;

/* Range is 0-32767. A negative x means that there is no touch */
#define EGC_GAMEPAD_TOUCH_RES 0x7fff

typedef struct {
    s16 x;
    s16 y;
} ATTRIBUTE_PACKED egc_point_t;

typedef struct egc_input_state_t {
    /* TODO: maybe add a timestamp or a counter, to let the client know if the
     * data was updated? */
    union {
        struct {
            u32 buttons;
            s16 axes[EGC_GAMEPAD_AXIS_COUNT] ATTRIBUTE_ALIGN(2);
            egc_accelerometer_t accelerometer[EGC_MAX_ACCELEROMETERS];
            egc_point_t touch_points[EGC_MAX_TOUCH_POINTS];
        } ATTRIBUTE_PACKED gamepad;
        /* TODO: add struct for guitar and drums */
    };
} egc_input_state_t;

typedef enum {
    EGC_CONNECTION_DISCONNECTED,
    EGC_CONNECTION_USB,
    EGC_CONNECTION_BT,
} egc_connection_e;

typedef struct egc_device_description_t {
    u16 vendor_id;
    u16 product_id;
    u32 available_buttons;    /* bitmask indexed by egc_gamepad_button_e */
    u32 available_axes;       /* bitmask indexed by egc_gamepad_axis_e */
    egc_device_type_e type;
    u8 num_touch_points;
    u8 num_leds;
    u8 num_accelerometers;
    bool has_rumble;
} ATTRIBUTE_PACKED egc_device_description_t;

#define EGC_INPUT_DEVICE_PRIVATE_DATA_SIZE 64

struct egc_input_device_t {
    const egc_device_description_t *desc;
    egc_input_state_t state ATTRIBUTE_ALIGN(4);
    egc_connection_e connection;
    bool suspended;
    /* The following fields are for EGC's internal use only */
    const egc_device_driver_t *driver;
    u8 private_data[EGC_INPUT_DEVICE_PRIVATE_DATA_SIZE];
} ATTRIBUTE_PACKED;

typedef void (*egc_input_device_cb)(egc_input_device_t *device, void *userdata);

int egc_initialize(egc_input_device_cb added_cb,
                   egc_input_device_cb removed_cb,
                   void *userdata);
int egc_input_device_suspend(egc_input_device_t *device);
int egc_input_device_resume(egc_input_device_t *device);
/* led_state is a bit mask of the active leds */
int egc_input_device_set_leds(egc_input_device_t *device, u32 led_state);
int egc_input_device_set_rumble(egc_input_device_t *device, u32 intensity);

/* Fetch events and invoke callbacks. */
int egc_handle_events(void);

#endif
