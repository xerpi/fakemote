#ifndef WIIMOTE_H
#define WIIMOTE_H

#include "types.h"
#include "utils.h"

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

#define WIIMOTE_REMOTE_FEATURES ((u8[]){0xBC, 0x02, 0x04, 0x38, 0x08, 0x00, 0x00, 0x00})

#define WIIMOTE_LMP_VERSION	0x2
#define WIIMOTE_LMP_SUBVERSION	0x229
/* Broadcom Corporation */
#define WIIMOTE_MANUFACTURER_ID	0x000F

#define WII_REQUEST_MTU		185
#define WIIMOTE_MAX_PAYLOAD	23

/* Wiimote -> Host */
#define INPUT_REPORT_ID_STATUS		0x20
#define INPUT_REPORT_ID_READ_DATA_REPLY	0x21
#define INPUT_REPORT_ID_ACK		0x22
/* Not a real value on the wiimote, just a state to disable reports */
#define INPUT_REPORT_ID_REPORT_DISABLED	0x00
#define INPUT_REPORT_ID_BTN		0x30
#define INPUT_REPORT_ID_BTN_ACC		0x31
#define INPUT_REPORT_ID_BTN_EXP8	0x32
#define INPUT_REPORT_ID_BTN_ACC_IR	0x33
#define INPUT_REPORT_ID_BTN_EXP19	0x34
#define INPUT_REPORT_ID_BTN_ACC_EXP	0x35
#define INPUT_REPORT_ID_BTN_IR_EXP	0x36
#define INPUT_REPORT_ID_BTN_ACC_IR_EXP	0x37
#define INPUT_REPORT_ID_EXP21		0x3d

/* Host -> Wiimote */
#define OUTPUT_REPORT_ID_RUMBLE		0x10
#define OUTPUT_REPORT_ID_LED		0x11
#define OUTPUT_REPORT_ID_REPORT_MODE	0x12
#define OUTPUT_REPORT_ID_IR_ENABLE	0x13
#define OUTPUT_REPORT_ID_SPEAKER_ENABLE	0x14
#define OUTPUT_REPORT_ID_STATUS 	0x15
#define OUTPUT_REPORT_ID_WRITE_DATA 	0x16
#define OUTPUT_REPORT_ID_READ_DATA	0x17
#define OUTPUT_REPORT_ID_SPEAKER_DATA	0x18
#define OUTPUT_REPORT_ID_SPEAKER_MUTE	0x19
#define OUTPUT_REPORT_ID_IR_ENABLE2	0x1a

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
#define CAMERA_I2C_ADDR		0x58

/* Memory sizes */
#define EEPROM_FREE_SIZE	0x1700

/* Offsets in Wiimote memory */
#define WIIMOTE_EXP_MEM_CALIBR	0x20
#define WIIMOTE_EXP_ID		0xFA

/* Wiimote button codes */
#define WIIMOTE_BUTTON_TWO		0x0001
#define WIIMOTE_BUTTON_ONE		0x0002
#define WIIMOTE_BUTTON_B		0x0004
#define WIIMOTE_BUTTON_A		0x0008
#define WIIMOTE_BUTTON_MINUS		0x0010
#define WIIMOTE_BUTTON_ZACCEL_BIT6	0x0020
#define WIIMOTE_BUTTON_ZACCEL_BIT7	0x0040
#define WIIMOTE_BUTTON_HOME		0x0080
#define WIIMOTE_BUTTON_LEFT		0x0100
#define WIIMOTE_BUTTON_RIGHT		0x0200
#define WIIMOTE_BUTTON_DOWN		0x0400
#define WIIMOTE_BUTTON_UP		0x0800
#define WIIMOTE_BUTTON_PLUS		0x1000
#define WIIMOTE_BUTTON_ZACCEL_BIT4	0x2000
#define WIIMOTE_BUTTON_ZACCEL_BIT5	0x4000
#define WIIMOTE_BUTTON_UNKNOWN		0x8000
#define WIIMOTE_BUTTON_ALL		0x1F9F

/* Nunchuk button codes */
#define NUNCHUK_BUTTON_Z	0x01
#define NUNCHUK_BUTTON_C	0x02

/* Classic Controller button codes */
#define CLASSIC_CTRL_BUTTON_UP		0x0001
#define CLASSIC_CTRL_BUTTON_LEFT	0x0002
#define CLASSIC_CTRL_BUTTON_ZR		0x0004
#define CLASSIC_CTRL_BUTTON_X		0x0008
#define CLASSIC_CTRL_BUTTON_A		0x0010
#define CLASSIC_CTRL_BUTTON_Y		0x0020
#define CLASSIC_CTRL_BUTTON_B		0x0040
#define CLASSIC_CTRL_BUTTON_ZL		0x0080
#define CLASSIC_CTRL_BUTTON_FULL_R	0x0200
#define CLASSIC_CTRL_BUTTON_PLUS	0x0400
#define CLASSIC_CTRL_BUTTON_HOME	0x0800
#define CLASSIC_CTRL_BUTTON_MINUS	0x1000
#define CLASSIC_CTRL_BUTTON_FULL_L	0x2000
#define CLASSIC_CTRL_BUTTON_DOWN	0x4000
#define CLASSIC_CTRL_BUTTON_RIGHT	0x8000
#define CLASSIC_CTRL_BUTTON_ALL		0xFEFF

/* Acceleromter configuration */
#define ACCEL_ZERO_G	(0x80 << 2)
#define ACCEL_ONE_G	(0x9A << 2)

/* IR data modes */
#define IR_MODE_BASIC		1
#define IR_MODE_EXTENDED	3
#define IR_MODE_FULL		5

/* IR configuration */
#define IR_MAX_DOTS	4
#define IR_LOW_X	0x7F
#define IR_LOW_Y	0x5D
#define IR_HIGH_X	0x380
#define IR_HIGH_Y	0x2A2
#define IR_CENTER_X	((IR_HIGH_X + IR_LOW_X) >> 1)
#define IR_CENTER_Y	((IR_HIGH_Y + IR_LOW_Y) >> 1)
#define IR_HORIZONTAL_OFFSET	64
#define IR_VERTICAL_OFFSET	110
#define IR_DOT_SIZE	4
#define IR_DOT_CENTER_MIN_X (IR_LOW_X  + IR_HORIZONTAL_OFFSET)
#define IR_DOT_CENTER_MAX_X (IR_HIGH_X - IR_HORIZONTAL_OFFSET)
#define IR_DOT_CENTER_MIN_Y (IR_LOW_Y  + IR_VERTICAL_OFFSET)
#define IR_DOT_CENTER_MAX_Y (IR_HIGH_Y - IR_VERTICAL_OFFSET)

enum wiimote_ext_e {
	WIIMOTE_EXT_NONE = 0,
	WIIMOTE_EXT_NUNCHUK,
	WIIMOTE_EXT_CLASSIC,
	WIIMOTE_EXT_CLASSIC_WIIU_PRO,
	WIIMOTE_EXT_GUITAR,
	WIIMOTE_EXT_MOTION_PLUS,
};

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

struct wiimote_output_report_enable_feature_t {
	u8 : 5;
	// Enable/disable certain feature.
	u8 enable : 1;
	// Respond with an ack.
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

/* IR camera */

#define CAMERA_DATA_BYTES	36

struct wiimote_ir_camera_registers_t {
	// Contains sensitivity and other unknown data
	// TODO: Does disabling the camera peripheral reset the mode or sensitivity?
	u8 sensitivity_block1[9];
	u8 unk_0x09[17];

	// addr: 0x1a
	u8 sensitivity_block2[2];
	u8 unk_0x1c[20];

	// addr: 0x30
	u8 enable_object_tracking;
	u8 unk_0x31[2];

	// addr: 0x33
	u8 mode;
	u8 unk_0x34[3];

	// addr: 0x37
	u8 camera_data[CAMERA_DATA_BYTES];
	u8 unk_0x5b[165];
} ATTRIBUTE_PACKED;

static_assert(sizeof(struct wiimote_ir_camera_registers_t) == 0x100);

struct ir_dot_t {
	u16 x, y;
};

/* Extensions */

#define CONTROLLER_DATA_BYTES	21

struct wiimote_extension_data_format_nunchuk_t {
	// joystick x, y
	u8 jx;
	u8 jy;
	// accelerometer
	u8 ax;
	u8 ay;
	u8 az;
	union {
		u8 hex;
		struct {
			// LSBs of accelerometer
			u8 acc_z_lsb : 2;
			u8 acc_y_lsb : 2;
			u8 acc_x_lsb : 2;
			u8 c : 1;
			u8 z : 1;
		};
	} bt;
};

struct wiimote_extension_data_format_classic_t {
	u8 rx3 : 2; // byte 0
	u8 lx : 6;

	u8 rx2 : 2; // byte 1
	u8 ly : 6;

	u8 rx1 : 1;
	u8 lt2 : 2;
	u8 ry : 5;

	u8 lt1 : 3;
	u8 rt : 5;

	union { // byte 4, 5
		u16 hex;
		struct {
			u8 dpad_right : 1;
			u8 dpad_down : 1;
			u8 lt : 1;  // left trigger
			u8 minus : 1;
			u8 home : 1;
			u8 plus : 1;
			u8 rt : 1;  // right trigger
			u8 : 1;

			u8 zl : 1;  // left z button
			u8 b : 1;
			u8 y : 1;
			u8 a : 1;
			u8 x : 1;
			u8 zr : 1;
			u8 dpad_left : 1;
			u8 dpad_up : 1;
		};
	} bt;
};

union wiimote_extension_data_t {
	struct wiimote_extension_data_format_nunchuk_t nunchuk;
	struct wiimote_extension_data_format_classic_t classic;
};
static_assert(sizeof(union wiimote_extension_data_t) <= CONTROLLER_DATA_BYTES);

#define ENCRYPTION_ENABLED 0xaa

struct wiimote_extension_registers_t {
	// 21 bytes of possible extension data
	u8 controller_data[CONTROLLER_DATA_BYTES];
	u8 unknown2[11];
	// address 0x20
	u8 calibration1[0x10];
	u8 calibration2[0x10];
	// address 0x40
	u8 encryption_key_data[0x10];
	u8 unknown4[0xA0];
	// address 0xF0
	u8 encryption;
	u8 unknown5[0x9];
	// address 0xFA
	u8 identifier[6];
} ATTRIBUTE_PACKED;
static_assert(sizeof(struct wiimote_extension_registers_t) == 0x100);

#define ENCRYPTION_KEY_DATA_BEGIN \
	offsetof(struct wiimote_extension_registers_t, encryption_key_data)

#define ENCRYPTION_KEY_DATA_END \
	(ENCRYPTION_KEY_DATA_BEGIN + \
	 MEMBER_SIZE(struct wiimote_extension_registers_t, encryption_key_data))

/* Extension IDs */

static const u8 EXT_ID_CODE_NUNCHUNK[6] 		= {0x00, 0x00, 0xa4, 0x20, 0x00, 0x00};
static const u8 EXP_ID_CODE_CLASSIC_CONTROLLER[6]	= {0x00, 0x00, 0xa4, 0x20, 0x01, 0x01};
static const u8 EXP_ID_CODE_CLASSIC_WIIU_PRO[6]		= {0x00, 0x00, 0xa4, 0x20, 0x01, 0x20};
static const u8 EXP_ID_CODE_GUITAR[6]			= {0x00, 0x00, 0xa4, 0x20, 0x01, 0x03};
static const u8 EXP_ID_CODE_MOTION_PLUS[6]		= {0x00, 0x00, 0xA6, 0x20, 0x00, 0x05};

/* EEPROM */
union wiimote_usable_eeprom_data_t {
	struct {
		// addr: 0x0000
		u8 ir_calibration_1[11];
		u8 ir_calibration_2[11];
		u8 accel_calibration_1[10];
		u8 accel_calibration_2[10];
		// addr: 0x002A
		u8 user_data[0x0FA0];
		// addr: 0x0FCA
		u8 mii_data_1[0x02f0];
		u8 mii_data_2[0x02f0];
		// addr: 0x15AA
		u8 unk_1[0x0126];
		// addr: 0x16D0
		u8 unk_2[24];
		u8 unk_3[24];
	};
	u8 data[EEPROM_FREE_SIZE];
};
static_assert(sizeof(union wiimote_usable_eeprom_data_t) == EEPROM_FREE_SIZE);

/* Helper inline functions */

static inline bool input_report_has_btn(u8 rpt_id)
{
	switch (rpt_id) {
	case INPUT_REPORT_ID_EXP21:
		return false;
	default:
		return true;
	}
}

static inline u8 input_report_acc_size(u8 rpt_id)
{
	switch (rpt_id) {
	case INPUT_REPORT_ID_BTN_ACC:
	case INPUT_REPORT_ID_BTN_ACC_IR:
	case INPUT_REPORT_ID_BTN_ACC_EXP:
	case INPUT_REPORT_ID_BTN_ACC_IR_EXP:
		return 3;
	default:
		return 0;
	}
}

static inline u8 input_report_acc_offset(u8 rpt_id)
{
	switch (rpt_id) {
	case INPUT_REPORT_ID_BTN_ACC:
	case INPUT_REPORT_ID_BTN_ACC_IR:
	case INPUT_REPORT_ID_BTN_ACC_EXP:
	case INPUT_REPORT_ID_BTN_ACC_IR_EXP:
		return 2;
	default:
		return 0;
	}
}

static inline u8 input_report_ext_size(u8 rpt_id)
{
	switch (rpt_id) {
	case INPUT_REPORT_ID_BTN_EXP8:
		return 8;
	case INPUT_REPORT_ID_BTN_EXP19:
		return 19;
	case INPUT_REPORT_ID_BTN_ACC_EXP:
		return 16;
	case INPUT_REPORT_ID_BTN_IR_EXP:
		return 9;
	case INPUT_REPORT_ID_BTN_ACC_IR_EXP:
		return 6;
	case INPUT_REPORT_ID_EXP21:
		return 21;
	default:
		return 0;
	}
}

static inline u8 input_report_ext_offset(u8 rpt_id)
{
	switch (rpt_id) {
	case INPUT_REPORT_ID_BTN_EXP8:
	case INPUT_REPORT_ID_BTN_EXP19:
		return 2;
	case INPUT_REPORT_ID_BTN_ACC_EXP:
		return 5;
	case INPUT_REPORT_ID_BTN_IR_EXP:
		return 12;
	case INPUT_REPORT_ID_BTN_ACC_IR_EXP:
		return 15;
	case INPUT_REPORT_ID_EXP21:
	default:
		return 0;
	}
}

static inline u8 input_report_ir_size(u8 rpt_id)
{
	switch (rpt_id) {
	case INPUT_REPORT_ID_BTN_ACC_IR:
		return 12;
	case INPUT_REPORT_ID_BTN_IR_EXP:
	case INPUT_REPORT_ID_BTN_ACC_IR_EXP:
		return 10;
	default:
		return 0;
	}
}

static inline u8 input_report_ir_offset(u8 rpt_id)
{
	switch (rpt_id) {
	case INPUT_REPORT_ID_BTN_ACC_IR:
	case INPUT_REPORT_ID_BTN_ACC_IR_EXP:
		return 5;
	case INPUT_REPORT_ID_BTN_IR_EXP:
		return 2;
	default:
		return 0;
	}
}

#endif
