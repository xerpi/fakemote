#include "button_map.h"
#include "usb_device_drivers.h"
#include "usb.h"
#include "utils.h"
#include "wiimote.h"

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

enum drum_analog_axis_e {
	DRUM_ANALOG_AXIS_GREEN,
	DRUM_ANALOG_AXIS_RED,
	DRUM_ANALOG_AXIS_YELLOW,
	DRUM_ANALOG_AXIS_BLUE,
	DRUM_ANALOG_AXIS_ORANGE,
	DRUM_ANALOG_AXIS_KICK,
	DRUM_ANALOG_AXIS__NUM
};


#define MAX_ANALOG_AXIS DRUM_ANALOG_AXIS__NUM

struct gh_drum_private_data_t {
	struct {
		u32 buttons;
		u8 analog_axis[MAX_ANALOG_AXIS];
	} input;
	u8 leds;
};
static_assert(sizeof(struct gh_drum_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

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


static inline void drum_get_analog_axis(const struct drum_input_report *report,
										u8 analog_axis[static MAX_ANALOG_AXIS]) {
	analog_axis[DRUM_ANALOG_AXIS_GREEN] = report->pressure_green;
	analog_axis[DRUM_ANALOG_AXIS_RED] = report->pressure_red;
	analog_axis[DRUM_ANALOG_AXIS_YELLOW] = report->pressure_yellow;
	analog_axis[DRUM_ANALOG_AXIS_BLUE] = report->pressure_blue;
	analog_axis[DRUM_ANALOG_AXIS_ORANGE] = report->pressure_orange;
	analog_axis[DRUM_ANALOG_AXIS_KICK] = report->pressure_kick;
}


static inline int gh_drum_request_data(usb_input_device_t *device)
{
	return usb_device_driver_issue_intr_transfer_async(device, device->endpoint_address, device->usb_async_resp,
							   device->max_packet_len);
}

static int gh_drum_driver_update_leds(usb_input_device_t *device)
{
	// TODO: this
	return 0;
}

bool gh_drum_driver_ops_probe(u16 vid, u16 pid)
{
	static const struct device_id_t compatible[] = {
		{SONY_INST_VID, GH_DRUM_PID}
	};

	return usb_driver_is_comaptible(vid, pid, compatible, ARRAY_SIZE(compatible));
}

int gh_drum_driver_ops_init(usb_input_device_t *device, u16 vid, u16 pid)
{
	int ret;
	
	fake_wiimote_set_extension(device->wiimote, WIIMOTE_EXT_DRUM);

	ret = gh_drum_request_data(device);
	if (ret < 0)
		return ret;

	return 0;
}

int gh_drum_driver_ops_disconnect(usb_input_device_t *device)
{
	struct gh_drum_private_data_t *priv = (void *)device->private_data;

	priv->leds = 0;

	return gh_drum_driver_update_leds(device);
}

int gh_drum_driver_ops_slot_changed(usb_input_device_t *device, u8 slot)
{
	struct gh_drum_private_data_t *priv = (void *)device->private_data;

	priv->leds = slot;

	return gh_drum_driver_update_leds(device);
}

bool gh_drum_report_input(usb_input_device_t *device)
{
	struct gh_drum_private_data_t *priv = (void *)device->private_data;
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

int gh_drum_driver_ops_usb_async_resp(usb_input_device_t *device)
{
	struct gh_drum_private_data_t *priv = (void *)device->private_data;
	struct drum_input_report *report = (void *)device->usb_async_resp;
	drum_get_buttons(report, &priv->input.buttons);
	drum_get_analog_axis(report, priv->input.analog_axis);

	return gh_drum_request_data(device);
}

const usb_device_driver_t gh_drum_usb_device_driver = {
	.probe		= gh_drum_driver_ops_probe,
	.init		= gh_drum_driver_ops_init,
	.disconnect	= gh_drum_driver_ops_disconnect,
	.slot_changed	= gh_drum_driver_ops_slot_changed,
	.report_input	= gh_drum_report_input,
	.usb_async_resp	= gh_drum_driver_ops_usb_async_resp,
};
