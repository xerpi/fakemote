#include "usb_device_drivers.h"
#include "utils.h"
#include "wiimote.h"

#define DS4_TOUCHPAD_W 1920
#define DS4_TOUCHPAD_H 940

struct ds4_private_data_t {
	enum wiimote_mgr_ext_u extension;
};
static_assert(sizeof(struct ds4_private_data_t) <= USB_INPUT_DEVICE_PRIVATE_DATA_SIZE);

struct ds4_input_report {
	u8 report_id;
	u8 left_x;
	u8 left_y;
	u8 right_x;
	u8 right_y;

	u8 triangle : 1;
	u8 circle   : 1;
	u8 cross    : 1;
	u8 square   : 1;
	u8 dpad	    : 4;

	u8 r3      : 1;
	u8 l3      : 1;
	u8 options : 1;
	u8 share   : 1;
	u8 r2      : 1;
	u8 l2      : 1;
	u8 r1      : 1;
	u8 l1      : 1;

	u8 cnt1   : 6;
	u8 tpad   : 1;
	u8 ps     : 1;

	u8 l_trigger;
	u8 r_trigger;

	u8 cnt2;
	u8 cnt3;

	u8 battery;

	s16 accel_x;
	s16 accel_y;
	s16 accel_z;

	union {
		s16 roll;
		s16 gyro_z;
	};

	union {
		s16 yaw;
		s16 gyro_y;
	};

	union {
		s16 pitch;
		s16 gyro_x;
	};

	u8 unk1[5];

	u8 padding       : 1;
	u8 microphone    : 1;
	u8 headphones    : 1;
	u8 usb_plugged   : 1;
	u8 battery_level : 4;

	u8 unk2[2];
	u8 trackpadpackets;
	u8 packetcnt;

	u8 finger1_nactive : 1;
	u8 finger1_id      : 7;
	u8 finger1_x_lo;
	u8 finger1_y_lo    : 4;
	u8 finger1_x_hi    : 4;
	u8 finger1_y_hi;

	u8 finger2_nactive : 1;
	u8 finger2_id      : 7;
	u8 finger2_x_lo;
	u8 finger2_y_lo    : 4;
	u8 finger2_x_hi    : 4;
	u8 finger2_y_hi;
} ATTRIBUTE_PACKED;

static inline void ds4_map_buttons(const struct ds4_input_report *input, u16 *buttons)
{
	if (input->dpad == 0 || input->dpad == 1 || input->dpad == 7)
		*buttons |= WPAD_BUTTON_UP;
	else if (input->dpad == 3 || input->dpad == 4 || input->dpad == 5)
		*buttons |= WPAD_BUTTON_DOWN;
	if (input->dpad == 1 || input->dpad == 2 || input->dpad == 3)
		*buttons |= WPAD_BUTTON_RIGHT;
	else if (input->dpad == 5 || input->dpad == 6 || input->dpad == 7)
		*buttons |= WPAD_BUTTON_LEFT;
	if (input->cross || input->tpad)
		*buttons |= WPAD_BUTTON_A;
	if (input->circle)
		*buttons |= WPAD_BUTTON_B;
	if (input->triangle)
		*buttons |= WPAD_BUTTON_1;
	if (input->square)
		*buttons |= WPAD_BUTTON_2;
	if (input->ps)
		*buttons |= WPAD_BUTTON_HOME;
	if (input->share)
		*buttons |= WPAD_BUTTON_MINUS;
	if (input->options)
		*buttons |= WPAD_BUTTON_PLUS;
}

static int ds4_set_leds_rumble(usb_input_device_t *device, u8 r, u8 g, u8 b)
{
	u8 buf[] ATTRIBUTE_ALIGN(32) = {
		0x05, // Report ID
		0x03, 0x00, 0x00,
		0x00, // Fast motor
		0x00, // Slow motor
		r, g, b, // RGB
		0x00, // LED on duration
		0x00  // LED off duration
	};

	return usb_device_driver_issue_intr_transfer(device, 1, buf, sizeof(buf));
}

static inline int ds4_request_data(usb_input_device_t *device)
{
	return usb_device_driver_issue_intr_transfer_async(device, 0, device->usb_async_resp,
							   sizeof(device->usb_async_resp));
}

int ds4_driver_ops_init(usb_input_device_t *device)
{
	struct ds4_private_data_t *priv = (void *)device->private_data;

	/* Set initial extension */
	priv->extension = WIIMOTE_MGR_EXT_NUNCHUK;
	fake_wiimote_mgr_set_extension(device->wiimote, priv->extension);

	return ds4_request_data(device);
}

int ds4_driver_ops_disconnect(usb_input_device_t *device)
{
	ds4_set_leds_rumble(device, 0, 0, 0);
	return 0;
}

int ds4_driver_ops_slot_changed(usb_input_device_t *device, u8 slot)
{
	static u8 colors[5][3] = {
		{  0,   0,   0},
		{  0,   0,  32},
		{ 32,   0,   0},
		{  0,  32,   0},
		{ 32,   0,  32},
	};

	slot = slot % ARRAY_SIZE(colors);

	u8 r = colors[slot][0],
	   g = colors[slot][1],
	   b = colors[slot][2];

	return ds4_set_leds_rumble(device, r, g, b);
}

int ds4_driver_ops_usb_async_resp(usb_input_device_t *device)
{
	struct ds4_private_data_t *priv = (void *)device->private_data;
	struct ds4_input_report *report = (void *)device->usb_async_resp;
	u16 buttons = 0;
	struct wiimote_extension_data_format_nunchuk_t nunchuk;
	u32 f_x, f_y;
	struct ir_dot_t ir_dots[2];
	u8 num_ir_dots = 0;

	if (report->report_id == 0x01) {
		ds4_map_buttons(report, &buttons);

		if (!report->finger1_nactive) {
			f_x = report->finger1_x_lo | ((u32)report->finger1_x_hi << 8);
			f_y = report->finger1_y_lo | ((u32)report->finger1_y_hi << 4);
			ir_dots[0].x = IR_DOT_CENTER_MIN_X + (f_x * (IR_DOT_CENTER_MAX_X - IR_DOT_CENTER_MIN_X)) / (DS4_TOUCHPAD_W - 1);
			ir_dots[0].y = IR_DOT_CENTER_MIN_Y + (f_y * (IR_DOT_CENTER_MAX_Y - IR_DOT_CENTER_MIN_Y)) / (DS4_TOUCHPAD_H - 1);
			num_ir_dots++;
		}

		if (!report->finger2_nactive) {
			f_x = report->finger2_x_lo | ((u32)report->finger2_x_hi << 8);
			f_y = report->finger2_y_lo | ((u32)report->finger2_y_hi << 4);
			ir_dots[1].x = IR_DOT_CENTER_MIN_X + (f_x * (IR_DOT_CENTER_MAX_X - IR_DOT_CENTER_MIN_X)) / (DS4_TOUCHPAD_W - 1);
			ir_dots[1].y = IR_DOT_CENTER_MIN_Y + (f_y * (IR_DOT_CENTER_MAX_Y - IR_DOT_CENTER_MIN_Y)) / (DS4_TOUCHPAD_H - 1);
			num_ir_dots++;
		}

		fake_wiimote_mgr_report_ir_dots(device->wiimote, num_ir_dots, ir_dots);

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
	}

	return ds4_request_data(device);
}
