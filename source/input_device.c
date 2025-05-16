#include "input_device.h"

#include "button_map.h"
#include "egc.h"
#include "fake_wiimote.h"
#include "types.h"
#include "utils.h"
#include "wiimote.h"

#define MAX_INPUT_DEVS  2
#define RECONNECT_DELAY 200 /* 1s @ 200Hz */

static const struct {
    u16 wiimote_button_map[EGC_GAMEPAD_BUTTON_COUNT];
    u8 nunchuk_button_map[EGC_GAMEPAD_BUTTON_COUNT];
    u8 nunchuk_analog_axis_map[EGC_GAMEPAD_AXIS_COUNT];
    u16 classic_button_map[EGC_GAMEPAD_BUTTON_COUNT];
    u8 classic_analog_axis_map[EGC_GAMEPAD_AXIS_COUNT];
} input_mappings = {
	.wiimote_button_map = {
		[EGC_GAMEPAD_BUTTON_NORTH] = WIIMOTE_BUTTON_ONE,
		[EGC_GAMEPAD_BUTTON_EAST] = WIIMOTE_BUTTON_B,
		[EGC_GAMEPAD_BUTTON_SOUTH] = WIIMOTE_BUTTON_A,
		[EGC_GAMEPAD_BUTTON_WEST] = WIIMOTE_BUTTON_TWO,
		[EGC_GAMEPAD_BUTTON_DPAD_UP] = WIIMOTE_BUTTON_UP,
		[EGC_GAMEPAD_BUTTON_DPAD_DOWN] = WIIMOTE_BUTTON_DOWN,
		[EGC_GAMEPAD_BUTTON_DPAD_LEFT] = WIIMOTE_BUTTON_LEFT,
		[EGC_GAMEPAD_BUTTON_DPAD_RIGHT] = WIIMOTE_BUTTON_RIGHT,
		[EGC_GAMEPAD_BUTTON_START] = WIIMOTE_BUTTON_PLUS,
		[EGC_GAMEPAD_BUTTON_BACK] = WIIMOTE_BUTTON_MINUS,
		[EGC_GAMEPAD_BUTTON_LEFT_STICK] = WIIMOTE_BUTTON_HOME,
	},
	.nunchuk_button_map = {
		[EGC_GAMEPAD_BUTTON_LEFT_SHOULDER] = NUNCHUK_BUTTON_C,
		[EGC_GAMEPAD_BUTTON_RIGHT_SHOULDER] = NUNCHUK_BUTTON_Z,
	},
	.nunchuk_analog_axis_map = {
		[EGC_GAMEPAD_AXIS_LEFTX] = BM_NUNCHUK_ANALOG_AXIS_X,
		[EGC_GAMEPAD_AXIS_LEFTY] = BM_NUNCHUK_ANALOG_AXIS_Y,
	},
	.classic_button_map = {
		[EGC_GAMEPAD_BUTTON_NORTH] = CLASSIC_CTRL_BUTTON_X,
		[EGC_GAMEPAD_BUTTON_EAST] = CLASSIC_CTRL_BUTTON_A,
		[EGC_GAMEPAD_BUTTON_SOUTH] = CLASSIC_CTRL_BUTTON_B,
		[EGC_GAMEPAD_BUTTON_WEST] = CLASSIC_CTRL_BUTTON_Y,
		[EGC_GAMEPAD_BUTTON_DPAD_UP] = CLASSIC_CTRL_BUTTON_UP,
		[EGC_GAMEPAD_BUTTON_DPAD_DOWN] = CLASSIC_CTRL_BUTTON_DOWN,
		[EGC_GAMEPAD_BUTTON_DPAD_LEFT] = CLASSIC_CTRL_BUTTON_LEFT,
		[EGC_GAMEPAD_BUTTON_DPAD_RIGHT] = CLASSIC_CTRL_BUTTON_RIGHT,
		[EGC_GAMEPAD_BUTTON_START] = CLASSIC_CTRL_BUTTON_PLUS,
		[EGC_GAMEPAD_BUTTON_BACK] = CLASSIC_CTRL_BUTTON_MINUS,
		[EGC_GAMEPAD_BUTTON_RIGHT_PADDLE1] = CLASSIC_CTRL_BUTTON_ZR,
		[EGC_GAMEPAD_BUTTON_LEFT_PADDLE1] = CLASSIC_CTRL_BUTTON_ZL,
		[EGC_GAMEPAD_BUTTON_RIGHT_SHOULDER] = CLASSIC_CTRL_BUTTON_FULL_R,
		[EGC_GAMEPAD_BUTTON_LEFT_SHOULDER] = CLASSIC_CTRL_BUTTON_FULL_L,
		[EGC_GAMEPAD_BUTTON_LEFT_STICK] = CLASSIC_CTRL_BUTTON_HOME,
	},
	.classic_analog_axis_map = {
		[EGC_GAMEPAD_AXIS_LEFTX] = BM_CLASSIC_ANALOG_AXIS_LEFT_X,
		[EGC_GAMEPAD_AXIS_LEFTY] = BM_CLASSIC_ANALOG_AXIS_LEFT_Y,
		[EGC_GAMEPAD_AXIS_RIGHTX] = BM_CLASSIC_ANALOG_AXIS_RIGHT_X,
		[EGC_GAMEPAD_AXIS_RIGHTY] = BM_CLASSIC_ANALOG_AXIS_RIGHT_Y,
	},
};

static const u8 ir_analog_axis_map[EGC_GAMEPAD_AXIS_COUNT] = {
    [EGC_GAMEPAD_AXIS_RIGHTX] = BM_IR_AXIS_X,
    [EGC_GAMEPAD_AXIS_RIGHTY] = BM_IR_AXIS_Y,
};

static const enum bm_ir_emulation_mode_e ir_emu_modes[] = {
    BM_IR_EMULATION_MODE_DIRECT,
    BM_IR_EMULATION_MODE_RELATIVE_ANALOG_AXIS,
    BM_IR_EMULATION_MODE_ABSOLUTE_ANALOG_AXIS,
};

static struct input_device_t {
    egc_input_device_t *device;
    /* NULL if no assigned fake Wiimote */
    fake_wiimote_t *assigned_wiimote;
    u32 reconnect_delay;
    u32 switch_mapping_combo;
    u32 switch_ir_emu_mode_combo;
    enum bm_ir_emulation_mode_e ir_emu_mode;
    struct bm_ir_emulation_state_t ir_emu_state;
    bool switch_mapping;
    bool switch_ir_emu_mode;
    u8 extension;
    u8 ir_emu_mode_idx;
} input_devices[MAX_INPUT_DEVS];

static inline bool has_button(egc_input_device_t *device, egc_gamepad_button_e button)
{
    return device->desc->available_buttons & BIT(button);
}

static input_device_t *input_device_from_egc(egc_input_device_t *device)
{
    for (int i = 0; i < ARRAY_SIZE(input_devices); i++)
        if (input_devices[i].device == device)
            return &input_devices[i];
    return NULL;
}

void input_devices_init(void)
{
    for (int i = 0; i < ARRAY_SIZE(input_devices); i++)
        input_devices[i].device = NULL;
}

void input_device_handle_added(egc_input_device_t *device, void *userdata)
{
    /* Find a free input device slot */
    for (int i = 0; i < ARRAY_SIZE(input_devices); i++) {
        if (!input_devices[i].device) {
            input_devices[i].device = device;
            /* No assigned fake Wiimote yet */
            input_devices[i].assigned_wiimote = NULL;
            input_devices[i].reconnect_delay = 0;
            input_devices[i].extension = WIIMOTE_EXT_NUNCHUK;
            input_devices[i].ir_emu_mode_idx = BM_IR_EMULATION_MODE_DIRECT;

            if (has_button(device, EGC_GAMEPAD_BUTTON_LEFT_STICK) &&
                has_button(device, EGC_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
                input_devices[i].switch_mapping_combo =
                    BIT(EGC_GAMEPAD_BUTTON_LEFT_STICK) |
                    BIT(EGC_GAMEPAD_BUTTON_LEFT_SHOULDER);
            } else {
                /* TODO; figure out another combination */
                input_devices[i].switch_mapping_combo = 0;
            }

            if (has_button(device, EGC_GAMEPAD_BUTTON_RIGHT_STICK) &&
                has_button(device, EGC_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
                input_devices[i].switch_ir_emu_mode_combo =
                    BIT(EGC_GAMEPAD_BUTTON_RIGHT_STICK) |
                    BIT(EGC_GAMEPAD_BUTTON_RIGHT_SHOULDER);
            } else {
                /* TODO; figure out another combination */
                input_devices[i].switch_ir_emu_mode_combo = 0;
            }
            break;
        }
    }
}

void input_device_handle_removed(egc_input_device_t *device, void *userdata)
{
    input_device_t *input_device = input_device_from_egc(device);
    if (!input_device) return;

    fake_wiimote_t *wiimote = input_device->assigned_wiimote;

    /* Check and disconnect if a fake wiimote is assigned to this input device */
    if (wiimote) {
        /* First unassign the input device so that disconnect() doesn't send USB requests */
        fake_wiimote_release_input_device(wiimote);
        fake_wiimote_disconnect(wiimote);
    }
    input_device->device = NULL;
}

void input_devices_tick(void)
{
    for (int i = 0; i < ARRAY_SIZE(input_devices); i++) {
        if (input_devices[i].device && !input_devices[i].assigned_wiimote) {
            if (input_devices[i].reconnect_delay > 0)
                input_devices[i].reconnect_delay--;
        }
    }
}

input_device_t *input_device_get_unassigned(void)
{
    for (int i = 0; i < ARRAY_SIZE(input_devices); i++) {
        if (input_devices[i].device && !input_devices[i].assigned_wiimote &&
            input_devices[i].reconnect_delay == 0) {
            return &input_devices[i];
        }
    }

    return NULL;
}

void input_device_assign_wiimote(input_device_t *input_device, fake_wiimote_t *wiimote)
{
    input_device->assigned_wiimote = wiimote;
}

void input_device_release_wiimote(input_device_t *input_device)
{
    input_device->assigned_wiimote = NULL;
    input_device->reconnect_delay = RECONNECT_DELAY;
    egc_input_device_suspend(input_device->device);
}

int input_device_resume(input_device_t *input_device)
{
    return egc_input_device_resume(input_device->device);
}

int input_device_suspend(input_device_t *input_device)
{
    return egc_input_device_suspend(input_device->device);
}

int input_device_set_leds(input_device_t *input_device, int leds)
{
    return egc_input_device_set_leds(input_device->device, leds);
}

int input_device_set_rumble(input_device_t *input_device, bool rumble_on)
{
    return egc_input_device_set_rumble(input_device->device,
                                       rumble_on ? EGC_RUMBLE_MAX : EGC_RUMBLE_OFF);
}

bool input_device_report_input(input_device_t *input_device)
{
    const egc_input_state_t *input = &input_device->device->state;
    fake_wiimote_t *wiimote = input_device->assigned_wiimote;
    u16 wiimote_buttons = 0;
    union wiimote_extension_data_t extension_data;
    struct ir_dot_t ir_dots[IR_MAX_DOTS];
    enum bm_ir_emulation_mode_e ir_emu_mode;

    if (bm_check_switch_mapping(input->gamepad.buttons,
                                &input_device->switch_mapping,
                                input_device->switch_mapping_combo)) {
        input_device->extension =
            input_device->extension == WIIMOTE_EXT_NUNCHUK ? WIIMOTE_EXT_CLASSIC : WIIMOTE_EXT_NUNCHUK;
        fake_wiimote_set_extension(wiimote, input_device->extension);
        return false;
    } else if (bm_check_switch_mapping(input->gamepad.buttons,
                                       &input_device->switch_ir_emu_mode,
                                       input_device->switch_ir_emu_mode_combo)) {
        input_device->ir_emu_mode_idx =
            (input_device->ir_emu_mode_idx + 1) % ARRAY_SIZE(ir_emu_modes);
        /* Direct mode is only supported if we have a touchpad */
        if (input_device->ir_emu_mode_idx == BM_IR_EMULATION_MODE_DIRECT &&
            input_device->device->desc->num_touch_points == 0) {
            input_device->ir_emu_mode_idx =
                (input_device->ir_emu_mode_idx + 1) % ARRAY_SIZE(ir_emu_modes);
        }
        bm_ir_emulation_state_reset(&input_device->ir_emu_state);
    }

    if (input_device->extension == WIIMOTE_EXT_NUNCHUK) {
        bm_map_wiimote(EGC_GAMEPAD_BUTTON_COUNT, input->gamepad.buttons,
                       input_mappings.wiimote_button_map, &wiimote_buttons);
    }

    if (input_device->device->desc->num_accelerometers > 0) {
        fake_wiimote_report_accelerometer(wiimote,
                                          input->gamepad.accelerometer[0].x,
                                          input->gamepad.accelerometer[0].y,
                                          input->gamepad.accelerometer[0].z);
    }

    ir_emu_mode = ir_emu_modes[input_device->ir_emu_mode_idx];
    if (ir_emu_mode == BM_IR_EMULATION_MODE_NONE) {
        bm_ir_dots_set_out_of_screen(ir_dots);
    } else {
        if (ir_emu_mode == BM_IR_EMULATION_MODE_DIRECT) {
            bm_map_ir_direct(input->gamepad.touch_points[0].x,
                             input->gamepad.touch_points[0].y,
                             ir_dots);
        } else {
            bm_map_ir_analog_axis(ir_emu_mode, &input_device->ir_emu_state, EGC_GAMEPAD_AXIS_COUNT,
                                  input->gamepad.axes, ir_analog_axis_map, ir_dots);
        }
    }

    fake_wiimote_report_ir_dots(wiimote, ir_dots);

    if (input_device->extension == WIIMOTE_EXT_NONE) {
        fake_wiimote_report_input(wiimote, wiimote_buttons);
    } else if (input_device->extension == WIIMOTE_EXT_NUNCHUK) {
        bm_map_nunchuk(
            EGC_GAMEPAD_BUTTON_COUNT, input->gamepad.buttons, EGC_GAMEPAD_AXIS_COUNT, input->gamepad.axes, 0,
            0, 0, input_mappings.nunchuk_button_map,
            input_mappings.nunchuk_analog_axis_map, &extension_data.nunchuk);
        fake_wiimote_report_input_ext(wiimote, wiimote_buttons, &extension_data,
                                      sizeof(extension_data.nunchuk));
    } else if (input_device->extension == WIIMOTE_EXT_CLASSIC) {
        bm_map_classic(EGC_GAMEPAD_BUTTON_COUNT, input->gamepad.buttons, EGC_GAMEPAD_AXIS_COUNT,
                       input->gamepad.axes, input_mappings.classic_button_map,
                       input_mappings.classic_analog_axis_map,
                       &extension_data.classic);
        fake_wiimote_report_input_ext(wiimote, wiimote_buttons, &extension_data,
                                      sizeof(extension_data.classic));
    }
    return true;
}
