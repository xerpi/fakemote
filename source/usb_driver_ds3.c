#include "button_mapping.h"
#include "usb_device_drivers.h"
#include "usb.h"
#include "utils.h"
#include "wiimote.h"

#define DS3_ACC_RES_PER_G	113

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
	enum wiimote_ext_e extension;
	u8 leds;
	bool rumble_on;
};
static_assert(sizeof(struct ds3_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

static const u16 wiimote_button_mapping[DS3_BUTTON__NUM] = {
	[DS3_BUTTON_TRIANGLE] = WPAD_BUTTON_1,
	[DS3_BUTTON_CIRCLE]   = WPAD_BUTTON_B,
	[DS3_BUTTON_CROSS]    = WPAD_BUTTON_A,
	[DS3_BUTTON_SQUARE]   = WPAD_BUTTON_2,
	[DS3_BUTTON_UP]       = WPAD_BUTTON_UP,
	[DS3_BUTTON_DOWN]     = WPAD_BUTTON_DOWN,
	[DS3_BUTTON_LEFT]     = WPAD_BUTTON_LEFT,
	[DS3_BUTTON_RIGHT]    = WPAD_BUTTON_RIGHT,
	[DS3_BUTTON_START]    = WPAD_BUTTON_PLUS,
	[DS3_BUTTON_SELECT]   = WPAD_BUTTON_MINUS,
	[DS3_BUTTON_PS]       = WPAD_BUTTON_HOME,
};

static const u8 nunchuk_button_mapping[DS3_BUTTON__NUM] = {
	[DS3_BUTTON_L1] = BM_NUNCHUK_BUTTON_C,
	[DS3_BUTTON_L2] = BM_NUNCHUK_BUTTON_Z,
};

static const u8 nunchuk_analog_axis_mapping[DS3_ANALOG_AXIS__NUM] = {
	[DS3_ANALOG_AXIS_LEFT_X] = BM_NUNCHUK_ANALOG_AXIS_X,
	[DS3_ANALOG_AXIS_LEFT_Y] = BM_NUNCHUK_ANALOG_AXIS_Y,
};

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

	u16  unk3;
	u8   unk4;

	u8   status;
	u8   power_rating;
	u8   comm_status;

	u32  unk5;
	u32  unk6;
	u8   unk7;

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

static inline void ds3_get_buttons(const struct ds3_input_report *input, u32 *buttons)
{
#define MAP(field, button) \
	if (input->field) \
		*buttons |= BIT(button);

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
}

static inline void ds3_get_analog_axis(const struct ds3_input_report *input,
				       u8 analog_axis[static DS3_ANALOG_AXIS__NUM])
{
	analog_axis[DS3_ANALOG_AXIS_LEFT_X] = input->left_x;
	analog_axis[DS3_ANALOG_AXIS_LEFT_Y] = 255 - input->left_y;
	analog_axis[DS3_ANALOG_AXIS_RIGHT_X] = input->right_x;
	analog_axis[DS3_ANALOG_AXIS_RIGHT_Y] = input->right_y;
}

static inline void ds3_map(struct ds3_private_data_t *priv,
			   const struct ds3_input_report *input,
			   u16 *wiimote_buttons,
			   union bm_extension_t *ext_data)
{
	u32 ds3_buttons = 0;
	u8 ds3_analog_axis[DS3_ANALOG_AXIS__NUM];

	ds3_get_buttons(input, &ds3_buttons);
	ds3_get_analog_axis(input, ds3_analog_axis);

	bm_map(priv->extension,
	       DS3_BUTTON__NUM, ds3_buttons,
	       DS3_ANALOG_AXIS__NUM, ds3_analog_axis,
	       wiimote_button_mapping,
	       nunchuk_button_mapping,
	       nunchuk_analog_axis_mapping,
	       wiimote_buttons,
	       ext_data);
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

int ds3_driver_ops_init(usb_input_device_t *device)
{
	int ret;
	struct ds3_private_data_t *priv = (void *)device->private_data;

	ret = ds3_set_operational(device);
	if (ret < 0)
		return ret;

	/* Init private state */
	priv->leds = 0;
	priv->rumble_on = false;
	priv->extension = WIIMOTE_EXT_NUNCHUK;

	/* Set initial extension */
	fake_wiimote_set_extension(device->wiimote, priv->extension);

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

int ds3_driver_ops_usb_async_resp(usb_input_device_t *device)
{
	struct ds3_private_data_t *priv = (void *)device->private_data;
	struct ds3_input_report *report = (void *)device->usb_async_resp;
	u16 buttons = 0;
	union bm_extension_t bm_ext = {0};
	s32 ds3_acc_x, ds3_acc_y, ds3_acc_z;
	u16 acc_x, acc_y, acc_z;
	struct wiimote_extension_data_format_nunchuk_t nunchuk;

	ds3_map(priv, report, &buttons, &bm_ext);

	ds3_acc_x = (s32)report->acc_x - 511;
	ds3_acc_y = 511 - (s32)report->acc_y;
	ds3_acc_z = 511 - (s32)report->acc_z;

	/* Normalize to accelerometer calibration configuration */
	acc_x = ACCEL_ZERO_G - (ds3_acc_x * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;
	acc_y = ACCEL_ZERO_G + (ds3_acc_y * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;
	acc_z = ACCEL_ZERO_G + (ds3_acc_z * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;

	fake_wiimote_report_accelerometer(device->wiimote, acc_x, acc_y, acc_z);

	if (priv->extension == WIIMOTE_EXT_NUNCHUK) {
		bm_nunchuk_format(&nunchuk, &bm_ext.nunchuk);
		fake_wiimote_report_input_ext(device->wiimote, buttons,
						  &nunchuk, sizeof(nunchuk));
	} else {
		fake_wiimote_report_input(device->wiimote, buttons);
	}

	return ds3_request_data(device);
}
