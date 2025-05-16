#include "driver_api.h"
#include "utils.h"

#include <limits.h>

#define SONY_VID 0x054c

#define DS4_TOUCHPAD_W    1920
#define DS4_TOUCHPAD_H    940
#define DS4_ACC_RES_PER_G 8192

struct ds4_input_report {
    u8 report_id;
    u8 left_x;
    u8 left_y;
    u8 right_x;
    u8 right_y;

    u8 triangle : 1;
    u8 circle : 1;
    u8 cross : 1;
    u8 square : 1;
    u8 dpad : 4;

    u8 r3 : 1;
    u8 l3 : 1;
    u8 options : 1;
    u8 share : 1;
    u8 r2 : 1;
    u8 l2 : 1;
    u8 r1 : 1;
    u8 l1 : 1;

    u8 cnt1 : 6;
    u8 tpad : 1;
    u8 ps : 1;

    u8 l_trigger;
    u8 r_trigger;

    u8 cnt2;
    u8 cnt3;

    u8 battery;

    union {
        s16 roll;
        s16 gyro_z;
    };

    union {
        s16 yaw;
        s16 gyro_y;
    };

    union {
        s16 pitch;
        s16 gyro_x;
    };

    s16 accel_x;
    s16 accel_y;
    s16 accel_z;

    u8 unk1[5];

    u8 padding : 1;
    u8 microphone : 1;
    u8 headphones : 1;
    u8 usb_plugged : 1;
    u8 battery_level : 4;

    u8 unk2[2];
    u8 trackpadpackets;
    u8 packetcnt;

    u8 finger1_nactive : 1;
    u8 finger1_id : 7;
    u8 finger1_x_lo;
    u8 finger1_y_lo : 4;
    u8 finger1_x_hi : 4;
    u8 finger1_y_hi;

    u8 finger2_nactive : 1;
    u8 finger2_id : 7;
    u8 finger2_x_lo;
    u8 finger2_y_lo : 4;
    u8 finger2_x_hi : 4;
    u8 finger2_y_hi;
} ATTRIBUTE_PACKED;

enum ds4_buttons_e {
    DS4_BUTTON_TRIANGLE,
    DS4_BUTTON_CIRCLE,
    DS4_BUTTON_CROSS,
    DS4_BUTTON_SQUARE,
    DS4_BUTTON_UP,
    DS4_BUTTON_DOWN,
    DS4_BUTTON_LEFT,
    DS4_BUTTON_RIGHT,
    DS4_BUTTON_R3,
    DS4_BUTTON_L3,
    DS4_BUTTON_OPTIONS,
    DS4_BUTTON_SHARE,
    DS4_BUTTON_R2,
    DS4_BUTTON_L2,
    DS4_BUTTON_R1,
    DS4_BUTTON_L1,
    DS4_BUTTON_TOUCHPAD,
    DS4_BUTTON_PS,
    DS4_BUTTON_COUNT
};

enum ds4_analog_axis_e {
    DS4_ANALOG_AXIS_LEFT_X,
    DS4_ANALOG_AXIS_LEFT_Y,
    DS4_ANALOG_AXIS_RIGHT_X,
    DS4_ANALOG_AXIS_RIGHT_Y,
    DS4_ANALOG_AXIS_COUNT
};

struct ds4_private_data_t {
    u8 led_color[3]; /* 0 - 32 */
    bool rumble_on;
};
static_assert(sizeof(struct ds4_private_data_t) <= EGC_INPUT_DEVICE_PRIVATE_DATA_SIZE);

/* Map each button of the controller to an egc_gamepad_button_e */
static const egc_gamepad_button_e s_button_map[DS4_BUTTON_COUNT] = {
    [DS4_BUTTON_UP] = EGC_GAMEPAD_BUTTON_DPAD_UP,
    [DS4_BUTTON_DOWN] = EGC_GAMEPAD_BUTTON_DPAD_DOWN,
    [DS4_BUTTON_LEFT] = EGC_GAMEPAD_BUTTON_DPAD_LEFT,
    [DS4_BUTTON_RIGHT] = EGC_GAMEPAD_BUTTON_DPAD_RIGHT,
    [DS4_BUTTON_TRIANGLE] = EGC_GAMEPAD_BUTTON_NORTH,
    [DS4_BUTTON_CIRCLE] = EGC_GAMEPAD_BUTTON_EAST,
    [DS4_BUTTON_CROSS] = EGC_GAMEPAD_BUTTON_SOUTH,
    [DS4_BUTTON_SQUARE] = EGC_GAMEPAD_BUTTON_WEST,
    [DS4_BUTTON_L1] = EGC_GAMEPAD_BUTTON_LEFT_SHOULDER,
    [DS4_BUTTON_R1] = EGC_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    [DS4_BUTTON_L2] = EGC_GAMEPAD_BUTTON_LEFT_PADDLE1,
    [DS4_BUTTON_R2] = EGC_GAMEPAD_BUTTON_RIGHT_PADDLE1,
    [DS4_BUTTON_SHARE] = EGC_GAMEPAD_BUTTON_BACK,
    [DS4_BUTTON_OPTIONS] = EGC_GAMEPAD_BUTTON_START,
    [DS4_BUTTON_PS] = EGC_GAMEPAD_BUTTON_GUIDE,
    [DS4_BUTTON_L3] = EGC_GAMEPAD_BUTTON_LEFT_STICK,
    [DS4_BUTTON_R3] = EGC_GAMEPAD_BUTTON_RIGHT_STICK,
    [DS4_BUTTON_TOUCHPAD] = EGC_GAMEPAD_BUTTON_TOUCHPAD,
};

static const egc_device_description_t s_device_description = {
    .vendor_id = SONY_VID,
    // Product ID is set dynamically
    .available_buttons =
        BIT(EGC_GAMEPAD_BUTTON_DPAD_UP) |
        BIT(EGC_GAMEPAD_BUTTON_DPAD_DOWN) |
        BIT(EGC_GAMEPAD_BUTTON_DPAD_LEFT) |
        BIT(EGC_GAMEPAD_BUTTON_DPAD_RIGHT) |
        BIT(EGC_GAMEPAD_BUTTON_NORTH) |
        BIT(EGC_GAMEPAD_BUTTON_EAST) |
        BIT(EGC_GAMEPAD_BUTTON_SOUTH) |
        BIT(EGC_GAMEPAD_BUTTON_WEST) |
        BIT(EGC_GAMEPAD_BUTTON_LEFT_SHOULDER) |
        BIT(EGC_GAMEPAD_BUTTON_RIGHT_SHOULDER) |
        BIT(EGC_GAMEPAD_BUTTON_LEFT_PADDLE1) |
        BIT(EGC_GAMEPAD_BUTTON_RIGHT_PADDLE1) |
        BIT(EGC_GAMEPAD_BUTTON_BACK) |
        BIT(EGC_GAMEPAD_BUTTON_START) |
        BIT(EGC_GAMEPAD_BUTTON_GUIDE) |
        BIT(EGC_GAMEPAD_BUTTON_LEFT_STICK) |
        BIT(EGC_GAMEPAD_BUTTON_RIGHT_STICK),
    .available_axes =
        BIT(EGC_GAMEPAD_AXIS_LEFTX) |
        BIT(EGC_GAMEPAD_AXIS_LEFTY) |
        BIT(EGC_GAMEPAD_AXIS_RIGHTX) |
        BIT(EGC_GAMEPAD_AXIS_RIGHTY),
    .type = EGC_DEVICE_TYPE_GAMEPAD,
    .num_touch_points = 2,
    .num_leds = 4,
    .num_accelerometers = 1,
    .has_rumble = true,
};


static inline int ds4_request_data(egc_input_device_t *device);

static inline u32 ds4_get_buttons(const struct ds4_input_report *report)
{
    u32 mask = 0;

#define MAP(field, button)                                                                         \
    if (report->field)                                                                             \
        mask |= BIT(button);

    MAP(triangle, DS4_BUTTON_TRIANGLE)
    MAP(circle, DS4_BUTTON_CIRCLE)
    MAP(cross, DS4_BUTTON_CROSS)
    MAP(square, DS4_BUTTON_SQUARE)

    if (report->dpad == 0 || report->dpad == 1 || report->dpad == 7)
        mask |= BIT(DS4_BUTTON_UP);
    else if (report->dpad == 3 || report->dpad == 4 || report->dpad == 5)
        mask |= BIT(DS4_BUTTON_DOWN);
    if (report->dpad == 5 || report->dpad == 6 || report->dpad == 7)
        mask |= BIT(DS4_BUTTON_LEFT);
    else if (report->dpad == 1 || report->dpad == 2 || report->dpad == 3)
        mask |= BIT(DS4_BUTTON_RIGHT);

    MAP(r3, DS4_BUTTON_R3)
    MAP(l3, DS4_BUTTON_L3)
    MAP(options, DS4_BUTTON_OPTIONS)
    MAP(share, DS4_BUTTON_SHARE)
    MAP(r2, DS4_BUTTON_R2)
    MAP(l2, DS4_BUTTON_L2)
    MAP(r1, DS4_BUTTON_R1)
    MAP(l1, DS4_BUTTON_L1)
    MAP(tpad, DS4_BUTTON_TOUCHPAD)
    MAP(ps, DS4_BUTTON_PS)
#undef MAP

    return mask;
}

static inline void ds4_get_analog_axis(const struct ds4_input_report *report,
                                       u8 analog_axis[static DS4_ANALOG_AXIS_COUNT])
{
    analog_axis[DS4_ANALOG_AXIS_LEFT_X] = report->left_x;
    analog_axis[DS4_ANALOG_AXIS_LEFT_Y] = 255 - report->left_y;
    analog_axis[DS4_ANALOG_AXIS_RIGHT_X] = report->right_x;
    analog_axis[DS4_ANALOG_AXIS_RIGHT_Y] = 255 - report->right_y;
}

static inline int ds4_set_leds_rumble(egc_input_device_t *device, u8 r, u8 g, u8 b, u8 rumble_small,
                                      u8 rumble_large)
{
    u8 buf[] = {
        0x05, // Report ID
        0x03,
        0x00,
        0x00,
        rumble_small, // Fast motor
        rumble_large, // Slow motor
        r,
        g,
        b,    // RGB
        0x00, // LED on duration
        0x00  // LED off duration
    };

    const egc_usb_transfer_t *transfer =
        egc_device_driver_issue_intr_transfer_async(device, 1, buf, sizeof(buf), NULL);
    return transfer != NULL ? 0 : -1;
}

static void ds4_request_data_cb(egc_usb_transfer_t *transfer)
{
    egc_input_device_t *device = transfer->device;
    struct ds4_input_report *report = (void *)transfer->data;
    struct egc_input_state_t state;

    if (transfer->status == EGC_USB_TRANSFER_STATUS_COMPLETED && report->report_id == 0x01) {
        u32 buttons = ds4_get_buttons(report);
        state.gamepad.buttons = egc_device_driver_map_buttons(buttons, DS4_BUTTON_COUNT, s_button_map);

        u8 axes[DS4_ANALOG_AXIS_COUNT];
        ds4_get_analog_axis(report, axes);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_LEFTX] = egc_u8_to_s16(axes[DS4_ANALOG_AXIS_LEFT_X]);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_LEFTY] = egc_u8_to_s16(axes[DS4_ANALOG_AXIS_LEFT_Y]);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_RIGHTX] = egc_u8_to_s16(axes[DS4_ANALOG_AXIS_RIGHT_X]);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_RIGHTY] = egc_u8_to_s16(axes[DS4_ANALOG_AXIS_RIGHT_Y]);

#define MAP_ACCEL(v) ((s16)le16toh(v) * EGC_ACCELEROMETER_RES_PER_G / DS4_ACC_RES_PER_G)
        state.gamepad.accelerometer[0].x = -MAP_ACCEL(report->accel_x);
        state.gamepad.accelerometer[0].y = MAP_ACCEL(report->accel_y);
        state.gamepad.accelerometer[0].z = MAP_ACCEL(report->accel_z);
#undef MAP_ACCEL

#define MAP_TOUCH_X(v) ((v) * EGC_GAMEPAD_TOUCH_RES / DS4_TOUCHPAD_W)
#define MAP_TOUCH_Y(v) ((v) * EGC_GAMEPAD_TOUCH_RES / DS4_TOUCHPAD_H)
        if (!report->finger1_nactive) {
            state.gamepad.touch_points[0].x = MAP_TOUCH_X(report->finger1_x_lo | ((u16)report->finger1_x_hi << 8));
            state.gamepad.touch_points[0].y = MAP_TOUCH_Y(report->finger1_y_lo | ((u16)report->finger1_y_hi << 4));
        } else {
            state.gamepad.touch_points[0].x = -1;
        }

        if (!report->finger2_nactive) {
            state.gamepad.touch_points[1].x = MAP_TOUCH_X(report->finger2_x_lo | ((u16)report->finger2_x_hi << 8));
            state.gamepad.touch_points[1].y = MAP_TOUCH_Y(report->finger2_y_lo | ((u16)report->finger2_y_hi << 4));
        } else {
            state.gamepad.touch_points[1].x = -1;
        }
#undef MAP_TOUCH_X
#undef MAP_TOUCH_Y

        egc_device_driver_report_input(device, &state);
    }

    ds4_request_data(device);
}

static inline int ds4_request_data(egc_input_device_t *device)
{
    const egc_usb_transfer_t *transfer = egc_device_driver_issue_intr_transfer_async(
        device, EGC_USB_ENDPOINT_IN, NULL, 0, ds4_request_data_cb);
    return transfer != NULL ? 0 : -1;
}

static int ds4_driver_update_leds_rumble(egc_input_device_t *device)
{
    struct ds4_private_data_t *priv = (void *)device->private_data;

    u8 r = priv->led_color[0], g = priv->led_color[1], b = priv->led_color[2];

    return ds4_set_leds_rumble(device, r, g, b, priv->rumble_on * 192, 0);
}

bool ds4_driver_ops_probe(u16 vid, u16 pid)
{
    static const egc_device_id_t compatible[] = {
        { SONY_VID, 0x05c4 },
        { SONY_VID, 0x09cc },
    };

    return egc_device_driver_is_compatible(vid, pid, compatible, ARRAY_SIZE(compatible));
}

int ds4_driver_ops_init(egc_input_device_t *device, u16 vid, u16 pid)
{
    struct ds4_private_data_t *priv = (void *)device->private_data;
    egc_device_description_t *desc = egc_device_driver_alloc_desc(device);

    if (desc) {
        memcpy(desc, &s_device_description, sizeof(*desc));
        desc->product_id = pid;
    }
    /* Init private state */
    priv->led_color[0] = priv->led_color[1] = priv->led_color[2] = 0;
    priv->rumble_on = false;

    return ds4_request_data(device);
}

int ds4_driver_ops_disconnect(egc_input_device_t *device)
{
    struct ds4_private_data_t *priv = (void *)device->private_data;

    priv->led_color[0] = priv->led_color[1] = priv->led_color[2] = 0;
    priv->rumble_on = false;

    return ds4_driver_update_leds_rumble(device);
}

static inline void add_color_component(struct ds4_private_data_t *priv, int component, u8 value)
{
    priv->led_color[component] += value;
    if (priv->led_color[component] > 32) priv->led_color[component] = 32;
}

int ds4_driver_ops_set_leds(egc_input_device_t *device, u32 led_state)
{
    struct ds4_private_data_t *priv = (void *)device->private_data;

    priv->led_color[0] = priv->led_color[1] = priv->led_color[2] = 0;
    if (led_state & 0x1) {
        /* Player 1 is blue */
        add_color_component(priv, 2, 32);
    }
    if (led_state & 0x2) {
        /* Player 2 is red */
        add_color_component(priv, 0, 32);
    }
    if (led_state & 0x4) {
        /* Player 3 is green */
        add_color_component(priv, 1, 32);
    }
    if (led_state & 0x8) {
        /* Player 4 is pink */
        add_color_component(priv, 0, 32);
        add_color_component(priv, 2, 32);
    }


    return ds4_driver_update_leds_rumble(device);
}

int ds4_driver_ops_set_rumble(egc_input_device_t *device, bool rumble_on)
{
    struct ds4_private_data_t *priv = (void *)device->private_data;

    priv->rumble_on = rumble_on;

    return ds4_driver_update_leds_rumble(device);
}

const egc_device_driver_t ds4_usb_device_driver = {
    .probe = ds4_driver_ops_probe,
    .init = ds4_driver_ops_init,
    .disconnect = ds4_driver_ops_disconnect,
    .set_leds = ds4_driver_ops_set_leds,
    .set_rumble = ds4_driver_ops_set_rumble,
};
