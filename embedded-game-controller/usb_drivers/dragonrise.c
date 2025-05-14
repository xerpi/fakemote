#include "button_map.h"
#include "usb_device_drivers.h"
#include "utils.h"
#include "wiimote.h"

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
    DR_BUTTON__NUM
};

enum dr_analog_axis_e {
    DR_ANALOG_AXIS_LEFT_X,
    DR_ANALOG_AXIS_LEFT_Y,
    DR_ANALOG_AXIS_RIGHT_X,
    DR_ANALOG_AXIS_RIGHT_Y,
    DR_ANALOG_AXIS__NUM
};

struct dr_private_data_t {
    struct {
        u32 buttons;
        u8 analog_axis[DR_ANALOG_AXIS__NUM];
    } input;
    enum bm_ir_emulation_mode_e ir_emu_mode;
    struct bm_ir_emulation_state_t ir_emu_state;
    u8 extension;
    u8 ir_emu_mode_idx;
    bool switch_mapping;
    bool switch_ir_emu_mode;
};
static_assert(sizeof(struct dr_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

#define SWITCH_MAPPING_COMBO     (BIT(DR_BUTTON_L1) | BIT(DR_BUTTON_JOY1))
#define SWITCH_IR_EMU_MODE_COMBO (BIT(DR_BUTTON_R1) | BIT(DR_BUTTON_JOY2))

static const struct {
    u16 wiimote_button_map[DR_BUTTON__NUM];
    u8 nunchuk_button_map[DR_BUTTON__NUM];
    u8 nunchuk_analog_axis_map[DR_ANALOG_AXIS__NUM];
    u16 classic_button_map[DR_BUTTON__NUM];
    u8 classic_analog_axis_map[DR_ANALOG_AXIS__NUM];
} input_mappings = {
	.wiimote_button_map = {
		[DR_BUTTON_1] = WIIMOTE_BUTTON_ONE,
		[DR_BUTTON_2] = WIIMOTE_BUTTON_B,
		[DR_BUTTON_3] = WIIMOTE_BUTTON_A,
		[DR_BUTTON_4] = WIIMOTE_BUTTON_TWO,
		[DR_BUTTON_UP] = WIIMOTE_BUTTON_UP,
		[DR_BUTTON_DOWN] = WIIMOTE_BUTTON_DOWN,
		[DR_BUTTON_LEFT] = WIIMOTE_BUTTON_LEFT,
		[DR_BUTTON_RIGHT] = WIIMOTE_BUTTON_RIGHT,
		[DR_BUTTON_START] = WIIMOTE_BUTTON_PLUS,
		[DR_BUTTON_SELECT] = WIIMOTE_BUTTON_MINUS,
		[DR_BUTTON_JOY1] = WIIMOTE_BUTTON_HOME,
	},
	.nunchuk_button_map = {
		[DR_BUTTON_L1] = NUNCHUK_BUTTON_C,
		[DR_BUTTON_L2] = NUNCHUK_BUTTON_Z,
	},
	.nunchuk_analog_axis_map = {
		[DR_ANALOG_AXIS_LEFT_X] = BM_NUNCHUK_ANALOG_AXIS_X,
		[DR_ANALOG_AXIS_LEFT_Y] = BM_NUNCHUK_ANALOG_AXIS_Y,
	},
	.classic_button_map = {
		[DR_BUTTON_1] = CLASSIC_CTRL_BUTTON_X,
		[DR_BUTTON_2] = CLASSIC_CTRL_BUTTON_A,
		[DR_BUTTON_3] = CLASSIC_CTRL_BUTTON_B,
		[DR_BUTTON_4] = CLASSIC_CTRL_BUTTON_Y,
		[DR_BUTTON_UP] = CLASSIC_CTRL_BUTTON_UP,
		[DR_BUTTON_DOWN] = CLASSIC_CTRL_BUTTON_DOWN,
		[DR_BUTTON_LEFT] = CLASSIC_CTRL_BUTTON_LEFT,
		[DR_BUTTON_RIGHT] = CLASSIC_CTRL_BUTTON_RIGHT,
		[DR_BUTTON_START] = CLASSIC_CTRL_BUTTON_PLUS,
		[DR_BUTTON_SELECT] = CLASSIC_CTRL_BUTTON_MINUS,
		[DR_BUTTON_R2] = CLASSIC_CTRL_BUTTON_ZR,
		[DR_BUTTON_L2] = CLASSIC_CTRL_BUTTON_ZL,
		[DR_BUTTON_R1] = CLASSIC_CTRL_BUTTON_FULL_R,
		[DR_BUTTON_L1] = CLASSIC_CTRL_BUTTON_FULL_L,
		[DR_BUTTON_JOY1] = CLASSIC_CTRL_BUTTON_HOME,
	},
	.classic_analog_axis_map = {
		[DR_ANALOG_AXIS_LEFT_X] = BM_CLASSIC_ANALOG_AXIS_LEFT_X,
		[DR_ANALOG_AXIS_LEFT_Y] = BM_CLASSIC_ANALOG_AXIS_LEFT_Y,
		[DR_ANALOG_AXIS_RIGHT_X] = BM_CLASSIC_ANALOG_AXIS_RIGHT_X,
		[DR_ANALOG_AXIS_RIGHT_Y] = BM_CLASSIC_ANALOG_AXIS_RIGHT_Y,
	},
};

static const u8 ir_analog_axis_map[DR_ANALOG_AXIS__NUM] = {
    [DR_ANALOG_AXIS_RIGHT_X] = BM_IR_AXIS_X,
    [DR_ANALOG_AXIS_RIGHT_Y] = BM_IR_AXIS_Y,
};

static const enum bm_ir_emulation_mode_e ir_emu_modes[] = {
    BM_IR_EMULATION_MODE_DIRECT,
    BM_IR_EMULATION_MODE_RELATIVE_ANALOG_AXIS,
    BM_IR_EMULATION_MODE_ABSOLUTE_ANALOG_AXIS,
};

static int dr_request_data(usb_input_device_t *device);

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

static inline void dr_get_buttons(const struct dr_input_report *report, u32 *buttons)
{
    u8 bitfield1 = report->bitfield1;
    bitfield1 = (bitfield1 & 0xf0) | dpad_to_buttons(bitfield1 & 0xf);
    *buttons = bitfield1 | (report->bitfield2 << 8);
}

static inline void dr_get_analog_axis(const struct dr_input_report *report,
                                      u8 analog_axis[static DR_ANALOG_AXIS__NUM])
{
    analog_axis[DR_ANALOG_AXIS_LEFT_X] = report->left_x;
    analog_axis[DR_ANALOG_AXIS_LEFT_Y] = 255 - report->left_y;
    analog_axis[DR_ANALOG_AXIS_RIGHT_X] = report->right_x;
    analog_axis[DR_ANALOG_AXIS_RIGHT_Y] = 255 - report->right_y;
}

static void intr_transfer_cb(egc_usb_transfer_t *transfer)
{
    usb_input_device_t *device = egc_input_device_from_usb(transfer->device);
    struct dr_private_data_t *priv = (void *)device->private_data;
    struct dr_input_report *report = (void *)transfer->data;

    if (transfer->status == EGC_USB_TRANSFER_STATUS_COMPLETED) {
        dr_get_buttons(report, &priv->input.buttons);
        dr_get_analog_axis(report, priv->input.analog_axis);
    }

    dr_request_data(device);
}

static int dr_request_data(usb_input_device_t *device)
{
    const egc_usb_transfer_t *transfer = usb_device_driver_issue_intr_transfer_async(
        device, EGC_USB_ENDPOINT_IN, NULL, 0, intr_transfer_cb);
    return transfer != NULL ? 0 : -1;
}

static bool dr_driver_ops_probe(u16 vid, u16 pid)
{
    static const struct device_id_t compatible[] = {
        { 0x0079, 0x0006 }, /* DragonRise Inc. | PC TWIN SHOCK Gamepad */
    };

    return usb_driver_is_compatible(vid, pid, compatible, ARRAY_SIZE(compatible));
}

static int dr_driver_ops_init(usb_input_device_t *device, u16 vid, u16 pid)
{
    struct dr_private_data_t *priv = (void *)device->private_data;

    /* Init private state */
    priv->ir_emu_mode_idx = 0;
    bm_ir_emulation_state_reset(&priv->ir_emu_state);
    priv->extension = WIIMOTE_EXT_NUNCHUK;
    priv->switch_mapping = false;
    priv->switch_ir_emu_mode = false;

    /* Set initial extension */
    fake_wiimote_set_extension(device->wiimote, priv->extension);

    /* Set a half-second timer to let the device stabilize before requesting
     * updates */
    usb_device_driver_set_timer(device, 1000 * 500, 0);
    return 0;
}

bool dr_report_input(usb_input_device_t *device)
{
    struct dr_private_data_t *priv = (void *)device->private_data;
    u16 wiimote_buttons = 0;
    union wiimote_extension_data_t extension_data;
    struct ir_dot_t ir_dots[IR_MAX_DOTS];
    enum bm_ir_emulation_mode_e ir_emu_mode;

    if (bm_check_switch_mapping(priv->input.buttons, &priv->switch_mapping, SWITCH_MAPPING_COMBO)) {
        priv->extension =
            priv->extension == WIIMOTE_EXT_NUNCHUK ? WIIMOTE_EXT_CLASSIC : WIIMOTE_EXT_NUNCHUK;
        fake_wiimote_set_extension(device->wiimote, priv->extension);
        return false;
    } else if (bm_check_switch_mapping(priv->input.buttons, &priv->switch_ir_emu_mode,
                                       SWITCH_IR_EMU_MODE_COMBO)) {
        priv->ir_emu_mode_idx = (priv->ir_emu_mode_idx + 1) % ARRAY_SIZE(ir_emu_modes);
        bm_ir_emulation_state_reset(&priv->ir_emu_state);
    }

    if (priv->extension == WIIMOTE_EXT_NUNCHUK) {
        bm_map_wiimote(DR_BUTTON__NUM, priv->input.buttons, input_mappings.wiimote_button_map,
                       &wiimote_buttons);
    }

    ir_emu_mode = ir_emu_modes[priv->ir_emu_mode_idx];
    if (ir_emu_mode == BM_IR_EMULATION_MODE_NONE) {
        bm_ir_dots_set_out_of_screen(ir_dots);
    } else {
        bm_map_ir_analog_axis(ir_emu_mode, &priv->ir_emu_state, DR_ANALOG_AXIS__NUM,
                              priv->input.analog_axis, ir_analog_axis_map, ir_dots);
    }

    fake_wiimote_report_ir_dots(device->wiimote, ir_dots);

    if (priv->extension == WIIMOTE_EXT_NUNCHUK) {
        bm_map_nunchuk(DR_BUTTON__NUM, priv->input.buttons, DR_ANALOG_AXIS__NUM,
                       priv->input.analog_axis, 0, 0, 0, input_mappings.nunchuk_button_map,
                       input_mappings.nunchuk_analog_axis_map, &extension_data.nunchuk);
        fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons, &extension_data,
                                      sizeof(extension_data.nunchuk));
    } else if (priv->extension == WIIMOTE_EXT_CLASSIC) {
        bm_map_classic(DR_BUTTON__NUM, priv->input.buttons, DR_ANALOG_AXIS__NUM,
                       priv->input.analog_axis, input_mappings.classic_button_map,
                       input_mappings.classic_analog_axis_map, &extension_data.classic);
        fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons, &extension_data,
                                      sizeof(extension_data.classic));
    }

    return true;
}

static bool dr_driver_ops_timer(usb_input_device_t *device)
{
    dr_request_data(device);
    /* Return false to destroy the timer */
    return false;
}

const usb_device_driver_t dr_usb_device_driver = {
    .probe = dr_driver_ops_probe,
    .init = dr_driver_ops_init,
    .report_input = dr_report_input,
    .timer = dr_driver_ops_timer,
};
