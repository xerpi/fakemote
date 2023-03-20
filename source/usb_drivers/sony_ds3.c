#include "button_map.h"
#include "usb_device_drivers.h"
#include "usb.h"
#include "utils.h"
#include "wiimote.h"

#define DS3_ACC_RES_PER_G	113

struct ds3_input_report {
	u8 report_id;
	u8 unk0;

	u8 left     : 1;
	u8 down     : 1;
	u8 right    : 1;
	u8 up       : 1;
	u8 start    : 1;
	u8 r3       : 1;
	u8 l3       : 1;
	u8 select   : 1;

	u8 square   : 1;
	u8 cross    : 1;
	u8 circle   : 1;
	u8 triangle : 1;
	u8 r1       : 1;
	u8 l1       : 1;
	u8 r2       : 1;
	u8 l2       : 1;

	u8 not_used : 7;
	u8 ps       : 1;

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
	u8  unk4;

	u8 status;
	u8 power_rating;
	u8 comm_status;

	u32 unk5;
	u32 unk6;
	u8  unk7;

	u16 acc_x;
	u16 acc_y;
	u16 acc_z;
	u16 z_gyro;
} ATTRIBUTE_PACKED;

struct guitar_input_report {
	u8 			: 2;
	u8 pedal	: 1;
	u8 orange	: 1;
	u8 blue		: 1;
	u8 red		: 1;
	u8 green 	: 1;
	u8 yellow	: 1;

	u8			: 3;
	u8 ps		: 1;
	u8			: 2;
	u8 start	: 1;
	u8 select	: 1;

	u8 hat;

	u8 unused0;
	u8 unused1;
	u8 whammy_bar;
	u8 tap_bar;

	u8 pressure_dpadRight_yellow;
	u8 pressure_dpadLeft;
	u8 pressure_dpadUp_green;
	u8 pressure_dpadDown_orange;
	u8 pressure_blue;
	u8 pressure_red;
	u8 unused2[6];

	// Reminder that these values are 10-bit in range
	u16 acc_x;
	u16 acc_z;
	u16 acc_y;
	u16 z_gyro;

} ATTRIBUTE_PACKED;
struct drum_input_report {
	u8			: 2;
	u8 kick		: 1;
	u8 orange	: 1;
	u8 yellow	: 1;
	u8 red		: 1;
	u8 green	: 1;
	u8 blue		: 1;

	u8			: 3;
	u8 ps		: 1;
	u8			: 2;
	u8 start	: 1;
	u8 select	: 1;

	u8 hat;
	u8 unused;
	u8 unused2;
	u8 whammy_bar;
	u8 tap_bar;
	u8 pressure_yellow;
	u8 pressure_red;
	u8 pressure_green;
	u8 pressure_blue;
	u8 pressure_kick;
	u8 pressure_orange;
	u8 unused3[2];
	u16 unused4[4];

} ATTRIBUTE_PACKED;
struct turntable_input_report {
	u8			: 4;
	u8 triangle_euphoria : 1;
	u8 circle	: 1;
	u8 cross	: 1;
	u8 square	: 1;

	u8			: 3;
	u8 ps		: 1;
	u8			: 2;
	u8 start	: 1;
	u8 select	: 1;

	u8 hat;
	u8 unused;
	u8 unused2;
	u8 left_turn_table_velocity;
	u8 right_turn_table_velocity;
	u8 pressure_yellow;
	u8 pressure_red;
	u8 pressure_green;
	u8 pressure_blue;
	u8 pressure_kick;
	u8 pressure_orange;
	u8 unused3[2];
	u16 effects_knob;
	u16 cross_fader;
	u16 right_green : 1;
	u16 right_red : 1;
	u16 right_blue : 1;
	u16 left_green : 1;
	u16 left_red : 1;
	u16 left_blue : 1;
	u16 : 3;
	u16 table_neutral : 1;
	u16 : 6;
	u16 : 16;

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
	DS3_BUTTON__NUM
};

enum guitar_buttons_e {
	GUITAR_BUTTON_YELLOW,
	GUITAR_BUTTON_GREEN,
	GUITAR_BUTTON_RED,
	GUITAR_BUTTON_BLUE,
	GUITAR_BUTTON_ORANGE,
	GUITAR_BUTTON_UP,
	GUITAR_BUTTON_DOWN,
	GUITAR_BUTTON_LEFT,
	GUITAR_BUTTON_RIGHT,
	GUITAR_BUTTON_SP_PEDAL,
	GUITAR_BUTTON_SELECT,
	GUITAR_BUTTON_START,
	GUITAR_BUTTON_PS,
	GUITAR_BUTTON__NUM
};

enum turntable_buttons_e {
	TURNTABLE_BUTTON_SQUARE,
	TURNTABLE_BUTTON_CROSS,
	TURNTABLE_BUTTON_CIRCLE,
	TURNTABLE_BUTTON_TRIANGLE_EUPHORIA,
	TURNTABLE_BUTTON_SELECT,
	TURNTABLE_BUTTON_START,
	TURNTABLE_BUTTON_LEFT_GREEN,
	TURNTABLE_BUTTON_LEFT_RED,
	TURNTABLE_BUTTON_LEFT_BLUE,
	TURNTABLE_BUTTON_RIGHT_GREEN,
	TURNTABLE_BUTTON_RIGHT_RED,
	TURNTABLE_BUTTON_RIGHT_BLUE,
	TURNTABLE_BUTTON_UP,
	TURNTABLE_BUTTON_DOWN,
	TURNTABLE_BUTTON_LEFT,
	TURNTABLE_BUTTON_RIGHT,
	TURNTABLE_BUTTON_PS,
	TURNTABLE_BUTTON__NUM
};

enum drum_buttons_e {
	DRUM_BUTTON_YELLOW,
	DRUM_BUTTON_GREEN,
	DRUM_BUTTON_RED,
	DRUM_BUTTON_BLUE,
	DRUM_BUTTON_ORANGE,
	DRUM_BUTTON_UP,
	DRUM_BUTTON_DOWN,
	DRUM_BUTTON_LEFT,
	DRUM_BUTTON_RIGHT,
	DRUM_BUTTON_KICK,
	DRUM_BUTTON_SELECT,
	DRUM_BUTTON_START,
	DRUM_BUTTON_PS,
	DRUM_BUTTON__NUM
};

enum ds3_analog_axis_e {
	DS3_ANALOG_AXIS_LEFT_X,
	DS3_ANALOG_AXIS_LEFT_Y,
	DS3_ANALOG_AXIS_RIGHT_X,
	DS3_ANALOG_AXIS_RIGHT_Y,
	DS3_ANALOG_AXIS__NUM
};

enum guitar_analog_axis_e {
	GUITAR_ANALOG_AXIS_TAP_BAR,
	GUITAR_ANALOG_AXIS_WHAMMY_BAR,
	GUITAR_ANALOG_AXIS__NUM
};

enum drum_analog_axis_e {
	DRUM_ANALOG_AXIS_GREEN,
	DRUM_ANALOG_AXIS_RED,
	DRUM_ANALOG_AXIS_YELLOW,
	DRUM_ANALOG_AXIS_BLUE,
	DRUM_ANALOG_AXIS_ORANGE,
	DRUM_ANALOG_AXIS_KICK,
	DRUM_ANALOG_AXIS__NUM
};

enum turntable_analog_axis_e {
	TURNTABLE_ANALOG_AXIS_LEFT_VELOCITY,
	TURNTABLE_ANALOG_AXIS_RIGHT_VELOCITY,
	TURNTABLE_ANALOG_AXIS_CROSS_FADER,
	TURNTABLE_ANALOG_AXIS_EFFECTS_KNOB,
	TURNTABLE_ANALOG_AXIS__NUM
};

#define MAX_ANALOG_AXIS DRUM_ANALOG_AXIS__NUM

struct ds3_private_data_t {
	struct {
		u32 buttons;
		u8 analog_axis[MAX_ANALOG_AXIS];
		s16 acc_x, acc_y, acc_z;
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
static_assert(sizeof(struct ds3_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

#define SWITCH_MAPPING_COMBO		(BIT(DS3_BUTTON_L1) | BIT(DS3_BUTTON_L3))
#define SWITCH_IR_EMU_MODE_COMBO	(BIT(DS3_BUTTON_R1) | BIT(DS3_BUTTON_R3))

static const struct {
	enum wiimote_ext_e extension;
	u16 wiimote_button_map[DS3_BUTTON__NUM];
	u8 nunchuk_button_map[DS3_BUTTON__NUM];
	u8 nunchuk_analog_axis_map[DS3_ANALOG_AXIS__NUM];
	u16 classic_button_map[DS3_BUTTON__NUM];
	u8 classic_analog_axis_map[DS3_ANALOG_AXIS__NUM];
} input_mappings[] = {
	{
		.extension = WIIMOTE_EXT_NUNCHUK,
		.wiimote_button_map = {
			[DS3_BUTTON_TRIANGLE] = WIIMOTE_BUTTON_ONE,
			[DS3_BUTTON_CIRCLE]   = WIIMOTE_BUTTON_B,
			[DS3_BUTTON_CROSS]    = WIIMOTE_BUTTON_A,
			[DS3_BUTTON_SQUARE]   = WIIMOTE_BUTTON_TWO,
			[DS3_BUTTON_UP]       = WIIMOTE_BUTTON_UP,
			[DS3_BUTTON_DOWN]     = WIIMOTE_BUTTON_DOWN,
			[DS3_BUTTON_LEFT]     = WIIMOTE_BUTTON_LEFT,
			[DS3_BUTTON_RIGHT]    = WIIMOTE_BUTTON_RIGHT,
			[DS3_BUTTON_START]    = WIIMOTE_BUTTON_PLUS,
			[DS3_BUTTON_SELECT]   = WIIMOTE_BUTTON_MINUS,
			[DS3_BUTTON_PS]       = WIIMOTE_BUTTON_HOME,
		},
		.nunchuk_button_map = {
			[DS3_BUTTON_L1] = NUNCHUK_BUTTON_C,
			[DS3_BUTTON_L2] = NUNCHUK_BUTTON_Z,
		},
		.nunchuk_analog_axis_map = {
			[DS3_ANALOG_AXIS_LEFT_X] = BM_NUNCHUK_ANALOG_AXIS_X,
			[DS3_ANALOG_AXIS_LEFT_Y] = BM_NUNCHUK_ANALOG_AXIS_Y,
		},
	},
	{
		.extension = WIIMOTE_EXT_CLASSIC,
		.classic_button_map = {
			[DS3_BUTTON_TRIANGLE] = CLASSIC_CTRL_BUTTON_X,
			[DS3_BUTTON_CIRCLE]   = CLASSIC_CTRL_BUTTON_A,
			[DS3_BUTTON_CROSS]    = CLASSIC_CTRL_BUTTON_B,
			[DS3_BUTTON_SQUARE]   = CLASSIC_CTRL_BUTTON_Y,
			[DS3_BUTTON_UP]       = CLASSIC_CTRL_BUTTON_UP,
			[DS3_BUTTON_DOWN]     = CLASSIC_CTRL_BUTTON_DOWN,
			[DS3_BUTTON_LEFT]     = CLASSIC_CTRL_BUTTON_LEFT,
			[DS3_BUTTON_RIGHT]    = CLASSIC_CTRL_BUTTON_RIGHT,
			[DS3_BUTTON_START]    = CLASSIC_CTRL_BUTTON_PLUS,
			[DS3_BUTTON_SELECT]   = CLASSIC_CTRL_BUTTON_MINUS,
			[DS3_BUTTON_R2]       = CLASSIC_CTRL_BUTTON_ZR,
			[DS3_BUTTON_L2]       = CLASSIC_CTRL_BUTTON_ZL,
			[DS3_BUTTON_R1]       = CLASSIC_CTRL_BUTTON_FULL_R,
			[DS3_BUTTON_L1]       = CLASSIC_CTRL_BUTTON_FULL_L,
			[DS3_BUTTON_PS]       = CLASSIC_CTRL_BUTTON_HOME,
		},
		.classic_analog_axis_map = {
			[DS3_ANALOG_AXIS_LEFT_X]  = BM_CLASSIC_ANALOG_AXIS_LEFT_X,
			[DS3_ANALOG_AXIS_LEFT_Y]  = BM_CLASSIC_ANALOG_AXIS_LEFT_Y,
			[DS3_ANALOG_AXIS_RIGHT_X] = BM_CLASSIC_ANALOG_AXIS_RIGHT_X,
			[DS3_ANALOG_AXIS_RIGHT_Y] = BM_CLASSIC_ANALOG_AXIS_RIGHT_Y,
		},
	},
};

static const struct {
	u16 wiimote_button_map[TURNTABLE_BUTTON__NUM];
	u16 turntable_button_map[TURNTABLE_BUTTON__NUM];
	u8 turntable_analog_axis_map[TURNTABLE_ANALOG_AXIS__NUM];
} turntable_mapping =
	{
		.wiimote_button_map = {
			[TURNTABLE_BUTTON_PS] = WIIMOTE_BUTTON_HOME,
		},
		.turntable_analog_axis_map = {
			[TURNTABLE_ANALOG_AXIS_LEFT_VELOCITY] = BM_TURNTABLE_ANALOG_LEFT_TURNTABLE_VELOCITY,
			[TURNTABLE_ANALOG_AXIS_RIGHT_VELOCITY] = BM_TURNTABLE_ANALOG_RIGHT_TURNTABLE_VELOCITY,
			[TURNTABLE_ANALOG_AXIS_CROSS_FADER] = BM_TURNTABLE_ANALOG_CROSS_FADER,
			[TURNTABLE_ANALOG_AXIS_EFFECTS_KNOB] = BM_TURNTABLE_ANALOG_EFFECTS_DIAL,
		},
		.turntable_button_map = {
			[TURNTABLE_BUTTON_LEFT_GREEN] = TURNTABLE_CTRL_BUTTON_LEFT_GREEN,
			[TURNTABLE_BUTTON_LEFT_RED] = TURNTABLE_CTRL_BUTTON_LEFT_RED,
			[TURNTABLE_BUTTON_LEFT_BLUE] = TURNTABLE_CTRL_BUTTON_LEFT_BLUE,
			[TURNTABLE_BUTTON_RIGHT_GREEN] = TURNTABLE_CTRL_BUTTON_RIGHT_GREEN,
			[TURNTABLE_BUTTON_RIGHT_RED] = TURNTABLE_CTRL_BUTTON_RIGHT_RED,
			[TURNTABLE_BUTTON_RIGHT_BLUE] = TURNTABLE_CTRL_BUTTON_RIGHT_BLUE,
			[TURNTABLE_BUTTON_CROSS] = TURNTABLE_CTRL_BUTTON_LEFT_GREEN | TURNTABLE_CTRL_BUTTON_RIGHT_GREEN,
			[TURNTABLE_BUTTON_CIRCLE] = TURNTABLE_CTRL_BUTTON_LEFT_RED | TURNTABLE_CTRL_BUTTON_RIGHT_RED,
			[TURNTABLE_BUTTON_SQUARE] = TURNTABLE_CTRL_BUTTON_LEFT_BLUE | TURNTABLE_CTRL_BUTTON_RIGHT_BLUE,
			[TURNTABLE_BUTTON_START] = TURNTABLE_CTRL_BUTTON_PLUS,
			[TURNTABLE_BUTTON_SELECT] = TURNTABLE_CTRL_BUTTON_MINUS,
		}};

static const struct {
	u16 wiimote_button_map[GUITAR_BUTTON__NUM];
	u16 guitar_button_map[GUITAR_BUTTON__NUM];
	u8 guitar_analog_axis_map[GUITAR_ANALOG_AXIS__NUM];
} guitar_mapping =
	{
		.wiimote_button_map = {
			[GUITAR_BUTTON_PS] = WIIMOTE_BUTTON_HOME,
		},
		.guitar_analog_axis_map = {
			[GUITAR_ANALOG_AXIS_TAP_BAR] = BM_GUITAR_ANALOG_AXIS_TAP_BAR,
			[GUITAR_ANALOG_AXIS_WHAMMY_BAR] = BM_GUITAR_ANALOG_AXIS_WHAMMY_BAR,
		},
		.guitar_button_map = {
			[GUITAR_BUTTON_YELLOW] = GUITAR_CTRL_BUTTON_YELLOW,
			[GUITAR_BUTTON_RED] = GUITAR_CTRL_BUTTON_RED,
			[GUITAR_BUTTON_GREEN] = GUITAR_CTRL_BUTTON_GREEN,
			[GUITAR_BUTTON_BLUE] = GUITAR_CTRL_BUTTON_BLUE,
			[GUITAR_BUTTON_ORANGE] = GUITAR_CTRL_BUTTON_ORANGE,
			[GUITAR_BUTTON_UP] = GUITAR_CTRL_BUTTON_STRUM_UP,
			[GUITAR_BUTTON_DOWN] = GUITAR_CTRL_BUTTON_STRUM_DOWN,
			[GUITAR_BUTTON_START] = GUITAR_CTRL_BUTTON_PLUS,
			[GUITAR_BUTTON_SELECT] = GUITAR_CTRL_BUTTON_MINUS,
		}};

static const struct {
	u16 wiimote_button_map[DRUM_BUTTON__NUM];
	u16 drum_button_map[DRUM_BUTTON__NUM];
} drum_mapping =
	{
		.wiimote_button_map = {
			[DRUM_BUTTON_PS] = WIIMOTE_BUTTON_HOME,
		},
		.drum_button_map = {
			[DRUM_BUTTON_YELLOW] = DRUM_CTRL_BUTTON_YELLOW,
			[DRUM_BUTTON_RED] = DRUM_CTRL_BUTTON_RED,
			[DRUM_BUTTON_GREEN] = DRUM_CTRL_BUTTON_GREEN,
			[DRUM_BUTTON_BLUE] = DRUM_CTRL_BUTTON_BLUE,
			[DRUM_BUTTON_ORANGE] = DRUM_CTRL_BUTTON_ORANGE,
			[DRUM_BUTTON_UP] = DRUM_CTRL_BUTTON_UP,
			[DRUM_BUTTON_DOWN] = DRUM_CTRL_BUTTON_DOWN,
			[DRUM_BUTTON_LEFT] = DRUM_CTRL_BUTTON_LEFT,
			[DRUM_BUTTON_RIGHT] = DRUM_CTRL_BUTTON_RIGHT,
			[DRUM_BUTTON_START] = DRUM_CTRL_BUTTON_PLUS,
			[DRUM_BUTTON_SELECT] = DRUM_CTRL_BUTTON_MINUS,
		}};

static const u8 ir_analog_axis_map[DS3_ANALOG_AXIS__NUM] = {
	[DS3_ANALOG_AXIS_RIGHT_X] = BM_IR_AXIS_X,
	[DS3_ANALOG_AXIS_RIGHT_Y] = BM_IR_AXIS_Y,
};

static const enum bm_ir_emulation_mode_e ir_emu_modes[] = {
	BM_IR_EMULATION_MODE_ABSOLUTE_ANALOG_AXIS,
	BM_IR_EMULATION_MODE_RELATIVE_ANALOG_AXIS,
	BM_IR_EMULATION_MODE_NONE,
};

static inline void ds3_get_buttons(const struct ds3_input_report *report, u32 *buttons)
{
	u32 mask = 0;

#define MAP(field, button) \
	if (report->field) \
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

	*buttons = mask;
}

static inline void guitar_get_buttons(const struct guitar_input_report *report, u32 *buttons) {
	u32 mask = 0;

#define MAP(field, button) \
	if (report->field)	 \
		mask |= BIT(button);

	MAP(green, GUITAR_BUTTON_GREEN)
	MAP(red, GUITAR_BUTTON_RED)
	MAP(yellow, GUITAR_BUTTON_YELLOW)
	MAP(blue, GUITAR_BUTTON_BLUE)
	MAP(orange, GUITAR_BUTTON_ORANGE)
	MAP(start, GUITAR_BUTTON_START)
	MAP(select, GUITAR_BUTTON_SELECT)
	MAP(pedal, GUITAR_BUTTON_SP_PEDAL)
	MAP(ps, GUITAR_BUTTON_PS)
#undef MAP
	if (report->hat == 0 || report->hat == 1 || report->hat == 7) {
		mask |= BIT(GUITAR_BUTTON_UP);
	}
	if (report->hat == 1 || report->hat == 2 || report->hat == 3) {
		mask |= BIT(GUITAR_BUTTON_LEFT);
	}
	if (report->hat == 3 || report->hat == 4 || report->hat == 5) {
		mask |= BIT(GUITAR_BUTTON_DOWN);
	}
	if (report->hat == 5 || report->hat == 6 || report->hat == 7) {
		mask |= BIT(GUITAR_BUTTON_RIGHT);
	}

	*buttons = mask;
}

static inline void drum_get_buttons(const struct drum_input_report *report, u32 *buttons) {
	u32 mask = 0;

#define MAP(field, button) \
	if (report->field)	 \
		mask |= BIT(button);

	MAP(green, DRUM_BUTTON_GREEN)
	MAP(red, DRUM_BUTTON_RED)
	MAP(yellow, DRUM_BUTTON_YELLOW)
	MAP(blue, DRUM_BUTTON_BLUE)
	MAP(orange, DRUM_BUTTON_ORANGE)
	MAP(start, DRUM_BUTTON_START)
	MAP(select, DRUM_BUTTON_SELECT)
	MAP(kick, DRUM_BUTTON_KICK)
	MAP(ps, DRUM_BUTTON_PS)
#undef MAP
	if (report->hat == 0 || report->hat == 1 || report->hat == 7) {
		mask |= BIT(DRUM_BUTTON_UP);
	}
	if (report->hat == 1 || report->hat == 2 || report->hat == 3) {
		mask |= BIT(DRUM_BUTTON_LEFT);
	}
	if (report->hat == 3 || report->hat == 4 || report->hat == 5) {
		mask |= BIT(DRUM_BUTTON_DOWN);
	}
	if (report->hat == 5 || report->hat == 6 || report->hat == 7) {
		mask |= BIT(DRUM_BUTTON_RIGHT);
	}

	*buttons = mask;
}

static inline void turntable_get_buttons(const struct turntable_input_report *report, u32 *buttons) {
	u32 mask = 0;

#define MAP(field, button) \
	if (report->field)	 \
		mask |= BIT(button);

	MAP(left_green, TURNTABLE_BUTTON_LEFT_GREEN)
	MAP(left_red, TURNTABLE_BUTTON_LEFT_RED)
	MAP(left_blue, TURNTABLE_BUTTON_LEFT_BLUE)
	MAP(right_green, TURNTABLE_BUTTON_RIGHT_GREEN)
	MAP(right_red, TURNTABLE_BUTTON_RIGHT_RED)
	MAP(right_blue, TURNTABLE_BUTTON_RIGHT_BLUE)
	MAP(cross, TURNTABLE_BUTTON_CROSS)
	MAP(circle, TURNTABLE_BUTTON_CIRCLE)
	MAP(square, TURNTABLE_BUTTON_SQUARE)
	MAP(triangle_euphoria, TURNTABLE_BUTTON_TRIANGLE_EUPHORIA)
	MAP(select, TURNTABLE_BUTTON_SELECT)
	MAP(ps, TURNTABLE_BUTTON_PS)
#undef MAP
	if (report->hat == 0 || report->hat == 1 || report->hat == 7) {
		mask |= BIT(TURNTABLE_BUTTON_UP);
	}
	if (report->hat == 1 || report->hat == 2 || report->hat == 3) {
		mask |= BIT(TURNTABLE_BUTTON_LEFT);
	}
	if (report->hat == 3 || report->hat == 4 || report->hat == 5) {
		mask |= BIT(TURNTABLE_BUTTON_DOWN);
	}
	if (report->hat == 5 || report->hat == 6 || report->hat == 7) {
		mask |= BIT(TURNTABLE_BUTTON_RIGHT);
	}

	*buttons = mask;
}


static inline void ds3_get_analog_axis(const struct ds3_input_report *report,
				       u8 analog_axis[static DS3_ANALOG_AXIS__NUM])
{
	analog_axis[DS3_ANALOG_AXIS_LEFT_X] = report->left_x;
	analog_axis[DS3_ANALOG_AXIS_LEFT_Y] = 255 - report->left_y;
	analog_axis[DS3_ANALOG_AXIS_RIGHT_X] = report->right_x;
	analog_axis[DS3_ANALOG_AXIS_RIGHT_Y] = 255 - report->right_y;
}

static inline void guitar_get_analog_axis(const struct guitar_input_report *report,
										  u8 analog_axis[static MAX_ANALOG_AXIS]) {
	analog_axis[GUITAR_ANALOG_AXIS_TAP_BAR] = report->tap_bar;
	analog_axis[GUITAR_ANALOG_AXIS_WHAMMY_BAR] = report->whammy_bar;
}

static inline void drum_get_analog_axis(const struct drum_input_report *report,
										u8 analog_axis[static MAX_ANALOG_AXIS]) {
	analog_axis[DRUM_ANALOG_AXIS_GREEN] = report->pressure_green;
	analog_axis[DRUM_ANALOG_AXIS_RED] = report->pressure_red;
	analog_axis[DRUM_ANALOG_AXIS_YELLOW] = report->pressure_yellow;
	analog_axis[DRUM_ANALOG_AXIS_BLUE] = report->pressure_blue;
	analog_axis[DRUM_ANALOG_AXIS_ORANGE] = report->pressure_orange;
	analog_axis[DRUM_ANALOG_AXIS_KICK] = report->pressure_kick;
}

static inline void turntable_get_analog_axis(const struct turntable_input_report *report,
										u8 analog_axis[static MAX_ANALOG_AXIS]) {
	analog_axis[TURNTABLE_ANALOG_AXIS_CROSS_FADER] = report->cross_fader;
	analog_axis[TURNTABLE_ANALOG_AXIS_EFFECTS_KNOB] = report->effects_knob;
	analog_axis[TURNTABLE_ANALOG_AXIS_LEFT_VELOCITY] = report->left_turn_table_velocity;
	analog_axis[TURNTABLE_ANALOG_AXIS_RIGHT_VELOCITY] = report->right_turn_table_velocity;
}

static int ds3_set_operational(usb_input_device_t *device)
{
	u8 buf[17] ATTRIBUTE_ALIGN(32);
	return usb_device_driver_issue_ctrl_transfer(device,
						     USB_REQTYPE_INTERFACE_GET,
						     USB_REQ_GETREPORT,
						     (USB_REPTYPE_FEATURE << 8) | 0xf2, 0,
						     buf, sizeof(buf));
}

static inline int ds3_request_data(usb_input_device_t *device)
{
	return usb_device_driver_issue_ctrl_transfer_async(device,
							   USB_REQTYPE_INTERFACE_GET,
							   USB_REQ_GETREPORT,
							   (USB_REPTYPE_INPUT << 8) | 0x01, 0,
							   device->usb_async_resp,
							   sizeof(device->usb_async_resp));
}

static int ds3_set_leds_rumble(usb_input_device_t *device, u8 leds, const struct ds3_rumble *rumble)
{
	u8 buf[] ATTRIBUTE_ALIGN(32) = {
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

	return usb_device_driver_issue_ctrl_transfer(device,
						     USB_REQTYPE_INTERFACE_SET,
						     USB_REQ_SETREPORT,
						     (USB_REPTYPE_OUTPUT << 8) | 0x01, 0,
						     buf, sizeof(buf));
}

static int ds3_driver_update_leds_rumble(usb_input_device_t *device)
{
	struct ds3_private_data_t *priv = (void *)device->private_data;
	struct ds3_rumble rumble;
	u8 leds;

	static const u8 led_pattern[] = {0x0, 0x02, 0x04, 0x08, 0x10, 0x12, 0x14, 0x18};

	leds = led_pattern[priv->leds % ARRAY_SIZE(led_pattern)];

	rumble.duration_right = priv->rumble_on * 255;
	rumble.power_right = 255;
	rumble.duration_left = 0;
	rumble.power_left = 0;

	return ds3_set_leds_rumble(device, leds, &rumble);
}

bool ds3_driver_ops_probe(u16 vid, u16 pid)
{
	static const struct device_id_t compatible[] = {
		{SONY_VID, 0x0268},
		{SONY_INST_VID, GH_GUITAR_PID},
		{SONY_INST_VID, GH_DRUM_PID},
		{SONY_INST_VID, DJ_TURNTABLE_PID},
	};

	return usb_driver_is_comaptible(vid, pid, compatible, ARRAY_SIZE(compatible));
}

int ds3_driver_ops_init(usb_input_device_t *device, u16 vid, u16 pid)
{
	int ret;
	struct ds3_private_data_t *priv = (void *)device->private_data;

	ret = ds3_set_operational(device);
	if (ret < 0)
		return ret;

	/* Init private state */
	priv->ir_emu_mode_idx = 0;
	bm_ir_emulation_state_reset(&priv->ir_emu_state);
	priv->mapping = 0;
	priv->leds = 0;
	priv->rumble_on = false;
	priv->switch_mapping = false;
	priv->switch_ir_emu_mode = false;

	/* Set initial extension */
	fake_wiimote_set_extension(device->wiimote, input_mappings[priv->mapping].extension);

	ret = ds3_request_data(device);
	if (ret < 0)
		return ret;

	return 0;
}

int ds3_driver_ops_disconnect(usb_input_device_t *device)
{
	struct ds3_private_data_t *priv = (void *)device->private_data;

	priv->leds = 0;
	priv->rumble_on = false;

	return ds3_driver_update_leds_rumble(device);
}

int ds3_driver_ops_slot_changed(usb_input_device_t *device, u8 slot)
{
	struct ds3_private_data_t *priv = (void *)device->private_data;

	priv->leds = slot;

	return ds3_driver_update_leds_rumble(device);
}

int ds3_driver_ops_set_rumble(usb_input_device_t *device, bool rumble_on)
{
	struct ds3_private_data_t *priv = (void *)device->private_data;

	priv->rumble_on = rumble_on;

	return ds3_driver_update_leds_rumble(device);
}

bool guitar_report_input(usb_input_device_t *device) {
	struct ds3_private_data_t *priv = (void *)device->private_data;
	u16 wiimote_buttons = 0;
	u16 acc_x, acc_y, acc_z;
	union wiimote_extension_data_t extension_data;

	bm_map_wiimote(GUITAR_BUTTON__NUM, priv->input.buttons,
				   guitar_mapping.wiimote_button_map,
				   &wiimote_buttons);
	/* Normalize to accelerometer calibration configuration */
	acc_x = ACCEL_ZERO_G - ((s32)priv->input.acc_x * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;
	acc_y = ACCEL_ZERO_G + ((s32)priv->input.acc_y * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;
	acc_z = ACCEL_ZERO_G + ((s32)priv->input.acc_z * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;

	fake_wiimote_report_accelerometer(device->wiimote, acc_x, acc_y, acc_z);

	bm_map_guitar(GUITAR_BUTTON__NUM, priv->input.buttons,
				  GUITAR_ANALOG_AXIS__NUM, priv->input.analog_axis,
				  guitar_mapping.guitar_button_map,
				  guitar_mapping.guitar_analog_axis_map,
				  &extension_data.guitar);
	fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
								  &extension_data, sizeof(extension_data.guitar));

	return true;
}

bool drum_report_input(usb_input_device_t *device) {
	struct ds3_private_data_t *priv = (void *)device->private_data;
	u16 wiimote_buttons = 0;
	u16 acc_x, acc_y, acc_z;
	union wiimote_extension_data_t extension_data;

	bm_map_wiimote(DRUM_BUTTON__NUM, priv->input.buttons,
				   drum_mapping.wiimote_button_map,
				   &wiimote_buttons);

	/* Normalize to accelerometer calibration configuration */
	acc_x = ACCEL_ZERO_G;
	acc_y = ACCEL_ZERO_G;
	acc_z = ACCEL_ZERO_G;

	fake_wiimote_report_accelerometer(device->wiimote, acc_x, acc_y, acc_z);

	u8 drum_analog_axis[BM_DRUM_ANALOG_AXIS__NUM] = {0};
	drum_analog_axis[BM_DRUM_ANALOG_AXIS_STICK_X - 1] = 128;
	drum_analog_axis[BM_DRUM_ANALOG_AXIS_STICK_Y - 1] = 128;
	// Manually handle velocity here, as its pretty different between the wii and ps3 formats
	if (priv->input.analog_axis[DRUM_ANALOG_AXIS_GREEN]) {
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY_SELECTOR - 1] = 0b10010;
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY - 1] = priv->input.analog_axis[DRUM_ANALOG_AXIS_GREEN];
	} else if (priv->input.analog_axis[DRUM_ANALOG_AXIS_RED]) {
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY_SELECTOR - 1] = 0b11001;
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY - 1] = priv->input.analog_axis[DRUM_ANALOG_AXIS_RED];
	} else if (priv->input.analog_axis[DRUM_ANALOG_AXIS_YELLOW]) {
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY_SELECTOR - 1] = 0b10001;
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY - 1] = priv->input.analog_axis[DRUM_ANALOG_AXIS_YELLOW];
	} else if (priv->input.analog_axis[DRUM_ANALOG_AXIS_BLUE]) {
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY_SELECTOR - 1] = 0b01111;
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY - 1] = priv->input.analog_axis[DRUM_ANALOG_AXIS_BLUE];
	} else if (priv->input.analog_axis[DRUM_ANALOG_AXIS_ORANGE]) {
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY_SELECTOR - 1] = 0b01110;
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY - 1] = priv->input.analog_axis[DRUM_ANALOG_AXIS_ORANGE];
	} else if (priv->input.analog_axis[DRUM_ANALOG_AXIS_KICK]) {
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY_SELECTOR - 1] = 0b11011;
		drum_analog_axis[BM_DRUM_ANALOG_AXIS_VELOCITY - 1] = priv->input.analog_axis[DRUM_ANALOG_AXIS_KICK];
	}

	bm_map_drum(DRUM_BUTTON__NUM, priv->input.buttons,
				BM_GUITAR_ANALOG_AXIS__NUM, drum_analog_axis,
				drum_mapping.drum_button_map,
				&extension_data.drum);
	fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
								  &extension_data, sizeof(extension_data.drum));

	return true;
}

bool turntable_report_input(usb_input_device_t *device) {
	struct ds3_private_data_t *priv = (void *)device->private_data;
	u16 wiimote_buttons = 0;
	u16 acc_x, acc_y, acc_z;
	union wiimote_extension_data_t extension_data;

	bm_map_wiimote(TURNTABLE_BUTTON__NUM, priv->input.buttons,
				   guitar_mapping.wiimote_button_map,
				   &wiimote_buttons);
	acc_x = ACCEL_ZERO_G;
	acc_y = ACCEL_ZERO_G;
	acc_z = ACCEL_ZERO_G;

	fake_wiimote_report_accelerometer(device->wiimote, acc_x, acc_y, acc_z);

	bm_map_turntable(TURNTABLE_BUTTON__NUM, priv->input.buttons,
				  TURNTABLE_ANALOG_AXIS__NUM, priv->input.analog_axis,
				  turntable_mapping.turntable_button_map,
				  turntable_mapping.turntable_analog_axis_map,
				  &extension_data.turntable);
	fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
								  &extension_data, sizeof(extension_data.turntable));

	return true;
}

bool ds3_report_input(usb_input_device_t *device)
{
	if (device->vid == SONY_INST_VID && device->pid == GH_GUITAR_PID) {
		return guitar_report_input(device);
	}
	if (device->vid == SONY_INST_VID && device->pid == GH_DRUM_PID) {
		return drum_report_input(device);
	}
	if (device->vid == SONY_INST_VID && device->pid == DJ_TURNTABLE_PID) {
		return turntable_report_input(device);
	}
		struct ds3_private_data_t *priv = (void *)device->private_data;
	u16 wiimote_buttons = 0;
	u16 acc_x, acc_y, acc_z;
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

	bm_map_wiimote(DS3_BUTTON__NUM, priv->input.buttons,
		       input_mappings[priv->mapping].wiimote_button_map,
		       &wiimote_buttons);

	/* Normalize to accelerometer calibration configuration */
	acc_x = ACCEL_ZERO_G - ((s32)priv->input.acc_x * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;
	acc_y = ACCEL_ZERO_G + ((s32)priv->input.acc_y * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;
	acc_z = ACCEL_ZERO_G + ((s32)priv->input.acc_z * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;

	fake_wiimote_report_accelerometer(device->wiimote, acc_x, acc_y, acc_z);

	ir_emu_mode = ir_emu_modes[priv->ir_emu_mode_idx];
	if (ir_emu_mode == BM_IR_EMULATION_MODE_NONE) {
		bm_ir_dots_set_out_of_screen(ir_dots);
	} else {
		bm_map_ir_analog_axis(ir_emu_mode, &priv->ir_emu_state,
				      DS3_ANALOG_AXIS__NUM, priv->input.analog_axis,
				      ir_analog_axis_map, ir_dots);
	}

	fake_wiimote_report_ir_dots(device->wiimote, ir_dots);

	if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_NONE) {
		fake_wiimote_report_input(device->wiimote, wiimote_buttons);
	} else if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_NUNCHUK) {
		bm_map_nunchuk(DS3_BUTTON__NUM, priv->input.buttons,
			       DS3_ANALOG_AXIS__NUM, priv->input.analog_axis,
			       0, 0, 0,
			       input_mappings[priv->mapping].nunchuk_button_map,
			       input_mappings[priv->mapping].nunchuk_analog_axis_map,
			       &extension_data.nunchuk);
		fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
					      &extension_data, sizeof(extension_data.nunchuk));
	} else if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_CLASSIC) {
		bm_map_classic(DS3_BUTTON__NUM, priv->input.buttons,
			       DS3_ANALOG_AXIS__NUM, priv->input.analog_axis,
			       input_mappings[priv->mapping].classic_button_map,
			       input_mappings[priv->mapping].classic_analog_axis_map,
			       &extension_data.classic);
		fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
					      &extension_data, sizeof(extension_data.classic));
	}

	return true;
}

int ds3_driver_ops_usb_async_resp(usb_input_device_t *device)
{
	struct ds3_private_data_t *priv = (void *)device->private_data;
	if (device->vid == SONY_INST_VID && device->pid == GH_GUITAR_PID) {
		struct guitar_input_report *report = (void *)device->usb_async_resp;
		guitar_get_buttons(report, &priv->input.buttons);
		guitar_get_analog_axis(report, priv->input.analog_axis);

		priv->input.acc_x = (s16)report->acc_x - 511;
		priv->input.acc_y = 511 - (s16)report->acc_y;
		priv->input.acc_z = 511 - (s16)report->acc_z;
	} else if (device->vid == SONY_INST_VID && device->pid == GH_DRUM_PID) {
		struct drum_input_report *report = (void *)device->usb_async_resp;
		drum_get_buttons(report, &priv->input.buttons);
		drum_get_analog_axis(report, priv->input.analog_axis);
	} else if (device->vid == SONY_INST_VID && device->pid == DJ_TURNTABLE_PID) {
		struct turntable_input_report *report = (void *)device->usb_async_resp;
		turntable_get_buttons(report, &priv->input.buttons);
		turntable_get_analog_axis(report, priv->input.analog_axis);
	} else {
		struct ds3_input_report *report = (void *)device->usb_async_resp;
		ds3_get_buttons(report, &priv->input.buttons);
		ds3_get_analog_axis(report, priv->input.analog_axis);

		priv->input.acc_x = (s16)report->acc_x - 511;
		priv->input.acc_y = 511 - (s16)report->acc_y;
		priv->input.acc_z = 511 - (s16)report->acc_z;
	}

	return ds3_request_data(device);
}

const usb_device_driver_t ds3_usb_device_driver = {
	.probe		= ds3_driver_ops_probe,
	.init		= ds3_driver_ops_init,
	.disconnect	= ds3_driver_ops_disconnect,
	.slot_changed	= ds3_driver_ops_slot_changed,
	.set_rumble	= ds3_driver_ops_set_rumble,
	.report_input	= ds3_report_input,
	.usb_async_resp	= ds3_driver_ops_usb_async_resp,
};
