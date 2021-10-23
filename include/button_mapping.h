#ifndef BUTTON_MAPPING_H
#define BUTTON_MAPPING_H

#include "utils.h"
#include "wiimote.h"

#define BM_ANALOG_AXIS_INVALID	0

/* Nunchuk */
#define BM_NUNCHUK_BUTTON_C	BIT(0)
#define BM_NUNCHUK_BUTTON_Z	BIT(1)

enum bm_nunchuk_analog_axis_e {
	BM_NUNCHUK_ANALOG_AXIS_X = 1,
	BM_NUNCHUK_ANALOG_AXIS_Y,
	BM_NUNCHUK_ANALOG_AXIS__NUM = BM_NUNCHUK_ANALOG_AXIS_Y
};

struct bm_nunchuk_t {
	u8 analog_axis[BM_NUNCHUK_ANALOG_AXIS__NUM];
	u16 ax;
	u16 ay;
	u16 az;
	u8 buttons;
};

/* Classic controller */
/* TODO */

union bm_extension_t {
	struct bm_nunchuk_t nunchuk;
};

void bm_map(
	/* Inputs */
	enum wiimote_ext_e ext,
	int num_buttons, u32 buttons,
	int num_analog_axis, const u8 *analog_axis,
	/* Mapping tables */
	const u16 *wiimote_button_mapping,
	const u8 *nunchuk_button_mapping,
	const u8 *nunchuk_analog_axis_mapping,
	/* Outputs */
	u16 *wiimote_buttons,
	union bm_extension_t *ext_data);

static inline void bm_nunchuk_format(struct wiimote_extension_data_format_nunchuk_t *out,
				     const struct bm_nunchuk_t *in)
{
	out->jx = in->analog_axis[BM_NUNCHUK_ANALOG_AXIS_X - 1];
	out->jy = in->analog_axis[BM_NUNCHUK_ANALOG_AXIS_Y - 1];
	out->ax = (in->ax >> 2) & 0xFF;
	out->ay = (in->ay >> 2) & 0xFF;
	out->az = (in->az >> 2) & 0xFF;
	out->bt.acc_z_lsb = in->az & 3;
	out->bt.acc_y_lsb = in->ay & 3;
	out->bt.acc_x_lsb = in->ax & 3;
	out->bt.c = !(in->buttons & BM_NUNCHUK_BUTTON_C);
	out->bt.z = !(in->buttons & BM_NUNCHUK_BUTTON_Z);
}

#endif
