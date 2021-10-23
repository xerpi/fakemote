#include "button_mapping.h"

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
	union bm_extension_t *ext_data)
{
	for (int i = 0; i < num_buttons; i++) {
		if (buttons & 1) {
			*wiimote_buttons |= wiimote_button_mapping[i];

			if (ext == WIIMOTE_EXT_NUNCHUK)
				ext_data->nunchuk.buttons |= nunchuk_button_mapping[i];
		}

		buttons >>= 1;
	}

	if (ext == WIIMOTE_EXT_NUNCHUK) {
		for (int i = 0; i < num_analog_axis; i++) {
			if (nunchuk_analog_axis_mapping[i]) {
				ext_data->nunchuk.analog_axis[nunchuk_analog_axis_mapping[i] - 1] =
					analog_axis[i];
			}
		}
	}
}
