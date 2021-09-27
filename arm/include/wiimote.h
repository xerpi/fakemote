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
#define OUTPUT_REPORT_ID_WRITE_DATA 	0x16
#define OUTPUT_REPORT_ID_READ_DATA	0x17

/* Error codes */
#define ERROR_CODE_SUCCESS		0
#define ERROR_CODE_BUSY			4
#define ERROR_CODE_INVALID_SPACE	6
#define ERROR_CODE_NACK			7
#define ERROR_CODE_INVALID_ADDRESS	8

/* Address spaces */
#define ADDRESS_SPACE_EEPROM		0x00
#define ADDRESS_SPACE_I2C_BUS		0x01
#define ADDRESS_SPACE_I2C_BUS_ALT	0x02

/* I2C addresses */
#define EEPROM_I2C_ADDR		0x50
#define EXTENSION_I2C_ADDR	0x52

/* Offsets in Wiimote memory */
#define WIIMOTE_EXP_MEM_CALIBR	0x20
#define WIIMOTE_EXP_ID		0xFA

/* Buttons */
#define WPAD_BUTTON_2		0x0001
#define WPAD_BUTTON_1		0x0002
#define WPAD_BUTTON_B		0x0004
#define WPAD_BUTTON_A		0x0008
#define WPAD_BUTTON_MINUS	0x0010
#define WPAD_BUTTON_HOME	0x0080
#define WPAD_BUTTON_LEFT	0x0100
#define WPAD_BUTTON_RIGHT	0x0200
#define WPAD_BUTTON_DOWN	0x0400
#define WPAD_BUTTON_UP		0x0800
#define WPAD_BUTTON_PLUS	0x1000

/* Input reports (Wiimote -> Host) */

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

/* Output reports (Host -> Wiimote) */

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

struct wiimote_output_report_write_data_t {
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
	u8 size;
	u8 data[16];
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
} ATTRIBUTE_PACKED;

/* Extensions */

#define CONTROLLER_DATA_BYTES	21

struct wiimote_extension_registers_t {
	// 21 bytes of possible extension data
	u8 controller_data[CONTROLLER_DATA_BYTES];
	u8 unknown2[11];
	// address 0x20
	u8 calibration[0x10];
	u8 unknown3[0x10];
	// address 0x40
	u8 encryption_key_data[0x10];
	u8 unknown4[0xA0];
	// address 0xF0
	u8 encryption;
	u8 unknown5[0x9];
	// address 0xFA
	u8 identifier[6];
} ATTRIBUTE_PACKED;

/* Extension IDs */

#define EXT_NUNCHUNK_ID ((u8[6]){0x00, 0x00, 0xa4, 0x20, 0x00, 0x00})

#endif
