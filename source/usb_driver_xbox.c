#include "button_map.h"
#include "usb_device_drivers.h"
#include "usb.h"
#include "utils.h"
#include "wiimote.h"

struct xbox360_rumble_data {
	u8 command;
	u8 size;
	u8 dummy1;
	u8 big;
	u8 little;
	u8 dummy2[3];
};
static_assert(sizeof(struct xbox360_rumble_data) == 8, "struct xbox360_rumble_data wrong size");

struct xboxone_rumble_data {
	u8 command;
	u8 dummy1;
	u8 counter;
	u8 size;
	u8 mode;
	u8 rumble_mask;
	u8 trigger_left;
	u8 trigger_right;
	u8 strong_magnitude;
	u8 weak_magnitude;
	u8 duration;
	u8 period;
	u8 extra;
};
static_assert(sizeof(struct xboxone_rumble_data) == 13, "struct xboxone_rumble_data wrong size");

enum xbox_control_message {
	CONTROL_MESSAGE_SET_RUMBLE = 0,
	CONTROL_MESSAGE_SET_LED = 1,
};

enum xbox_led_pattern {
	LED_OFF = 0,
	// 2 quick flashes, then a series of slow flashes (about 1 per second).
	LED_FLASH = 1,
	// Flash three times then hold the LED on. This is the standard way to tell
	// the player which player number they are.
	LED_FLASH_TOP_LEFT = 2,
	LED_FLASH_TOP_RIGHT = 3,
	LED_FLASH_BOTTOM_LEFT = 4,
	LED_FLASH_BOTTOM_RIGHT = 5,
	// Simply turn on the specified LED and turn all other LEDs off.
	LED_HOLD_TOP_LEFT = 6,
	LED_HOLD_TOP_RIGHT = 7,
	LED_HOLD_BOTTOM_LEFT = 8,
	LED_HOLD_BOTTOM_RIGHT = 9,
	LED_ROTATE = 10,
	LED_FLASH_FAST = 11,
	LED_FLASH_SLOW = 12,  // Flash about once per 3 seconds
	// Flash alternating LEDs for a few seconds, then flash all LEDs about once
	// per second
	LED_ALTERNATE_PATTERN = 13,
	// 14 is just another boring flashing speed.
	// Flash all LEDs once then go black.
	LED_FLASH_ONCE = 15,
	LED_NUM_PATTERNS
};

struct xbox_input_report {
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

enum xbox_buttons_e {
	XBOX_BUTTON_TRIANGLE,
	XBOX_BUTTON_CIRCLE,
	XBOX_BUTTON_CROSS,
	XBOX_BUTTON_SQUARE,
	XBOX_BUTTON_UP,
	XBOX_BUTTON_DOWN,
	XBOX_BUTTON_LEFT,
	XBOX_BUTTON_RIGHT,
	XBOX_BUTTON_R3,
	XBOX_BUTTON_L3,
	XBOX_BUTTON_START,
	XBOX_BUTTON_SELECT,
	XBOX_BUTTON_R2,
	XBOX_BUTTON_L2,
	XBOX_BUTTON_R1,
	XBOX_BUTTON_L1,
	XBOX_BUTTON_PS,
	XBOX_BUTTON__NUM
};

enum xbox_analog_axis_e {
	XBOX_ANALOG_AXIS_LEFT_X,
	XBOX_ANALOG_AXIS_LEFT_Y,
	XBOX_ANALOG_AXIS_RIGHT_X,
	XBOX_ANALOG_AXIS_RIGHT_Y,
	XBOX_ANALOG_AXIS__NUM
};

struct xbox_private_data_t {
	bool is_xboxone;
	u8 command_cnt;
	u8 mapping;
	bool rumble_on;
	bool switch_input_combo_pressed;
};
static_assert(sizeof(struct xbox_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

static const struct device_id_t compatible_xbox360[] = {
	{0x1949, 0x041a}, /* Amazon Luna Controller */
	{0x045e, 0x028e}, /* Xbox 360 */
};

static const struct device_id_t compatible_xboxone[] = {
	{0x045e, 0x02d1}, /* Xbox One */
	{0x045e, 0x02dd}, /* Xbox One, 2015 firmware */
	{0x045e, 0x02e3}, /* Xbox Elite */
	{0x045e, 0x02ea}, /* Xbox One S */
	{0x045e, 0x0b00}, /* Xbox Elite 2 */
	{0x045e, 0x0b0a}, /* Xbox Adaptive */
	{0x045e, 0x0b12}, /* Xbox Series X */
};

#define SWITCH_INPUT_MAPPING_COMBO	(BIT(XBOX_BUTTON_R3))

static const struct {
	enum wiimote_ext_e extension;
	u16 wiimote_button_map[XBOX_BUTTON__NUM];
	u8 nunchuk_button_map[XBOX_BUTTON__NUM];
	u8 nunchuk_analog_axis_map[XBOX_ANALOG_AXIS__NUM];
	u16 classic_button_map[XBOX_BUTTON__NUM];
	u8 classic_analog_axis_map[XBOX_ANALOG_AXIS__NUM];
} input_mappings[] = {
	{
		.extension = WIIMOTE_EXT_NUNCHUK,
		.wiimote_button_map = {
			[XBOX_BUTTON_TRIANGLE] = WIIMOTE_BUTTON_ONE,
			[XBOX_BUTTON_CIRCLE]   = WIIMOTE_BUTTON_B,
			[XBOX_BUTTON_CROSS]    = WIIMOTE_BUTTON_A,
			[XBOX_BUTTON_SQUARE]   = WIIMOTE_BUTTON_TWO,
			[XBOX_BUTTON_UP]       = WIIMOTE_BUTTON_UP,
			[XBOX_BUTTON_DOWN]     = WIIMOTE_BUTTON_DOWN,
			[XBOX_BUTTON_LEFT]     = WIIMOTE_BUTTON_LEFT,
			[XBOX_BUTTON_RIGHT]    = WIIMOTE_BUTTON_RIGHT,
			[XBOX_BUTTON_START]    = WIIMOTE_BUTTON_PLUS,
			[XBOX_BUTTON_SELECT]   = WIIMOTE_BUTTON_MINUS,
			[XBOX_BUTTON_PS]       = WIIMOTE_BUTTON_HOME,
		},
		.nunchuk_button_map = {
			[XBOX_BUTTON_L1] = NUNCHUK_BUTTON_C,
			[XBOX_BUTTON_L2] = NUNCHUK_BUTTON_Z,
		},
		.nunchuk_analog_axis_map = {
			[XBOX_ANALOG_AXIS_LEFT_X] = BM_NUNCHUK_ANALOG_AXIS_X,
			[XBOX_ANALOG_AXIS_LEFT_Y] = BM_NUNCHUK_ANALOG_AXIS_Y,
		},
	},
	{
		.extension = WIIMOTE_EXT_CLASSIC,
		.classic_button_map = {
			[XBOX_BUTTON_TRIANGLE] = CLASSIC_CTRL_BUTTON_X,
			[XBOX_BUTTON_CIRCLE]   = CLASSIC_CTRL_BUTTON_A,
			[XBOX_BUTTON_CROSS]    = CLASSIC_CTRL_BUTTON_B,
			[XBOX_BUTTON_SQUARE]   = CLASSIC_CTRL_BUTTON_Y,
			[XBOX_BUTTON_UP]       = CLASSIC_CTRL_BUTTON_UP,
			[XBOX_BUTTON_DOWN]     = CLASSIC_CTRL_BUTTON_DOWN,
			[XBOX_BUTTON_LEFT]     = CLASSIC_CTRL_BUTTON_LEFT,
			[XBOX_BUTTON_RIGHT]    = CLASSIC_CTRL_BUTTON_RIGHT,
			[XBOX_BUTTON_START]    = CLASSIC_CTRL_BUTTON_PLUS,
			[XBOX_BUTTON_SELECT]   = CLASSIC_CTRL_BUTTON_MINUS,
			[XBOX_BUTTON_R2]       = CLASSIC_CTRL_BUTTON_ZR,
			[XBOX_BUTTON_L2]       = CLASSIC_CTRL_BUTTON_ZL,
			[XBOX_BUTTON_R1]       = CLASSIC_CTRL_BUTTON_FULL_R,
			[XBOX_BUTTON_L1]       = CLASSIC_CTRL_BUTTON_FULL_L,
			[XBOX_BUTTON_PS]       = CLASSIC_CTRL_BUTTON_HOME,
		},
		.classic_analog_axis_map = {
			[XBOX_ANALOG_AXIS_LEFT_X]  = BM_CLASSIC_ANALOG_AXIS_LEFT_X,
			[XBOX_ANALOG_AXIS_LEFT_Y]  = BM_CLASSIC_ANALOG_AXIS_LEFT_Y,
			[XBOX_ANALOG_AXIS_RIGHT_X] = BM_CLASSIC_ANALOG_AXIS_RIGHT_X,
			[XBOX_ANALOG_AXIS_RIGHT_Y] = BM_CLASSIC_ANALOG_AXIS_RIGHT_Y,
		},
	},
};

static inline void xbox_get_buttons(const struct xbox_input_report *report, u32 *buttons)
{
#define MAP(field, button) \
	if (report->field) \
		*buttons |= BIT(button);

	MAP(triangle, XBOX_BUTTON_TRIANGLE)
	MAP(circle, XBOX_BUTTON_CIRCLE)
	MAP(cross, XBOX_BUTTON_CROSS)
	MAP(square, XBOX_BUTTON_SQUARE)
	MAP(up, XBOX_BUTTON_UP)
	MAP(down, XBOX_BUTTON_DOWN)
	MAP(left, XBOX_BUTTON_LEFT)
	MAP(right, XBOX_BUTTON_RIGHT)
	MAP(r3, XBOX_BUTTON_R3)
	MAP(l3, XBOX_BUTTON_L3)
	MAP(start, XBOX_BUTTON_START)
	MAP(select, XBOX_BUTTON_SELECT)
	MAP(r2, XBOX_BUTTON_R2)
	MAP(l2, XBOX_BUTTON_L2)
	MAP(r1, XBOX_BUTTON_R1)
	MAP(l1, XBOX_BUTTON_L1)
	MAP(ps, XBOX_BUTTON_PS)

#undef MAP
}

static inline void xbox_get_analog_axis(const struct xbox_input_report *report,
				       u8 analog_axis[static XBOX_ANALOG_AXIS__NUM])
{
	analog_axis[XBOX_ANALOG_AXIS_LEFT_X] = report->left_x;
	analog_axis[XBOX_ANALOG_AXIS_LEFT_Y] = 255 - report->left_y;
	analog_axis[XBOX_ANALOG_AXIS_RIGHT_X] = report->right_x;
	analog_axis[XBOX_ANALOG_AXIS_RIGHT_Y] = 255 - report->right_y;
}

static inline int xbox_request_data(usb_input_device_t *device)
{
	return usb_device_driver_issue_intr_transfer_async(device, 0, device->usb_async_resp,
							   sizeof(device->usb_async_resp));
}

static inline int xboxone_write_command(usb_input_device_t *device, u8 *data, int size)
{
	struct xbox_private_data_t *priv = (void *)device->private_data;
	/* Increment the output command counter */
	data[2] = priv->command_cnt++;
	return usb_device_driver_issue_intr_transfer(device, 1, data, size);
}

static int xboxone_init(usb_input_device_t *device)
{
	static u8 buf[5] ATTRIBUTE_ALIGN(32) = {
		0x05, 0x20, 0x00, 0x01, 0x00
	};

	return xboxone_write_command(device, buf, sizeof(buf));
}

static int xboxone_send_rumble(usb_input_device_t *device, u8 strong_magnitude, u8 weak_magnitude)
{
	struct xboxone_rumble_data rumble ATTRIBUTE_ALIGN(32);
	rumble.command = 0x09;
	rumble.dummy1 = 0x00;
	rumble.size = 0x09;
	rumble.mode = 0x00;
	rumble.rumble_mask = 0x0f;
	rumble.duration = 0xff;
	rumble.period = 0x00;
	rumble.extra = 0x00;
	rumble.trigger_left = 0x00;
	rumble.trigger_right = 0x00;
	rumble.strong_magnitude = strong_magnitude;
	rumble.weak_magnitude = weak_magnitude;
	return xboxone_write_command(device, (u8 *)&rumble, sizeof(rumble));
}

static int xbox360_send_rumble(usb_input_device_t *device, u8 strong_magnitude, u8 weak_magnitude)
{
	struct xbox360_rumble_data rumble ATTRIBUTE_ALIGN(32);
	memset(&rumble, 0, sizeof(rumble));
	rumble.command = CONTROL_MESSAGE_SET_RUMBLE;
	rumble.size = sizeof(rumble);
	rumble.big = strong_magnitude;
	rumble.little = weak_magnitude;
	return usb_device_driver_issue_intr_transfer(device, 1, (u8 *)&rumble, sizeof(rumble));
}

static int xbox360_send_led_pattern(usb_input_device_t *device, enum xbox_led_pattern pattern)
{
	u8 buf[3] ATTRIBUTE_ALIGN(32);
	buf[0] = CONTROL_MESSAGE_SET_LED;
	buf[1] = sizeof(buf);
	buf[2] = pattern;
	return usb_device_driver_issue_intr_transfer(device, 1, buf, sizeof(buf));
}

bool xbox_driver_ops_probe(u16 vid, u16 pid)
{
	bool compat;

	compat = usb_driver_is_comaptible(vid, pid, compatible_xbox360,
					  ARRAY_SIZE(compatible_xbox360));
	printf("Xbox 360 probe %d\n", compat);
	if (compat)
		return true;

	compat = usb_driver_is_comaptible(vid, pid, compatible_xboxone,
					  ARRAY_SIZE(compatible_xboxone));
	printf("Xbox One probe %d\n", compat);
	if (compat)
		return true;

	return false;
}

int xbox_driver_ops_init(usb_input_device_t *device, u16 vid, u16 pid)
{
	int ret;
	struct xbox_private_data_t *priv = (void *)device->private_data;

	/* Init private state */
	priv->is_xboxone = usb_driver_is_comaptible(vid, pid, compatible_xboxone,
						    ARRAY_SIZE(compatible_xboxone));
	priv->command_cnt = 0;
	priv->rumble_on = false;
	priv->mapping = 0;
	priv->switch_input_combo_pressed = false;

	/* Set initial extension */
	fake_wiimote_set_extension(device->wiimote, input_mappings[priv->mapping].extension);

	printf("Xbox driver init\n");

	if (priv->is_xboxone) {
		ret = xboxone_init(device);
		printf("xboxone_init: %d\n", ret);
		if (ret < 0)
			return ret;
	}

	return xbox_request_data(device);
}

int xbox_driver_ops_disconnect(usb_input_device_t *device)
{
	struct xbox_private_data_t *priv = (void *)device->private_data;

	if (priv->rumble_on) {
		if (priv->is_xboxone)
			xboxone_send_rumble(device, 0, 0);
		else
			xbox360_send_rumble(device, 0, 0);
	}

	if (!priv->is_xboxone)
		xbox360_send_led_pattern(device, LED_OFF);

	priv->rumble_on = false;

	return 0;
}

int xbox_driver_ops_slot_changed(usb_input_device_t *device, u8 slot)
{
	struct xbox_private_data_t *priv = (void *)device->private_data;

	if (!priv->is_xboxone)
		return xbox360_send_led_pattern(device, LED_FLASH_TOP_LEFT + slot);

	return 0;
}

int xbox_driver_ops_set_rumble(usb_input_device_t *device, bool rumble_on)
{
	struct xbox_private_data_t *priv = (void *)device->private_data;
	u8 strong = rumble_on ? 192 : 0;
	u8 weak = rumble_on ? 64 : 0;

	priv->rumble_on = rumble_on;

	if (priv->is_xboxone)
		return xboxone_send_rumble(device, strong, weak);
	else
		return xbox360_send_rumble(device, strong, weak);
}

void xbox360_process_packet(usb_input_device_t *device, const void *data, int size)
{

}

void xboxone_process_packet(usb_input_device_t *device, const void *data, int size)
{

}

int xbox_driver_ops_usb_async_resp(usb_input_device_t *device)
{
	struct xbox_private_data_t *priv = (void *)device->private_data;
	int size = device->usb_async_resp_msg.result;

	printf("Xbox got packet: data: 0x%08x, size: 0x%x\n", *(u32 *)device->usb_async_resp, size);

	if (priv->is_xboxone)
		xboxone_process_packet(device, device->usb_async_resp, size);
	else
		xbox360_process_packet(device, device->usb_async_resp, size);

#if 0
	struct xbox_private_data_t *priv = (void *)device->private_data;
	struct xbox_input_report *report = (void *)device->usb_async_resp;
	u32 xbox_buttons = 0;
	u8 xbox_analog_axis[XBOX_ANALOG_AXIS__NUM];
	s32 xbox_acc_x, xbox_acc_y, xbox_acc_z;
	u16 acc_x, acc_y, acc_z;
	u16 wiimote_buttons = 0;
	union wiimote_extension_data_t extension_data;
	bool switch_input;

	xbox_get_buttons(report, &xbox_buttons);
	xbox_get_analog_axis(report, xbox_analog_axis);

	switch_input = (xbox_buttons & SWITCH_INPUT_MAPPING_COMBO) == SWITCH_INPUT_MAPPING_COMBO;
	if (switch_input && !priv->switch_input_combo_pressed) {
		priv->mapping = (priv->mapping + 1) % ARRAY_SIZE(input_mappings);
		fake_wiimote_set_extension(device->wiimote,
					   input_mappings[priv->mapping].extension);
	}
	priv->switch_input_combo_pressed = switch_input;

	bm_map_wiimote(XBOX_BUTTON__NUM, xbox_buttons,
		       input_mappings[priv->mapping].wiimote_button_map,
		       &wiimote_buttons);

	xbox_acc_x = (s32)report->acc_x - 511;
	xbox_acc_y = 511 - (s32)report->acc_y;
	xbox_acc_z = 511 - (s32)report->acc_z;

	/* Normalize to accelerometer calibration configuration */
	acc_x = ACCEL_ZERO_G - (xbox_acc_x * (ACCEL_ONE_G - ACCEL_ZERO_G)) / XBOX_ACC_RES_PER_G;
	acc_y = ACCEL_ZERO_G + (xbox_acc_y * (ACCEL_ONE_G - ACCEL_ZERO_G)) / XBOX_ACC_RES_PER_G;
	acc_z = ACCEL_ZERO_G + (xbox_acc_z * (ACCEL_ONE_G - ACCEL_ZERO_G)) / XBOX_ACC_RES_PER_G;

	fake_wiimote_report_accelerometer(device->wiimote, acc_x, acc_y, acc_z);

	if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_NONE) {
		fake_wiimote_report_input(device->wiimote, wiimote_buttons);
	} else if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_NUNCHUK) {
		bm_map_nunchuk(XBOX_BUTTON__NUM, xbox_buttons,
			       XBOX_ANALOG_AXIS__NUM, xbox_analog_axis,
			       0, 0, 0,
			       input_mappings[priv->mapping].nunchuk_button_map,
			       input_mappings[priv->mapping].nunchuk_analog_axis_map,
			       &extension_data.nunchuk);
		fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
					      &extension_data, sizeof(extension_data.nunchuk));
	} else if (input_mappings[priv->mapping].extension == WIIMOTE_EXT_CLASSIC) {
		bm_map_classic(XBOX_BUTTON__NUM, xbox_buttons,
			       XBOX_ANALOG_AXIS__NUM, xbox_analog_axis,
			       input_mappings[priv->mapping].classic_button_map,
			       input_mappings[priv->mapping].classic_analog_axis_map,
			       &extension_data.classic);
		fake_wiimote_report_input_ext(device->wiimote, wiimote_buttons,
					      &extension_data, sizeof(extension_data.classic));
	}
#endif
	return xbox_request_data(device);
}

const usb_device_driver_t xbox_usb_device_driver = {
	.probe		= xbox_driver_ops_probe,
	.init		= xbox_driver_ops_init,
	.disconnect	= xbox_driver_ops_disconnect,
	.slot_changed	= xbox_driver_ops_slot_changed,
	.set_rumble	= xbox_driver_ops_set_rumble,
	.usb_async_resp	= xbox_driver_ops_usb_async_resp,
};
