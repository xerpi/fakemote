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

enum ds3_analog_axis_e {
	DS3_ANALOG_AXIS_LEFT_X,
	DS3_ANALOG_AXIS_LEFT_Y,
	DS3_ANALOG_AXIS_RIGHT_X,
	DS3_ANALOG_AXIS_RIGHT_Y,
	DS3_ANALOG_AXIS__NUM
};

struct ds3_private_data_t {
	struct {
		u32 buttons;
		u8 analog_axis[DS3_ANALOG_AXIS__NUM];
		s16 acc_x, acc_y, acc_z;
	} input;
	u8 mapping;
	u8 leds;
	bool rumble_on;
	bool switch_mapping;
};
static_assert(sizeof(struct ds3_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

#define SWITCH_MAPPING_COMBO	(BIT(DS3_BUTTON_R3))

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

static inline void ds3_get_analog_axis(const struct ds3_input_report *report,
				       u8 analog_axis[static DS3_ANALOG_AXIS__NUM])
{
	analog_axis[DS3_ANALOG_AXIS_LEFT_X] = report->left_x;
	analog_axis[DS3_ANALOG_AXIS_LEFT_Y] = 255 - report->left_y;
	analog_axis[DS3_ANALOG_AXIS_RIGHT_X] = report->right_x;
	analog_axis[DS3_ANALOG_AXIS_RIGHT_Y] = 255 - report->right_y;
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
	priv->leds = 0;
	priv->rumble_on = false;
	priv->mapping = 0;
	priv->switch_mapping = false;

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

bool ds3_report_input(usb_input_device_t *device)
{
	struct ds3_private_data_t *priv = (void *)device->private_data;
	u16 wiimote_buttons = 0;
	u16 acc_x, acc_y, acc_z;
	union wiimote_extension_data_t extension_data;

	if (bm_check_switch_mapping(priv->input.buttons, &priv->switch_mapping, SWITCH_MAPPING_COMBO)) {
		priv->mapping = (priv->mapping + 1) % ARRAY_SIZE(input_mappings);
		fake_wiimote_set_extension(device->wiimote, input_mappings[priv->mapping].extension);
		return false;
	}

	bm_map_wiimote(DS3_BUTTON__NUM, priv->input.buttons,
		       input_mappings[priv->mapping].wiimote_button_map,
		       &wiimote_buttons);

	/* Normalize to accelerometer calibration configuration */
	acc_x = ACCEL_ZERO_G - ((s32)priv->input.acc_x * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;
	acc_y = ACCEL_ZERO_G + ((s32)priv->input.acc_y * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;
	acc_z = ACCEL_ZERO_G + ((s32)priv->input.acc_z * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;

	fake_wiimote_report_accelerometer(device->wiimote, acc_x, acc_y, acc_z);

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
	struct ds3_input_report *report = (void *)device->usb_async_resp;

	ds3_get_buttons(report, &priv->input.buttons);
	ds3_get_analog_axis(report, priv->input.analog_axis);

	priv->input.acc_x = (s16)report->acc_x - 511;
	priv->input.acc_y = 511 - (s16)report->acc_y;
	priv->input.acc_z = 511 - (s16)report->acc_z;

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
