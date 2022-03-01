#include "button_map.h"
#include "nintendont_hid.h"
#include "HID_controllers.h"
#include "usb_device_drivers.h"
#include "usb.h"
#include "utils.h"
#include "wiimote.h"

#if 0
	layout Power;

	layout A;
	layout B;
	layout X;
	layout Y;
	layout ZL;
	layout Z;

	layout L;
	layout R;
	layout S;

	layout Left;
	layout Down;
	layout Right;
	layout Up;

	layout RightUp;
	layout DownRight;
	layout DownLeft;
	layout UpLeft;
#endif

enum nintendont_buttons_e {
	NINTENDONT_BUTTON_TRIANGLE,
	NINTENDONT_BUTTON_CIRCLE,
	NINTENDONT_BUTTON_CROSS,
	NINTENDONT_BUTTON_SQUARE,
	NINTENDONT_BUTTON_UP,
	NINTENDONT_BUTTON_DOWN,
	NINTENDONT_BUTTON_LEFT,
	NINTENDONT_BUTTON_RIGHT,
	NINTENDONT_BUTTON_R3,
	NINTENDONT_BUTTON_L3,
	NINTENDONT_BUTTON_START,
	NINTENDONT_BUTTON_SELECT,
	NINTENDONT_BUTTON_R2,
	NINTENDONT_BUTTON_L2,
	NINTENDONT_BUTTON_R1,
	NINTENDONT_BUTTON_L1,
	NINTENDONT_BUTTON_PS,
	NINTENDONT_BUTTON__NUM
};

enum nintendont_analog_axis_e {
	NINTENDONT_ANALOG_AXIS_LEFT_X,
	NINTENDONT_ANALOG_AXIS_LEFT_Y,
	NINTENDONT_ANALOG_AXIS_RIGHT_X,
	NINTENDONT_ANALOG_AXIS_RIGHT_Y,
	NINTENDONT_ANALOG_AXIS__NUM
};

struct nintendont_private_data_t {
	struct {
		u32 buttons;
		u8 analog_axis[NINTENDONT_ANALOG_AXIS__NUM];
	} input;
	enum bm_ir_emulation_mode_e ir_emu_mode;
	struct bm_ir_emulation_state_t ir_emu_state;
	u8 mapping;
	u8 ir_emu_mode_idx;
	u8 leds;
	bool rumble_on;
	bool switch_mapping;
	bool switch_ir_emu_mode;
};
static_assert(sizeof(struct nintendont_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

#define SWITCH_MAPPING_COMBO		(BIT(NINTENDONT_BUTTON_L1) | BIT(NINTENDONT_BUTTON_L3))
#define SWITCH_IR_EMU_MODE_COMBO	(BIT(NINTENDONT_BUTTON_R1) | BIT(NINTENDONT_BUTTON_R3))

static const struct {
	enum wiimote_ext_e extension;
	u16 wiimote_button_map[NINTENDONT_BUTTON__NUM];
	u8 nunchuk_button_map[NINTENDONT_BUTTON__NUM];
	u8 nunchuk_analog_axis_map[NINTENDONT_ANALOG_AXIS__NUM];
	u16 classic_button_map[NINTENDONT_BUTTON__NUM];
	u8 classic_analog_axis_map[NINTENDONT_ANALOG_AXIS__NUM];
} input_mappings[] = {
	{
		.extension = WIIMOTE_EXT_NUNCHUK,
		.wiimote_button_map = {
			[NINTENDONT_BUTTON_TRIANGLE] = WIIMOTE_BUTTON_ONE,
			[NINTENDONT_BUTTON_CIRCLE]   = WIIMOTE_BUTTON_B,
			[NINTENDONT_BUTTON_CROSS]    = WIIMOTE_BUTTON_A,
			[NINTENDONT_BUTTON_SQUARE]   = WIIMOTE_BUTTON_TWO,
			[NINTENDONT_BUTTON_UP]       = WIIMOTE_BUTTON_UP,
			[NINTENDONT_BUTTON_DOWN]     = WIIMOTE_BUTTON_DOWN,
			[NINTENDONT_BUTTON_LEFT]     = WIIMOTE_BUTTON_LEFT,
			[NINTENDONT_BUTTON_RIGHT]    = WIIMOTE_BUTTON_RIGHT,
			[NINTENDONT_BUTTON_START]    = WIIMOTE_BUTTON_PLUS,
			[NINTENDONT_BUTTON_SELECT]   = WIIMOTE_BUTTON_MINUS,
			[NINTENDONT_BUTTON_PS]       = WIIMOTE_BUTTON_HOME,
		},
		.nunchuk_button_map = {
			[NINTENDONT_BUTTON_L1] = NUNCHUK_BUTTON_C,
			[NINTENDONT_BUTTON_L2] = NUNCHUK_BUTTON_Z,
		},
		.nunchuk_analog_axis_map = {
			[NINTENDONT_ANALOG_AXIS_LEFT_X] = BM_NUNCHUK_ANALOG_AXIS_X,
			[NINTENDONT_ANALOG_AXIS_LEFT_Y] = BM_NUNCHUK_ANALOG_AXIS_Y,
		},
	},
	{
		.extension = WIIMOTE_EXT_CLASSIC,
		.classic_button_map = {
			[NINTENDONT_BUTTON_TRIANGLE] = CLASSIC_CTRL_BUTTON_X,
			[NINTENDONT_BUTTON_CIRCLE]   = CLASSIC_CTRL_BUTTON_A,
			[NINTENDONT_BUTTON_CROSS]    = CLASSIC_CTRL_BUTTON_B,
			[NINTENDONT_BUTTON_SQUARE]   = CLASSIC_CTRL_BUTTON_Y,
			[NINTENDONT_BUTTON_UP]       = CLASSIC_CTRL_BUTTON_UP,
			[NINTENDONT_BUTTON_DOWN]     = CLASSIC_CTRL_BUTTON_DOWN,
			[NINTENDONT_BUTTON_LEFT]     = CLASSIC_CTRL_BUTTON_LEFT,
			[NINTENDONT_BUTTON_RIGHT]    = CLASSIC_CTRL_BUTTON_RIGHT,
			[NINTENDONT_BUTTON_START]    = CLASSIC_CTRL_BUTTON_PLUS,
			[NINTENDONT_BUTTON_SELECT]   = CLASSIC_CTRL_BUTTON_MINUS,
			[NINTENDONT_BUTTON_R2]       = CLASSIC_CTRL_BUTTON_ZR,
			[NINTENDONT_BUTTON_L2]       = CLASSIC_CTRL_BUTTON_ZL,
			[NINTENDONT_BUTTON_R1]       = CLASSIC_CTRL_BUTTON_FULL_R,
			[NINTENDONT_BUTTON_L1]       = CLASSIC_CTRL_BUTTON_FULL_L,
			[NINTENDONT_BUTTON_PS]       = CLASSIC_CTRL_BUTTON_HOME,
		},
		.classic_analog_axis_map = {
			[NINTENDONT_ANALOG_AXIS_LEFT_X]  = BM_CLASSIC_ANALOG_AXIS_LEFT_X,
			[NINTENDONT_ANALOG_AXIS_LEFT_Y]  = BM_CLASSIC_ANALOG_AXIS_LEFT_Y,
			[NINTENDONT_ANALOG_AXIS_RIGHT_X] = BM_CLASSIC_ANALOG_AXIS_RIGHT_X,
			[NINTENDONT_ANALOG_AXIS_RIGHT_Y] = BM_CLASSIC_ANALOG_AXIS_RIGHT_Y,
		},
	},
};

static const u8 ir_analog_axis_map[NINTENDONT_ANALOG_AXIS__NUM] = {
	[NINTENDONT_ANALOG_AXIS_RIGHT_X] = BM_IR_AXIS_X,
	[NINTENDONT_ANALOG_AXIS_RIGHT_Y] = BM_IR_AXIS_Y,
};

static const enum bm_ir_emulation_mode_e ir_emu_modes[] = {
	BM_IR_EMULATION_MODE_ABSOLUTE_ANALOG_AXIS,
	BM_IR_EMULATION_MODE_RELATIVE_ANALOG_AXIS,
	BM_IR_EMULATION_MODE_NONE,
};

static inline void nintendont_get_buttons(const u8 *report, u32 *buttons)
{
	u32 mask = 0;
#if 0
#define MAP(field, button) \
	if (report->field) \
		mask |= BIT(button);

	MAP(triangle, NINTENDONT_BUTTON_TRIANGLE)
	MAP(circle, NINTENDONT_BUTTON_CIRCLE)
	MAP(cross, NINTENDONT_BUTTON_CROSS)
	MAP(square, NINTENDONT_BUTTON_SQUARE)
	MAP(up, NINTENDONT_BUTTON_UP)
	MAP(down, NINTENDONT_BUTTON_DOWN)
	MAP(left, NINTENDONT_BUTTON_LEFT)
	MAP(right, NINTENDONT_BUTTON_RIGHT)
	MAP(r3, NINTENDONT_BUTTON_R3)
	MAP(l3, NINTENDONT_BUTTON_L3)
	MAP(start, NINTENDONT_BUTTON_START)
	MAP(select, NINTENDONT_BUTTON_SELECT)
	MAP(r2, NINTENDONT_BUTTON_R2)
	MAP(l2, NINTENDONT_BUTTON_L2)
	MAP(r1, NINTENDONT_BUTTON_R1)
	MAP(l1, NINTENDONT_BUTTON_L1)
	MAP(ps, NINTENDONT_BUTTON_PS)
#undef MAP
#endif
	*buttons = mask;
}

static inline void nintendont_get_analog_axis(const u8 *report,
					      u8 analog_axis[static NINTENDONT_ANALOG_AXIS__NUM])
{
#if 0
	analog_axis[NINTENDONT_ANALOG_AXIS_LEFT_X] = report->left_x;
	analog_axis[NINTENDONT_ANALOG_AXIS_LEFT_Y] = 255 - report->left_y;
	analog_axis[NINTENDONT_ANALOG_AXIS_RIGHT_X] = report->right_x;
	analog_axis[NINTENDONT_ANALOG_AXIS_RIGHT_Y] = 255 - report->right_y;
#endif
}

static inline int nintendont_request_data(usb_input_device_t *device)
{
	return usb_device_driver_issue_ctrl_transfer_async(device,
							   USB_REQTYPE_INTERFACE_GET,
							   USB_REQ_GETREPORT,
							   (USB_REPTYPE_INPUT << 8) | 0x01, 0,
							   device->usb_async_resp,
							   sizeof(device->usb_async_resp));
}

bool nintendont_driver_ops_probe(u16 vid, u16 pid)
{
	static const struct device_id_t compatible[] = {
		{SONY_VID, 0x0268},
	};

	return usb_driver_is_comaptible(vid, pid, compatible, ARRAY_SIZE(compatible));
}

int nintendont_driver_ops_init(usb_input_device_t *device, u16 vid, u16 pid)
{
	int ret;
	struct nintendont_private_data_t *priv = (void *)device->private_data;

	/* Init private state */
	priv->ir_emu_mode_idx = 0;
	bm_ir_emulation_state_reset(&priv->ir_emu_state);
	priv->mapping = 0;
	priv->switch_mapping = false;
	priv->switch_ir_emu_mode = false;

	/* Set initial extension */
	fake_wiimote_set_extension(device->wiimote, input_mappings[priv->mapping].extension);

	ret = nintendont_request_data(device);
	if (ret < 0)
		return ret;

	return 0;
}

bool nintendont_report_input(usb_input_device_t *device)
{
	struct nintendont_private_data_t *priv = (void *)device->private_data;
	u16 wiimote_buttons = 0;
	union wiimote_extension_data_t extension_data;
	struct ir_dot_t ir_dots[IR_MAX_DOTS];
	enum bm_ir_emulation_mode_e ir_emu_mode;

	if (bm_check_switch_mapping(priv->input.buttons, &priv->switch_mapping, SWITCH_MAPPING_COMBO)) {
		priv->mapping = (priv->mapping + 1) % ARRAY_SIZE(input_mappings);
		fake_wiimote_set_extension(device->wiimote, input_mappings[priv->mapping].extension);
		return false;
	} else if (bm_check_switch_mapping(priv->input.buttons, &priv->switch_ir_emu_mode, SWITCH_IR_EMU_MODE_COMBO)) {
		priv->ir_emu_mode_idx = (priv->ir_emu_mode_idx + 1) % ARRAY_SIZE(ir_emu_modes);
		bm_ir_emulation_state_reset(&priv->ir_emu_state);
	}

	bm_map_wiimote(NINTENDONT_BUTTON__NUM, priv->input.buttons,
		       input_mappings[priv->mapping].wiimote_button_map,
		       &wiimote_buttons);

	ir_emu_mode = ir_emu_modes[priv->ir_emu_mode_idx];
	if (ir_emu_mode == BM_IR_EMULATION_MODE_NONE) {
		bm_ir_dots_set_out_of_screen(ir_dots);
	} else {
		bm_map_ir_analog_axis(ir_emu_mode, &priv->ir_emu_state,
				      NINTENDONT_ANALOG_AXIS__NUM, priv->input.analog_axis,
				      ir_analog_axis_map, ir_dots);
	}

	fake_wiimote_report_ir_dots(device->wiimote, ir_dots);

	if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_NONE) {
		fake_wiimote_report_input(device->wiimote, wiimote_buttons);
	} else if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_NUNCHUK) {
		bm_map_nunchuk(NINTENDONT_BUTTON__NUM, priv->input.buttons,
			       NINTENDONT_ANALOG_AXIS__NUM, priv->input.analog_axis,
			       0, 0, 0,
			       input_mappings[priv->mapping].nunchuk_button_map,
			       input_mappings[priv->mapping].nunchuk_analog_axis_map,
			       &extension_data.nunchuk);
		fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
					      &extension_data, sizeof(extension_data.nunchuk));
	} else if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_CLASSIC) {
		bm_map_classic(NINTENDONT_BUTTON__NUM, priv->input.buttons,
			       NINTENDONT_ANALOG_AXIS__NUM, priv->input.analog_axis,
			       input_mappings[priv->mapping].classic_button_map,
			       input_mappings[priv->mapping].classic_analog_axis_map,
			       &extension_data.classic);
		fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
					      &extension_data, sizeof(extension_data.classic));
	}

	return true;
}

int nintendont_driver_ops_usb_async_resp(usb_input_device_t *device)
{
	struct nintendont_private_data_t *priv = (void *)device->private_data;
	//struct nintendont_input_report *report = (void *)device->usb_async_resp;

	nintendont_get_buttons(device->usb_async_resp, &priv->input.buttons);
	nintendont_get_analog_axis(device->usb_async_resp, priv->input.analog_axis);

	return nintendont_request_data(device);
}

const usb_device_driver_t nintendont_usb_device_driver = {
	.probe		= nintendont_driver_ops_probe,
	.init		= nintendont_driver_ops_init,
	.report_input	= nintendont_report_input,
	.usb_async_resp	= nintendont_driver_ops_usb_async_resp,
};
