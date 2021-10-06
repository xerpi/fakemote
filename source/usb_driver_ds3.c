#include "usb_device_drivers.h"
#include "usb.h"
#include "utils.h"
#include "wiimote.h"

#define DS3_ACC_RES_PER_G	113

struct ds3_private_data_t {
	enum wiimote_mgr_ext_u extension;
	u8 leds;
	bool rumble_on;
};
static_assert(sizeof(struct ds3_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

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

static inline void ds3_map_buttons(const struct ds3_input_report *input, u16 *buttons)
{
	if (input->left)
		*buttons |= WPAD_BUTTON_LEFT;
	if (input->down)
		*buttons |= WPAD_BUTTON_DOWN;
	if (input->right)
		*buttons |= WPAD_BUTTON_RIGHT;
	if (input->up)
		*buttons |= WPAD_BUTTON_UP;
	if (input->cross)
		*buttons |= WPAD_BUTTON_A;
	if (input->circle)
		*buttons |= WPAD_BUTTON_B;
	if (input->triangle)
		*buttons |= WPAD_BUTTON_1;
	if (input->square)
		*buttons |= WPAD_BUTTON_2;
	if (input->ps)
		*buttons |= WPAD_BUTTON_HOME;
	if (input->select)
		*buttons |= WPAD_BUTTON_MINUS;
	if (input->start)
		*buttons |= WPAD_BUTTON_PLUS;
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
	priv->extension = WIIMOTE_MGR_EXT_NUNCHUK;

	/* Set initial extension */
	fake_wiimote_mgr_set_extension(device->wiimote, priv->extension);

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
	s32 ds3_acc_x, ds3_acc_y, ds3_acc_z;
	u16 acc_x, acc_y, acc_z;
	struct wiimote_extension_data_format_nunchuk_t nunchuk;

	ds3_map_buttons(report, &buttons);

	ds3_acc_x = (s32)report->acc_x - 511;
	ds3_acc_y = 511 - (s32)report->acc_y;
	ds3_acc_z = 511 - (s32)report->acc_z;

	/* Normalize to accelerometer calibration configuration */
	acc_x = ACCEL_ZERO_G - (ds3_acc_x * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;
	acc_y = ACCEL_ZERO_G + (ds3_acc_y * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;
	acc_z = ACCEL_ZERO_G + (ds3_acc_z * (ACCEL_ONE_G - ACCEL_ZERO_G)) / DS3_ACC_RES_PER_G;

	fake_wiimote_mgr_report_accelerometer(device->wiimote, acc_x, acc_y, acc_z);

	if (priv->extension == WIIMOTE_MGR_EXT_NUNCHUK) {
		memset(&nunchuk, 0, sizeof(nunchuk));
		nunchuk.jx = report->left_x;
		nunchuk.jy = 255 - report->left_y;
		nunchuk.bt.c = !report->l1;
		nunchuk.bt.z = !report->l2;
		fake_wiimote_mgr_report_input_ext(device->wiimote, buttons,
						  &nunchuk, sizeof(nunchuk));
	} else {
		fake_wiimote_mgr_report_input(device->wiimote, buttons);
	}

	return ds3_request_data(device);
}
