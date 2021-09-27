#include "usb_device_drivers.h"
#include "usb.h"
#include "utils.h"
#include "wiimote.h"

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

static inline void ds3_map_buttons(const struct ds3_input_report *input, u16 *buttons)
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

int ds3_driver_ops_init(usb_input_device_t *device)
{
	int ret;

	ret = ds3_set_operational(device);
	if (ret < 0)
		return ret;

	ret = ds3_request_data(device);
	if (ret < 0)
		return ret;

	return 0;
}

int ds3_driver_ops_disconnect(usb_input_device_t *device)
{
	/* Do nothing */
	return 0;
}

int ds3_driver_ops_slot_changed(usb_input_device_t *device, u8 slot)
{
	static const u8 led_pattern[] = {0x0, 0x02, 0x04, 0x08, 0x10, 0x12, 0x14, 0x18};

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

	buf[9] = led_pattern[slot % ARRAY_SIZE(led_pattern)];

	return usb_device_driver_issue_ctrl_transfer(device,
						     USB_REQTYPE_INTERFACE_SET,
						     USB_REQ_SETREPORT,
						     (USB_REPTYPE_OUTPUT << 8) | 0x01, 0,
						     buf, sizeof(buf));
}

int ds3_driver_ops_usb_async_resp(usb_input_device_t *device)
{
	struct ds3_input_report *report = (void *)device->usb_async_resp;
	u16 buttons = 0;

	ds3_map_buttons(report, &buttons);
	fake_wiimote_mgr_report_input(device->wiimote, buttons);

	return ds3_request_data(device);
}
