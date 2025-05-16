#include "driver_api.h"
#include "utils.h"

struct dr_input_report {
    u8 left_x;
    u8 left_y;
    u8 left_x_ignore;
    u8 right_x;
    u8 right_y;

    u8 bitfield1;
    u8 bitfield2;

    u8 unknown; /* Hardcoded 0x40? */
} ATTRIBUTE_PACKED;

enum dr_buttons_e {
    /* bitfield1: */
    DR_BUTTON_UP,
    DR_BUTTON_DOWN,
    DR_BUTTON_LEFT,
    DR_BUTTON_RIGHT,
    DR_BUTTON_1,
    DR_BUTTON_2,
    DR_BUTTON_3,
    DR_BUTTON_4,
    /* bitfield2: */
    DR_BUTTON_L1,
    DR_BUTTON_R1,
    DR_BUTTON_L2,
    DR_BUTTON_R2,
    DR_BUTTON_SELECT,
    DR_BUTTON_START,
    DR_BUTTON_JOY1,
    DR_BUTTON_JOY2,
    DR_BUTTON_COUNT
};

enum dr_analog_axis_e {
    DR_ANALOG_AXIS_LEFT_X,
    DR_ANALOG_AXIS_LEFT_Y,
    DR_ANALOG_AXIS_RIGHT_X,
    DR_ANALOG_AXIS_RIGHT_Y,
    DR_ANALOG_AXIS_COUNT
};

struct dr_private_data_t {
};
static_assert(sizeof(struct dr_private_data_t) <= EGC_INPUT_DEVICE_PRIVATE_DATA_SIZE);

/* Map each button of the controller to an egc_gamepad_button_e */
static const egc_gamepad_button_e s_button_map[DR_BUTTON_COUNT] = {
    [DR_BUTTON_UP] = EGC_GAMEPAD_BUTTON_DPAD_UP,
    [DR_BUTTON_DOWN] = EGC_GAMEPAD_BUTTON_DPAD_DOWN,
    [DR_BUTTON_LEFT] = EGC_GAMEPAD_BUTTON_DPAD_LEFT,
    [DR_BUTTON_RIGHT] = EGC_GAMEPAD_BUTTON_DPAD_RIGHT,
    [DR_BUTTON_1] = EGC_GAMEPAD_BUTTON_NORTH,
    [DR_BUTTON_2] = EGC_GAMEPAD_BUTTON_EAST,
    [DR_BUTTON_3] = EGC_GAMEPAD_BUTTON_SOUTH,
    [DR_BUTTON_4] = EGC_GAMEPAD_BUTTON_WEST,
    [DR_BUTTON_L1] = EGC_GAMEPAD_BUTTON_LEFT_SHOULDER,
    [DR_BUTTON_R1] = EGC_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    [DR_BUTTON_L2] = EGC_GAMEPAD_BUTTON_LEFT_PADDLE1,
    [DR_BUTTON_R2] = EGC_GAMEPAD_BUTTON_RIGHT_PADDLE1,
    [DR_BUTTON_SELECT] = EGC_GAMEPAD_BUTTON_BACK,
    [DR_BUTTON_START] = EGC_GAMEPAD_BUTTON_START,
    [DR_BUTTON_JOY1] = EGC_GAMEPAD_BUTTON_LEFT_STICK,
    [DR_BUTTON_JOY2] = EGC_GAMEPAD_BUTTON_RIGHT_STICK,
};

static const egc_device_description_t s_device_description = {
    .vendor_id = 0x0079,
    .product_id = 0x0006,
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
        BIT(EGC_GAMEPAD_BUTTON_LEFT_STICK) |
        BIT(EGC_GAMEPAD_BUTTON_RIGHT_STICK),
    .available_axes =
        BIT(EGC_GAMEPAD_AXIS_LEFTX) |
        BIT(EGC_GAMEPAD_AXIS_LEFTY) |
        BIT(EGC_GAMEPAD_AXIS_RIGHTX) |
        BIT(EGC_GAMEPAD_AXIS_RIGHTY),
    .type = EGC_DEVICE_TYPE_GAMEPAD,
    .num_touch_points = 0,
    .num_leds = 0,
    .num_accelerometers = 0,
    .has_rumble = false,
};

static int dr_request_data(egc_input_device_t *device);

static inline u8 dpad_to_buttons(u8 dpad)
{
    switch (dpad) {
    case 0:
        return 1 << DR_BUTTON_UP;
    case 1:
        return (1 << DR_BUTTON_UP) | (1 << DR_BUTTON_RIGHT);
    case 2:
        return 1 << DR_BUTTON_RIGHT;
    case 3:
        return (1 << DR_BUTTON_RIGHT) | (1 << DR_BUTTON_DOWN);
    case 4:
        return 1 << DR_BUTTON_DOWN;
    case 5:
        return (1 << DR_BUTTON_DOWN) | (1 << DR_BUTTON_LEFT);
    case 6:
        return 1 << DR_BUTTON_LEFT;
    case 7:
        return (1 << DR_BUTTON_LEFT) | (1 << DR_BUTTON_UP);
    default:
        return 0;
    }
}

static inline u32 dr_get_buttons(const struct dr_input_report *report)
{
    u8 bitfield1 = report->bitfield1;
    bitfield1 = (bitfield1 & 0xf0) | dpad_to_buttons(bitfield1 & 0xf);
    return bitfield1 | (report->bitfield2 << 8);
}

static inline void dr_get_analog_axis(const struct dr_input_report *report,
                                      u8 analog_axis[static DR_ANALOG_AXIS_COUNT])
{
    analog_axis[DR_ANALOG_AXIS_LEFT_X] = report->left_x;
    analog_axis[DR_ANALOG_AXIS_LEFT_Y] = 255 - report->left_y;
    analog_axis[DR_ANALOG_AXIS_RIGHT_X] = report->right_x;
    analog_axis[DR_ANALOG_AXIS_RIGHT_Y] = 255 - report->right_y;
}

static void intr_transfer_cb(egc_usb_transfer_t *transfer)
{
    egc_input_device_t *device = transfer->device;
    struct dr_input_report *report = (void *)transfer->data;
    struct egc_input_state_t state;

    if (transfer->status == EGC_USB_TRANSFER_STATUS_COMPLETED) {
        u32 buttons = dr_get_buttons(report);
        state.gamepad.buttons = egc_device_driver_map_buttons(buttons, DR_BUTTON_COUNT, s_button_map);

        u8 axes[DR_ANALOG_AXIS_COUNT];
        dr_get_analog_axis(report, axes);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_LEFTX] = egc_u8_to_s16(axes[DR_ANALOG_AXIS_LEFT_X]);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_LEFTY] = egc_u8_to_s16(axes[DR_ANALOG_AXIS_LEFT_Y]);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_RIGHTX] = egc_u8_to_s16(axes[DR_ANALOG_AXIS_RIGHT_X]);
        state.gamepad.axes[EGC_GAMEPAD_AXIS_RIGHTY] = egc_u8_to_s16(axes[DR_ANALOG_AXIS_RIGHT_Y]);

        egc_device_driver_report_input(device, &state);
    }

    dr_request_data(device);
}

static int dr_request_data(egc_input_device_t *device)
{
    const egc_usb_transfer_t *transfer = egc_device_driver_issue_intr_transfer_async(
        device, EGC_USB_ENDPOINT_IN, NULL, 0, intr_transfer_cb);
    return transfer != NULL ? 0 : -1;
}

static bool dr_driver_ops_probe(u16 vid, u16 pid)
{
    static const egc_device_id_t compatible[] = {
        { 0x0079, 0x0006 }, /* DragonRise Inc. | PC TWIN SHOCK Gamepad */
    };

    return egc_device_driver_is_compatible(vid, pid, compatible, ARRAY_SIZE(compatible));
}

static int dr_driver_ops_init(egc_input_device_t *device, u16 vid, u16 pid)
{
    device->desc = &s_device_description;

    /* Set a half-second timer to let the device stabilize before requesting
     * updates */
    egc_device_driver_set_timer(device, 1000 * 500, 0);
    return 0;
}

static bool dr_driver_ops_timer(egc_input_device_t *device)
{
    dr_request_data(device);
    /* Return false to destroy the timer */
    return false;
}

const egc_device_driver_t dr_usb_device_driver = {
    .probe = dr_driver_ops_probe,
    .init = dr_driver_ops_init,
    .timer = dr_driver_ops_timer,
};
