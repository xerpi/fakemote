#include "button_map.h"
#include "usb_device_drivers.h"
#include "usb.h"
#include "utils.h"
#include "wiimote.h"

#define GUITAR_ACC_RES_PER_G	113

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

enum guitar_analog_axis_e {
	GUITAR_ANALOG_AXIS_TAP_BAR,
	GUITAR_ANALOG_AXIS_WHAMMY_BAR,
	GUITAR_ANALOG_AXIS__NUM
};

#define MAX_ANALOG_AXIS GUITAR_ANALOG_AXIS__NUM

struct gh_guitar_private_data_t {
	struct {
		u32 buttons;
		u8 analog_axis[MAX_ANALOG_AXIS];
		s16 acc_x, acc_y, acc_z;
	} input;
	u8 leds;
};
static_assert(sizeof(struct gh_guitar_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

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

static inline void gh_guitar_get_buttons(const struct guitar_input_report *report, u32 *buttons) {
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

static inline void gh_guitar_get_analog_axis(const struct guitar_input_report *report,
										  u8 analog_axis[static MAX_ANALOG_AXIS]) {
	analog_axis[GUITAR_ANALOG_AXIS_TAP_BAR] = report->tap_bar;
	analog_axis[GUITAR_ANALOG_AXIS_WHAMMY_BAR] = report->whammy_bar;
}


static inline int gh_guitar_request_data(usb_input_device_t *device)
{
	return usb_device_driver_issue_intr_transfer_async(device, device->endpoint_address, device->usb_async_resp,
							   device->max_packet_len);
}


bool gh_guitar_driver_ops_probe(u16 vid, u16 pid)
{
	static const struct device_id_t compatible[] = {
		{SONY_INST_VID, GH_GUITAR_PID},
	};

	return usb_driver_is_comaptible(vid, pid, compatible, ARRAY_SIZE(compatible));
}

int gh_guitar_driver_ops_init(usb_input_device_t *device, u16 vid, u16 pid)
{
	int ret;

	fake_wiimote_set_extension(device->wiimote, WIIMOTE_EXT_GUITAR);

	ret = gh_guitar_request_data(device);
	if (ret < 0)
		return ret;

	return 0;
}

static int gh_guitar_driver_update_leds(usb_input_device_t *device)
{
	// TODO: this
	return 0;
}

int gh_guitar_driver_ops_disconnect(usb_input_device_t *device)
{
	struct gh_guitar_private_data_t *priv = (void *)device->private_data;

	priv->leds = 0;

	return gh_guitar_driver_update_leds(device);
}

int gh_guitar_driver_ops_slot_changed(usb_input_device_t *device, u8 slot)
{
	struct gh_guitar_private_data_t *priv = (void *)device->private_data;

	priv->leds = slot;

	return gh_guitar_driver_update_leds(device);
}

bool gh_guitar_report_input(usb_input_device_t *device)
{
	struct gh_guitar_private_data_t *priv = (void *)device->private_data;
	u16 wiimote_buttons = 0;
	u16 acc_x, acc_y, acc_z;
	union wiimote_extension_data_t extension_data;

	bm_map_wiimote(GUITAR_BUTTON__NUM, priv->input.buttons,
				   guitar_mapping.wiimote_button_map,
				   &wiimote_buttons);
	/* Normalize to accelerometer calibration configuration */
	acc_x = ACCEL_ZERO_G - ((s32)priv->input.acc_x * (ACCEL_ONE_G - ACCEL_ZERO_G)) / GUITAR_ACC_RES_PER_G;
	acc_y = ACCEL_ZERO_G + ((s32)priv->input.acc_y * (ACCEL_ONE_G - ACCEL_ZERO_G)) / GUITAR_ACC_RES_PER_G;
	acc_z = ACCEL_ZERO_G + ((s32)priv->input.acc_z * (ACCEL_ONE_G - ACCEL_ZERO_G)) / GUITAR_ACC_RES_PER_G;

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

int gh_guitar_driver_ops_usb_async_resp(usb_input_device_t *device)
{
	struct gh_guitar_private_data_t *priv = (void *)device->private_data;
	struct guitar_input_report *report = (void *)device->usb_async_resp;
	gh_guitar_get_buttons(report, &priv->input.buttons);
	gh_guitar_get_analog_axis(report, priv->input.analog_axis);

	priv->input.acc_x = (s16)report->acc_x - 511;
	priv->input.acc_y = 511 - (s16)report->acc_y;
	priv->input.acc_z = 511 - (s16)report->acc_z;

	return gh_guitar_request_data(device);
}

const usb_device_driver_t gh_guitar_usb_device_driver = {
	.probe		= gh_guitar_driver_ops_probe,
	.init		= gh_guitar_driver_ops_init,
	.disconnect	= gh_guitar_driver_ops_disconnect,
	.slot_changed	= gh_guitar_driver_ops_slot_changed,
	.report_input	= gh_guitar_report_input,
	.usb_async_resp	= gh_guitar_driver_ops_usb_async_resp,
};
