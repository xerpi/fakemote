#ifndef WIIMOTE_H
#define WIIMOTE_H

#include "types.h"

/* Source: HID_010_SPC_PFL/1.0 (official HID specification) and Dolphin emulator */
#define HID_TYPE_HANDSHAKE	0
#define HID_TYPE_SET_REPORT	5
#define HID_TYPE_DATA		0xA
#define HID_HANDSHAKE_SUCCESS	0
#define HID_PARAM_INPUT		1
#define HID_PARAM_OUTPUT	2

/* Wiimote definitions */
#define WIIMOTE_HCI_CLASS_0	0x00
#define WIIMOTE_HCI_CLASS_1	0x04
#define WIIMOTE_HCI_CLASS_2	0x48

#define WII_REQUEST_MTU		185
#define WIIMOTE_MAX_PAYLOAD	23

/* Wiimote -> Host */
#define INPUT_REPORT_ID_STATUS		0x20
#define INPUT_REPORT_ID_READ_DATA_REPLY	0x21
#define INPUT_REPORT_ID_ACK		0x22
#define INPUT_REPORT_ID_REPORT_CORE	0x30

/* Host -> Wiimote */
#define OUTPUT_REPORT_ID_LED		0x11
#define OUTPUT_REPORT_ID_REPORT_MODE	0x12
#define OUTPUT_REPORT_ID_STATUS 	0x15
#define OUTPUT_REPORT_ID_READ_DATA	0x17

#define ERROR_CODE_SUCCESS	0

struct wiimote_input_report_ack_t {
	u16 buttons;
	u8 rpt_id;
	u8 error_code;
} ATTRIBUTE_PACKED;

struct wiimote_input_report_status_t {
	u16 buttons;
	u8 leds : 4;
	u8 ir : 1;
	u8 speaker : 1;
	u8 extension : 1;
	u8 battery_low : 1;
	u8 padding2[2];
	u8 battery;
} ATTRIBUTE_PACKED;

struct wiimote_input_report_read_data_t {
	u16 buttons;
	u8 size_minus_one : 4;
	u8 error : 4;
	// big endian:
	u16 address;
	u8 data[16];
} ATTRIBUTE_PACKED;

struct wiimote_output_report_led_t {
	u8 leds : 4;
	u8 : 2;
	u8 ack : 1;
	u8 rumble : 1;
} ATTRIBUTE_PACKED;

struct wiimote_output_report_mode_t {
	u8 : 5;
	u8 continuous : 1;
	u8 ack : 1;
	u8 rumble : 1;
	u8 mode;
} ATTRIBUTE_PACKED;

struct wiimote_output_report_read_data_t {
	u8 : 4;
	u8 space : 2;
	u8 : 1;
	u8 rumble : 1;
	// Used only for register space (i2c bus) (7-bits):
	u8 slave_address : 7;
	// A real wiimote ignores the i2c read/write bit.
	u8 i2c_rw_ignored : 1;
	// big endian:
	u16 address;
	u16 size;
};

#endif
