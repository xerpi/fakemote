#ifndef BUTTON_MAPPING_H
#define BUTTON_MAPPING_H

#include "utils.h"
#include "wiimote.h"

#define BM_ANALOG_AXIS_INVALID	0

/* Nunchuk */
enum bm_nunchuk_analog_axis_e {
	BM_NUNCHUK_ANALOG_AXIS_X = 1,
	BM_NUNCHUK_ANALOG_AXIS_Y,
	BM_NUNCHUK_ANALOG_AXIS__NUM = BM_NUNCHUK_ANALOG_AXIS_Y
};

/* Classic controller */
enum bm_classic_analog_axis_e {
	BM_CLASSIC_ANALOG_AXIS_LEFT_X = 1,
	BM_CLASSIC_ANALOG_AXIS_LEFT_Y,
	BM_CLASSIC_ANALOG_AXIS_RIGHT_X,
	BM_CLASSIC_ANALOG_AXIS_RIGHT_Y,
	BM_CLASSIC_ANALOG_AXIS__NUM = BM_CLASSIC_ANALOG_AXIS_RIGHT_Y
};

/* IR pointer emulation */
enum bm_ir_emulation_mode_e {
	BM_IR_EMULATION_MODE_NONE,
	BM_IR_EMULATION_MODE_DIRECT,
	BM_IR_EMULATION_MODE_RELATIVE_ANALOG_AXIS,
	BM_IR_EMULATION_MODE_ABSOLUTE_ANALOG_AXIS,
};

enum bm_ir_axis_e {
	BM_IR_AXIS_NONE,
	BM_IR_AXIS_X,
	BM_IR_AXIS_Y,
	BM_IR_AXIS__NUM = BM_IR_AXIS_Y,
};

struct bm_ir_emulation_state_t {
	u16 position[BM_IR_AXIS__NUM];
};

void bm_map_wiimote(
	/* Inputs */
	int num_buttons, u32 buttons,
	/* Mapping tables */
	const u16 *wiimote_button_map,
	/* Outputs */
	u16 *wiimote_buttons);

void bm_map_nunchuk(
	/* Inputs */
	int num_buttons, u32 buttons,
	int num_analog_axis, const u8 *analog_axis,
	u16 ax, u16 ay, u16 az,
	/* Mapping tables */
	const u8 *nunchuk_button_map,
	const u8 *nunchuk_analog_axis_map,
	/* Outputs */
	struct wiimote_extension_data_format_nunchuk_t *nunchuk);

void bm_map_classic(
	/* Inputs */
	int num_buttons, u32 buttons,
	int num_analog_axis, const u8 *analog_axis,
	/* Mapping tables */
	const u16 *classic_button_map,
	const u8 *classic_analog_axis_map,
	/* Outputs */
	struct wiimote_extension_data_format_classic_t *classic);

void bm_map_ir_direct(
	/* Inputs */
	int num_coordinates, const u16 *x, const u16 *y,
	u16 max_x, u16 max_y,
	/* Outputs */
	struct ir_dot_t ir_dots[static IR_MAX_DOTS]);

void bm_map_ir_analog_axis(
	/* Inputs */
	enum bm_ir_emulation_mode_e mode,
	struct bm_ir_emulation_state_t *state,
	int num_analog_axis, const u8 *analog_axis,
	const u8 *ir_analog_axis_map,
	/* Outputs */
	struct ir_dot_t ir_dots[static IR_MAX_DOTS]);

static inline bool bm_check_switch_mapping(u32 buttons, bool *switch_mapping, u32 switch_mapping_combo)
{
	bool switch_pressed = (buttons & switch_mapping_combo) == switch_mapping_combo;
	bool ret = false;

	if (switch_pressed && !*switch_mapping)
		ret = true;

	*switch_mapping = switch_pressed;
	return ret;
}

static inline void bm_nunchuk_format(struct wiimote_extension_data_format_nunchuk_t *out,
				     u8 buttons, u8 analog_axis[static BM_NUNCHUK_ANALOG_AXIS__NUM],
				     u16 ax, u16 ay, u16 az)
{
	out->jx = analog_axis[BM_NUNCHUK_ANALOG_AXIS_X - 1];
	out->jy = analog_axis[BM_NUNCHUK_ANALOG_AXIS_Y - 1];
	out->ax = (ax >> 2) & 0xFF;
	out->ay = (ay >> 2) & 0xFF;
	out->az = (az >> 2) & 0xFF;
	out->bt.acc_z_lsb = az & 3;
	out->bt.acc_y_lsb = ay & 3;
	out->bt.acc_x_lsb = ax & 3;
	out->bt.c = !(buttons & NUNCHUK_BUTTON_C);
	out->bt.z = !(buttons & NUNCHUK_BUTTON_Z);
}

static inline void bm_classic_format(struct wiimote_extension_data_format_classic_t *out,
				     u16 buttons, u8 analog_axis[static BM_CLASSIC_ANALOG_AXIS__NUM])
{
	u8 lx = analog_axis[BM_CLASSIC_ANALOG_AXIS_LEFT_X - 1] >> 2;
	u8 ly = analog_axis[BM_CLASSIC_ANALOG_AXIS_LEFT_Y - 1] >> 2;
	u8 rx = analog_axis[BM_CLASSIC_ANALOG_AXIS_RIGHT_X - 1] >> 3;
	u8 ry = analog_axis[BM_CLASSIC_ANALOG_AXIS_RIGHT_Y - 1] >> 3;
	u8 lt = (buttons & CLASSIC_CTRL_BUTTON_FULL_L) ? 31 : 0;
	u8 rt = (buttons & CLASSIC_CTRL_BUTTON_FULL_R) ? 31 : 0;

	out->rx3 = (rx >> 3) & 3;
	out->lx = lx & 0x3F;
	out->rx2 = (rx >> 1) & 3;
	out->ly = ly & 0x3F;
	out->rx1 = rx & 1;
	out->lt2 = (lt >> 3) & 3;
	out->ry = ry & 0x1F;
	out->lt1 = lt & 0x7;
	out->rt = rt & 0x1F;
	out->bt.hex = (~buttons) & CLASSIC_CTRL_BUTTON_ALL;
}

static inline void bm_ir_emulation_state_reset(struct bm_ir_emulation_state_t *state)
{
	state->position[BM_IR_AXIS_X - 1] = IR_CENTER_X;
	state->position[BM_IR_AXIS_Y - 1] = IR_CENTER_Y;
}

static inline void bm_ir_dots_set_out_of_screen(struct ir_dot_t ir_dots[static IR_MAX_DOTS])
{
	for (int i = 0; i < IR_MAX_DOTS; i++)
		ir_dots[i].y = 1023;
}

#endif
