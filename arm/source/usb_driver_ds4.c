#include "usb_device_drivers.h"
#include "wiimote.h"

struct ds4_input {
	u8 report_id;
	u8 left_x;
	u8 left_y;
	u8 right_x;
	u8 righty_;

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

	u32 finger1active : 1;
	u32 finger1_id     : 7;
	u32 finger1_x      : 12;
	u32 finger1_y      : 12;

	u32 finger2active : 1;
	u32 finger2_id    : 7;
	u32 finger2_x     : 12;
	u32 finger2_y     : 12;
} __attribute__((packed, aligned(32)));

static inline void ds4_map_buttons(const struct ds4_input *input, u16 *buttons)
{
	if (input->cross)
		*buttons |= WPAD_BUTTON_A;
	if (input->circle)
		*buttons |= WPAD_BUTTON_B;
	if (input->l3)
		*buttons |= WPAD_BUTTON_1;
	if (input->r3)
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

	return usb_device_driver_issue_write_intr(device, buf, sizeof(buf));
}

static inline int ds4_request_data(usb_input_device_t *device)
{
	return usb_device_driver_issue_read_intr_async(device);
}

int ds4_driver_ops_init(usb_input_device_t *device)
{
	return ds4_request_data(device);
}

int ds4_driver_ops_disconnect(usb_input_device_t *device)
{
	/* TODO */
	return 0;
}

int ds4_driver_ops_slot_changed(usb_input_device_t *device, u8 slot)
{
	static u8 colors[5][3] = {
		{  0,   0,   0},
		{  0,   0, 255},
		{255,   0,   0},
		{0,   255,   0},
		{255,   0, 255},
	};

	u8 r = colors[slot][0],
	   g = colors[slot][1],
	   b = colors[slot][2];

	return ds4_set_leds_rumble(device, r, g, b);
}

int ds4_driver_ops_usb_intr_in_resp(usb_input_device_t *device)
{
	u16 buttons = 0;

	ds4_map_buttons((struct ds4_input *)device->usb_intr_in_data, &buttons);

	fake_wiimote_mgr_report_input(device->wiimote, buttons);

	return ds4_request_data(device);
}
