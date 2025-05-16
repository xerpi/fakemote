#include "driver_api.h"
#include "utils.h"

#define SONY_VID 0x054c

#define DS3_ACC_RES_PER_G 113

struct ds3_input_report {
    u8 report_id;
    u8 unk0;

    u8 left : 1;
    u8 down : 1;
    u8 right : 1;
    u8 up : 1;
    u8 start : 1;
    u8 r3 : 1;
    u8 l3 : 1;
    u8 select : 1;

    u8 square : 1;
    u8 cross : 1;
    u8 circle : 1;
    u8 triangle : 1;
    u8 r1 : 1;
    u8 l1 : 1;
    u8 r2 : 1;
    u8 l2 : 1;

    u8 not_used : 7;
    u8 ps : 1;

    u8 unk1;

    u8 left_x;
    u8 left_y;
    u8 right_x;
    u8 right_y;

    u32 unk2;

    u8 dpad_sens_up;
    u8 dpad_sens_right;
    u8 dpad_sens_down;
    u8 dpad_sens_left;

    u8 shoulder_sens_l2;
    u8 shoulder_sens_r2;
    u8 shoulder_sens_l1;
    u8 shoulder_sens_r1;

    u8 button_sens_triangle;
    u8 button_sens_circle;
    u8 button_sens_cross;
    u8 button_sens_square;

    u16 unk3;
    u8 unk4;

    u8 status;
    u8 power_rating;
    u8 comm_status;

    u32 unk5;
    u32 unk6;
    u8 unk7;

    u16 acc_x;
    u16 acc_y;
    u16 acc_z;
    u16 z_gyro;
} ATTRIBUTE_PACKED;

struct ds3_rumble {
    u8 duration_right;
    u8 power_right;
    u8 duration_left;
    u8 power_left;
};

enum ds3_buttons_e {
    DS3_BUTTON_TRIANGLE,
    DS3_BUTTON_CIRCLE,
    DS3_BUTTON_CROSS,
    DS3_BUTTON_SQUARE,
    DS3_BUTTON_UP,
    DS3_BUTTON_DOWN,
    DS3_BUTTON_LEFT,
    DS3_BUTTON_RIGHT,
    DS3_BUTTON_R3,
    DS3_BUTTON_L3,
    DS3_BUTTON_START,
    DS3_BUTTON_SELECT,
    DS3_BUTTON_R2,
    DS3_BUTTON_L2,
    DS3_BUTTON_R1,
    DS3_BUTTON_L1,
    DS3_BUTTON_PS,
    DS3_BUTTON_COUNT
};

enum ds3_analog_axis_e {
    DS3_ANALOG_AXIS_LEFT_X,
    DS3_ANALOG_AXIS_LEFT_Y,
    DS3_ANALOG_AXIS_RIGHT_X,
    DS3_ANALOG_AXIS_RIGHT_Y,
    /* TODO: L2 and R2 are also axes */
    DS3_ANALOG_AXIS_COUNT
};

struct ds3_private_data_t {
    u8 leds;
    bool rumble_on;
};
static_assert(sizeof(struct ds3_private_data_t) <= EGC_INPUT_DEVICE_PRIVATE_DATA_SIZE);

/* Map each button of the controller to an egc_gamepad_button_e */
static const egc_gamepad_button_e s_button_map[DS3_BUTTON_COUNT] = {
    [DS3_BUTTON_UP] = EGC_GAMEPAD_BUTTON_DPAD_UP,
    [DS3_BUTTON_DOWN] = EGC_GAMEPAD_BUTTON_DPAD_DOWN,
    [DS3_BUTTON_LEFT] = EGC_GAMEPAD_BUTTON_DPAD_LEFT,
    [DS3_BUTTON_RIGHT] = EGC_GAMEPAD_BUTTON_DPAD_RIGHT,
    [DS3_BUTTON_TRIANGLE] = EGC_GAMEPAD_BUTTON_NORTH,
    [DS3_BUTTON_CIRCLE] = EGC_GAMEPAD_BUTTON_EAST,
    [DS3_BUTTON_CROSS] = EGC_GAMEPAD_BUTTON_SOUTH,
    [DS3_BUTTON_SQUARE] = EGC_GAMEPAD_BUTTON_WEST,
    [DS3_BUTTON_L1] = EGC_GAMEPAD_BUTTON_LEFT_SHOULDER,
    [DS3_BUTTON_R1] = EGC_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    [DS3_BUTTON_L2] = EGC_GAMEPAD_BUTTON_LEFT_PADDLE1,
    [DS3_BUTTON_R2] = EGC_GAMEPAD_BUTTON_RIGHT_PADDLE1,
    [DS3_BUTTON_SELECT] = EGC_GAMEPAD_BUTTON_BACK,
    [DS3_BUTTON_START] = EGC_GAMEPAD_BUTTON_START,
    [DS3_BUTTON_PS] = EGC_GAMEPAD_BUTTON_GUIDE,
    [DS3_BUTTON_L3] = EGC_GAMEPAD_BUTTON_LEFT_STICK,
    [DS3_BUTTON_R3] = EGC_GAMEPAD_BUTTON_RIGHT_STICK,
};

static const egc_device_description_t s_device_description = {
    .vendor_id = SONY_VID,
    .product_id = 0x0268,
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
    .num_touch_points = 0,
    .num_leds = 4,
    .num_accelerometers = 1,
    .has_rumble = true,
};

static int ds3_request_data(egc_input_device_t *device);

static inline u32 ds3_get_buttons(const struct ds3_input_report *report)
{
    u32 mask = 0;

#define MAP(field, button)                                                                         \
    if (report->field)                                                                             \
        mask |= BIT(button);

    MAP(triangle, DS3_BUTTON_TRIANGLE)
    MAP(circle, DS3_BUTTON_CIRCLE)
    MAP(cross, DS3_BUTTON_CROSS)
    MAP(square, DS3_BUTTON_SQUARE)
    MAP(up, DS3_BUTTON_UP)
    MAP(down, DS3_BUTTON_DOWN)
    MAP(left, DS3_BUTTON_LEFT)
    MAP(right, DS3_BUTTON_RIGHT)
    MAP(r3, DS3_BUTTON_R3)
    MAP(l3, DS3_BUTTON_L3)
    MAP(start, DS3_BUTTON_START)
    MAP(select, DS3_BUTTON_SELECT)
    MAP(r2, DS3_BUTTON_R2)
    MAP(l2, DS3_BUTTON_L2)
    MAP(r1, DS3_BUTTON_R1)
    MAP(l1, DS3_BUTTON_L1)
    MAP(ps, DS3_BUTTON_PS)
#undef MAP

    return mask;
}

static inline void ds3_get_analog_axis(const struct ds3_input_report *report,
                                       u8 analog_axis[static DS3_ANALOG_AXIS_COUNT])
{
    analog_axis[DS3_ANALOG_AXIS_LEFT_X] = report->left_x;
    analog_axis[DS3_ANALOG_AXIS_LEFT_Y] = 255 - report->left_y;
    analog_axis[DS3_ANALOG_AXIS_RIGHT_X] = report->right_x;
    analog_axis[DS3_ANALOG_AXIS_RIGHT_Y] = 255 - report->right_y;
}

static void ds3_get_report_cb(egc_usb_transfer_t *transfer)
{
    egc_input_device_t *device = transfer->device;
    struct ds3_input_report *report = (void *)transfer->data;
    struct egc_input_state_t state;

    if (transfer->status == EGC_USB_TRANSFER_STATUS_COMPLETED) {
        u32 buttons = ds3_get_buttons(report);
        state.gamepad.buttons = egc_device_driver_map_buttons(buttons, DS3_BUTTON_COUNT, s_button_map);

        u8 axes[DS3_ANALOG_AXIS_COUNT];
        ds3_get_analog_axis(report, axes);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_LEFTX] = egc_u8_to_s16(axes[DS3_ANALOG_AXIS_LEFT_X]);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_LEFTY] = egc_u8_to_s16(axes[DS3_ANALOG_AXIS_LEFT_Y]);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_RIGHTX] = egc_u8_to_s16(axes[DS3_ANALOG_AXIS_RIGHT_X]);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_RIGHTY] = egc_u8_to_s16(axes[DS3_ANALOG_AXIS_RIGHT_Y]);

#define MAP_ACCEL(v) ((v) * EGC_ACCELEROMETER_RES_PER_G / DS3_ACC_RES_PER_G)
        state.gamepad.accelerometer[0].x = MAP_ACCEL((s16)report->acc_x - 511);
        state.gamepad.accelerometer[0].y = MAP_ACCEL(511 - (s16)report->acc_y);
        state.gamepad.accelerometer[0].z = MAP_ACCEL(511 - (s16)report->acc_z);
#undef MAP_ACCEL

        egc_device_driver_report_input(device, &state);
    }

    ds3_request_data(device);
}

static int ds3_request_data(egc_input_device_t *device)
{
    const egc_usb_transfer_t *transfer = egc_device_driver_issue_ctrl_transfer_async(
        device, EGC_USB_REQTYPE_INTERFACE_GET, EGC_USB_REQ_GETREPORT,
        (EGC_USB_REPTYPE_INPUT << 8) | 0x01, 0, NULL, 0, ds3_get_report_cb);
    return transfer != NULL ? 0 : -1;
}

static void ds3_set_operational_cb(egc_usb_transfer_t *transfer)
{
    egc_input_device_t *device = transfer->device;
    ds3_request_data(device);
}

static int ds3_set_operational(egc_input_device_t *device)
{
    const egc_usb_transfer_t *transfer = egc_device_driver_issue_ctrl_transfer_async(
        device, EGC_USB_REQTYPE_INTERFACE_GET, EGC_USB_REQ_GETREPORT,
        (EGC_USB_REPTYPE_FEATURE << 8) | 0xf2, 0, NULL, 0, ds3_set_operational_cb);
    return transfer != NULL ? 0 : -1;
}

static int ds3_set_leds_rumble(egc_input_device_t *device, u8 leds, const struct ds3_rumble *rumble)
{
    u8 buf[] = {
        0x00,                         /* Padding */
        0x00, 0x00, 0x00, 0x00,       /* Rumble (r, r, l, l) */
        0x00, 0x00, 0x00, 0x00,       /* Padding */
        0x00,                         /* LED_1 = 0x02, LED_2 = 0x04, ... */
        0xff, 0x27, 0x10, 0x00, 0x32, /* LED_4 */
        0xff, 0x27, 0x10, 0x00, 0x32, /* LED_3 */
        0xff, 0x27, 0x10, 0x00, 0x32, /* LED_2 */
        0xff, 0x27, 0x10, 0x00, 0x32, /* LED_1 */
        0x00, 0x00, 0x00, 0x00, 0x00  /* LED_5 (not soldered) */
    };

    buf[1] = rumble->duration_right;
    buf[2] = rumble->power_right;
    buf[3] = rumble->duration_left;
    buf[4] = rumble->power_left;
    buf[9] = leds;

    const egc_usb_transfer_t *transfer = egc_device_driver_issue_ctrl_transfer_async(
        device, EGC_USB_REQTYPE_INTERFACE_SET, EGC_USB_REQ_SETREPORT,
        (EGC_USB_REPTYPE_OUTPUT << 8) | 0x01, 0, buf, sizeof(buf), NULL);
    return transfer != NULL ? 0 : -1;
}

static int ds3_driver_update_leds_rumble(egc_input_device_t *device)
{
    struct ds3_private_data_t *priv = (void *)device->private_data;
    struct ds3_rumble rumble;
    u8 leds;

    leds = priv->leds << 1;

    rumble.duration_right = priv->rumble_on * 255;
    rumble.power_right = 255;
    rumble.duration_left = 0;
    rumble.power_left = 0;

    return ds3_set_leds_rumble(device, leds, &rumble);
}

bool ds3_driver_ops_probe(u16 vid, u16 pid)
{
    static const egc_device_id_t compatible[] = {
        { SONY_VID, 0x0268 },
    };

    return egc_device_driver_is_compatible(vid, pid, compatible, ARRAY_SIZE(compatible));
}

int ds3_driver_ops_init(egc_input_device_t *device, u16 vid, u16 pid)
{
    int ret;
    struct ds3_private_data_t *priv = (void *)device->private_data;

    device->desc = &s_device_description;

    /* Init private state */
    priv->leds = 0;
    priv->rumble_on = false;

    ret = ds3_set_operational(device);
    if (ret < 0)
        return ret;

    return 0;
}

int ds3_driver_ops_disconnect(egc_input_device_t *device)
{
    struct ds3_private_data_t *priv = (void *)device->private_data;

    priv->leds = 0;
    priv->rumble_on = false;

    return ds3_driver_update_leds_rumble(device);
}

int ds3_driver_ops_set_leds(egc_input_device_t *device, u32 led_state)
{
    struct ds3_private_data_t *priv = (void *)device->private_data;

    priv->leds = led_state;

    return ds3_driver_update_leds_rumble(device);
}

int ds3_driver_ops_set_rumble(egc_input_device_t *device, bool rumble_on)
{
    struct ds3_private_data_t *priv = (void *)device->private_data;

    priv->rumble_on = rumble_on;

    return ds3_driver_update_leds_rumble(device);
}

const egc_device_driver_t ds3_usb_device_driver = {
    .probe = ds3_driver_ops_probe,
    .init = ds3_driver_ops_init,
    .disconnect = ds3_driver_ops_disconnect,
    .set_leds = ds3_driver_ops_set_leds,
    .set_rumble = ds3_driver_ops_set_rumble,
};
