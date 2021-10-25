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

void bm_calculate_ir(
	/* Inputs */
	int num_coordinates, const u16 *x, const u16 *y,
	u16 max_x, u16 max_y,
	/* Outputs */
	struct ir_dot_t ir_dots[static IR_MAX_DOTS]);

#endif
