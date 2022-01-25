#include "button_map.h"
#include "usb_device_drivers.h"
#include "utils.h"
#include "wiimote.h"

#if 0
typedef struct {
  uint8_t  GD_JoystickX;                             // Usage 0x00010030: X, Value = 0 to 255, Physical = Value
  uint8_t  GD_JoystickY;                             // Usage 0x00010031: Y, Value = 0 to 255, Physical = Value
  uint8_t  pad_2;                                    // Pad
  uint8_t  GD_JoystickZ;                             // Usage 0x00010032: Z, Value = 0 to 255, Physical = Value
  uint8_t  GD_JoystickRz;                            // Usage 0x00010035: Rz, Value = 0 to 255, Physical = Value
  uint8_t  GD_JoystickHatSwitch : 4;                 // Usage 0x00010039: Hat switch, Value = 0 to 7, Physical = Value x 45 in degrees
  uint8_t  BTN_JoystickButton1 : 1;                  // Usage 0x00090001: Button 1 Primary/trigger, Value = 0 to 1, Physical = Value
  uint8_t  BTN_JoystickButton2 : 1;                  // Usage 0x00090002: Button 2 Secondary, Value = 0 to 1, Physical = Value
  uint8_t  BTN_JoystickButton3 : 1;                  // Usage 0x00090003: Button 3 Tertiary, Value = 0 to 1, Physical = Value
  uint8_t  BTN_JoystickButton4 : 1;                  // Usage 0x00090004: Button 4, Value = 0 to 1, Physical = Value

  uint8_t  BTN_JoystickButton5 : 1;                  // Usage 0x00090005: Button 5, Value = 0 to 1, Physical = Value
  uint8_t  BTN_JoystickButton6 : 1;                  // Usage 0x00090006: Button 6, Value = 0 to 1, Physical = Value
  uint8_t  BTN_JoystickButton7 : 1;                  // Usage 0x00090007: Button 7, Value = 0 to 1, Physical = Value
  uint8_t  BTN_JoystickButton8 : 1;                  // Usage 0x00090008: Button 8, Value = 0 to 1, Physical = Value
  uint8_t  BTN_JoystickButton9 : 1;                  // Usage 0x00090009: Button 9, Value = 0 to 1, Physical = Value
  uint8_t  BTN_JoystickButton10 : 1;                 // Usage 0x0009000A: Button 10, Value = 0 to 1, Physical = Value
  uint8_t  BTN_JoystickButton11 : 1;                 // Usage 0x0009000B: Button 11, Value = 0 to 1, Physical = Value
  uint8_t  BTN_JoystickButton12 : 1;                 // Usage 0x0009000C: Button 12, Value = 0 to 1, Physical = Value

  uint8_t  VEN_Joystick0001 : 1;                     // Usage 0xFF000001: , Value = 0 to 1, Physical = Value
  uint8_t  VEN_Joystick00011 : 1;                    // Usage 0xFF000001: , Value = 0 to 1, Physical = Value
  uint8_t  VEN_Joystick00012 : 1;                    // Usage 0xFF000001: , Value = 0 to 1, Physical = Value
  uint8_t  VEN_Joystick00013 : 1;                    // Usage 0xFF000001: , Value = 0 to 1, Physical = Value
  uint8_t  VEN_Joystick00014 : 1;                    // Usage 0xFF000001: , Value = 0 to 1, Physical = Value
  uint8_t  VEN_Joystick00015 : 1;                    // Usage 0xFF000001: , Value = 0 to 1, Physical = Value
  uint8_t  VEN_Joystick00016 : 1;                    // Usage 0xFF000001: , Value = 0 to 1, Physical = Value
  uint8_t  VEN_Joystick00017 : 1;                    // Usage 0xFF000001: , Value = 0 to 1, Physical = Value
} inputReport_t;
#endif

struct generic_usb_pid_0006_input_report {
	u8 left_x;
	u8 left_y;
	u8 unused1;
	u8 right_x;
	u8 right_y;
	u8 hat : 4; // Hat switch, Value = 0 to 7, Physical = Value x 45 in degrees
	u8 Y : 1;
	u8 X : 1;
	u8 A : 1;
	u8 B : 1;
	u8 L : 1;
	u8 Z : 1;
	u8 R : 1;
	u8 power : 1;
	u8 S : 1;
} ATTRIBUTE_PACKED;

enum generic_usb_pid_0006_buttons_e {
	BUTTON_TRIANGLE,
	BUTTON_CIRCLE,
	BUTTON_CROSS,
	BUTTON_SQUARE,
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_LEFT,
	BUTTON_RIGHT,
	BUTTON_R3,
	BUTTON_L3,
	BUTTON_START,
	BUTTON_SELECT,
	BUTTON_R2,
	BUTTON_L2,
	BUTTON_R1,
	BUTTON_L1,
	BUTTON_PS,
	BUTTON__NUM
};

enum generic_usb_pid_0006_analog_axis_e {
	ANALOG_AXIS_LEFT_X,
	ANALOG_AXIS_LEFT_Y,
	ANALOG_AXIS_RIGHT_X,
	ANALOG_AXIS_RIGHT_Y,
	ANALOG_AXIS__NUM
};

struct generic_usb_pid_0006_private_data_t {
	struct {
		u32 buttons;
		u8 analog_axis[ANALOG_AXIS__NUM];
	} input;
	enum bm_ir_emulation_mode_e ir_emu_mode;
	struct bm_ir_emulation_state_t ir_emu_state;
	u8 mapping;
	u8 ir_emu_mode_idx;
	bool switch_mapping;
	bool switch_ir_emu_mode;
};
static_assert(sizeof(struct generic_usb_pid_0006_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

#define SWITCH_MAPPING_COMBO		(BIT(BUTTON_L1) | BIT(BUTTON_L3))
#define SWITCH_IR_EMU_MODE_COMBO	(BIT(BUTTON_R1) | BIT(BUTTON_R3))

static const struct {
	enum wiimote_ext_e extension;
	u16 wiimote_button_map[BUTTON__NUM];
	u8 nunchuk_button_map[BUTTON__NUM];
	u8 nunchuk_analog_axis_map[ANALOG_AXIS__NUM];
	u16 classic_button_map[BUTTON__NUM];
	u8 classic_analog_axis_map[ANALOG_AXIS__NUM];
} input_mappings[] = {
	{
		.extension = WIIMOTE_EXT_NUNCHUK,
		.wiimote_button_map = {
			[BUTTON_TRIANGLE] = WIIMOTE_BUTTON_ONE,
			[BUTTON_CIRCLE]   = WIIMOTE_BUTTON_B,
			[BUTTON_CROSS]    = WIIMOTE_BUTTON_A,
			[BUTTON_SQUARE]   = WIIMOTE_BUTTON_TWO,
			[BUTTON_UP]       = WIIMOTE_BUTTON_UP,
			[BUTTON_DOWN]     = WIIMOTE_BUTTON_DOWN,
			[BUTTON_LEFT]     = WIIMOTE_BUTTON_LEFT,
			[BUTTON_RIGHT]    = WIIMOTE_BUTTON_RIGHT,
			[BUTTON_START]    = WIIMOTE_BUTTON_PLUS,
			[BUTTON_SELECT]   = WIIMOTE_BUTTON_MINUS,
			[BUTTON_PS]       = WIIMOTE_BUTTON_HOME,
		},
		.nunchuk_button_map = {
			[BUTTON_L1] = NUNCHUK_BUTTON_C,
			[BUTTON_L2] = NUNCHUK_BUTTON_Z,
		},
		.nunchuk_analog_axis_map = {
			[ANALOG_AXIS_LEFT_X] = BM_NUNCHUK_ANALOG_AXIS_X,
			[ANALOG_AXIS_LEFT_Y] = BM_NUNCHUK_ANALOG_AXIS_Y,
		},
	},
	{
		.extension = WIIMOTE_EXT_CLASSIC,
		.classic_button_map = {
			[BUTTON_TRIANGLE] = CLASSIC_CTRL_BUTTON_X,
			[BUTTON_CIRCLE]   = CLASSIC_CTRL_BUTTON_A,
			[BUTTON_CROSS]    = CLASSIC_CTRL_BUTTON_B,
			[BUTTON_SQUARE]   = CLASSIC_CTRL_BUTTON_Y,
			[BUTTON_UP]       = CLASSIC_CTRL_BUTTON_UP,
			[BUTTON_DOWN]     = CLASSIC_CTRL_BUTTON_DOWN,
			[BUTTON_LEFT]     = CLASSIC_CTRL_BUTTON_LEFT,
			[BUTTON_RIGHT]    = CLASSIC_CTRL_BUTTON_RIGHT,
			[BUTTON_START]    = CLASSIC_CTRL_BUTTON_PLUS,
			[BUTTON_SELECT]   = CLASSIC_CTRL_BUTTON_MINUS,
			[BUTTON_R2]       = CLASSIC_CTRL_BUTTON_ZR,
			[BUTTON_L2]       = CLASSIC_CTRL_BUTTON_ZL,
			[BUTTON_R1]       = CLASSIC_CTRL_BUTTON_FULL_R,
			[BUTTON_L1]       = CLASSIC_CTRL_BUTTON_FULL_L,
			[BUTTON_PS]       = CLASSIC_CTRL_BUTTON_HOME,
		},
		.classic_analog_axis_map = {
			[ANALOG_AXIS_LEFT_X]  = BM_CLASSIC_ANALOG_AXIS_LEFT_X,
			[ANALOG_AXIS_LEFT_Y]  = BM_CLASSIC_ANALOG_AXIS_LEFT_Y,
			[ANALOG_AXIS_RIGHT_X] = BM_CLASSIC_ANALOG_AXIS_RIGHT_X,
			[ANALOG_AXIS_RIGHT_Y] = BM_CLASSIC_ANALOG_AXIS_RIGHT_Y,
		},
	},
};

static const u8 ir_analog_axis_map[ANALOG_AXIS__NUM] = {
	[ANALOG_AXIS_RIGHT_X] = BM_IR_AXIS_X,
	[ANALOG_AXIS_RIGHT_Y] = BM_IR_AXIS_Y,
};

static const enum bm_ir_emulation_mode_e ir_emu_modes[] = {
	BM_IR_EMULATION_MODE_ABSOLUTE_ANALOG_AXIS,
	BM_IR_EMULATION_MODE_RELATIVE_ANALOG_AXIS,
	BM_IR_EMULATION_MODE_NONE,
};

static inline void generic_usb_pid_0006_get_buttons(const struct generic_usb_pid_0006_input_report *report,
						    u32 *buttons)
{
	u32 mask = 0;

#define MAP(field, button) \
	if (report->field) \
		mask |= BIT(button);

	MAP(Y, BUTTON_TRIANGLE)
	MAP(B, BUTTON_CIRCLE)
	MAP(A, BUTTON_CROSS)
	MAP(X, BUTTON_SQUARE)
	/*MAP(up, BUTTON_UP)
	MAP(down, BUTTON_DOWN)
	MAP(left, BUTTON_LEFT)
	MAP(right, BUTTON_RIGHT)*/
	MAP(R, BUTTON_R3)
	MAP(L, BUTTON_L3)
	MAP(Z, BUTTON_START)
	MAP(S, BUTTON_SELECT)
	MAP(power, BUTTON_PS)
#undef MAP

	*buttons = mask;
}

static inline void generic_usb_pid_0006_get_analog_axis(const struct generic_usb_pid_0006_input_report *report,
				                        u8 analog_axis[static ANALOG_AXIS__NUM])
{
	analog_axis[ANALOG_AXIS_LEFT_X] = report->left_x;
	analog_axis[ANALOG_AXIS_LEFT_Y] = 255 - report->left_y;
	analog_axis[ANALOG_AXIS_RIGHT_X] = report->right_x;
	analog_axis[ANALOG_AXIS_RIGHT_Y] = 255 - report->right_y;
}

static inline int generic_usb_pid_0006_request_data(usb_input_device_t *device)
{
	return usb_device_driver_issue_intr_transfer_async(device, 0, device->usb_async_resp,
							   sizeof(device->usb_async_resp));
}

bool generic_usb_pid_0006_driver_ops_probe(u16 vid, u16 pid)
{
	static const struct device_id_t compatible[] = {
		{0x0079, 0x0006},
	};

	return usb_driver_is_comaptible(vid, pid, compatible, ARRAY_SIZE(compatible));
}

int generic_usb_pid_0006_driver_ops_init(usb_input_device_t *device, u16 vid, u16 pid)
{
	struct generic_usb_pid_0006_private_data_t *priv = (void *)device->private_data;

	/* Init private state */
	priv->ir_emu_mode_idx = 0;
	bm_ir_emulation_state_reset(&priv->ir_emu_state);
	priv->mapping = 0;
	priv->switch_mapping = false;
	priv->switch_ir_emu_mode = false;

	/* Set initial extension */
	fake_wiimote_set_extension(device->wiimote, input_mappings[priv->mapping].extension);

	return generic_usb_pid_0006_request_data(device);
}

bool generic_usb_pid_0006_report_input(usb_input_device_t *device)
{
	struct generic_usb_pid_0006_private_data_t *priv = (void *)device->private_data;
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

	bm_map_wiimote(BUTTON__NUM, priv->input.buttons,
		       input_mappings[priv->mapping].wiimote_button_map,
		       &wiimote_buttons);

	ir_emu_mode = ir_emu_modes[priv->ir_emu_mode_idx];
	if (ir_emu_mode == BM_IR_EMULATION_MODE_NONE) {
		bm_ir_dots_set_out_of_screen(ir_dots);
	} else {
		bm_map_ir_analog_axis(ir_emu_mode, &priv->ir_emu_state,
				      ANALOG_AXIS__NUM, priv->input.analog_axis,
				      ir_analog_axis_map, ir_dots);
	}

	fake_wiimote_report_ir_dots(device->wiimote, ir_dots);

	if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_NONE) {
		fake_wiimote_report_input(device->wiimote, wiimote_buttons);
	} else if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_NUNCHUK) {
		bm_map_nunchuk(BUTTON__NUM, priv->input.buttons,
			       ANALOG_AXIS__NUM, priv->input.analog_axis,
			       0, 0, 0,
			       input_mappings[priv->mapping].nunchuk_button_map,
			       input_mappings[priv->mapping].nunchuk_analog_axis_map,
			       &extension_data.nunchuk);
		fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
					      &extension_data, sizeof(extension_data.nunchuk));
	} else if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_CLASSIC) {
		bm_map_classic(BUTTON__NUM, priv->input.buttons,
			       ANALOG_AXIS__NUM, priv->input.analog_axis,
			       input_mappings[priv->mapping].classic_button_map,
			       input_mappings[priv->mapping].classic_analog_axis_map,
			       &extension_data.classic);
		fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
					      &extension_data, sizeof(extension_data.classic));
	}

	return true;
}

int generic_usb_pid_0006_driver_ops_usb_async_resp(usb_input_device_t *device)
{
	struct generic_usb_pid_0006_private_data_t *priv = (void *)device->private_data;
	struct generic_usb_pid_0006_input_report *report = (void *)device->usb_async_resp;

	generic_usb_pid_0006_get_buttons(report, &priv->input.buttons);
	generic_usb_pid_0006_get_analog_axis(report, priv->input.analog_axis);

	return generic_usb_pid_0006_request_data(device);
}

const usb_device_driver_t generic_usb_pid_0006_usb_device_driver = {
	.probe		= generic_usb_pid_0006_driver_ops_probe,
	.init		= generic_usb_pid_0006_driver_ops_init,
	.report_input	= generic_usb_pid_0006_report_input,
	.usb_async_resp	= generic_usb_pid_0006_driver_ops_usb_async_resp,
};
