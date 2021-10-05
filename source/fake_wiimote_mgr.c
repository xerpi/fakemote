#include <string.h>
#include "fake_wiimote_mgr.h"
#include "hci.h"
#include "hci_state.h"
#include "l2cap.h"
#include "syscalls.h"
#include "utils.h"
#include "wiimote.h"
#include "wiimote_crypto.h"

typedef enum {
	BASEBAND_STATE_INACTIVE,
	BASEBAND_STATE_REQUEST_CONNECTION,
	BASEBAND_STATE_COMPLETE
} baseband_state_e;

typedef enum {
	ACL_STATE_INACTIVE,
	ACL_STATE_LINKING
} acl_state_e;

typedef enum {
	L2CAP_CHANNEL_STATE_INACTIVE_INACTIVE,
	L2CAP_CHANNEL_STATE_INACTIVE_CONFIG_PEND,
	L2CAP_CHANNEL_STATE_INACTIVE_COMPLETE
} l2cap_channel_state_e;

typedef struct {
	bool valid;
	l2cap_channel_state_e state;
	u16 psm;
	u16 local_cid;
	u16 remote_cid;
	u16 remote_mtu;
} l2cap_channel_info_t;

typedef struct fake_wiimote_t {
	bool active;
	bdaddr_t bdaddr;
	/* Bluetooth connection state */
	u16 hci_con_handle;
	baseband_state_e baseband_state;
	acl_state_e acl_state;
	l2cap_channel_info_t psm_sdp_chn;
	l2cap_channel_info_t psm_hid_cntl_chn;
	l2cap_channel_info_t psm_hid_intr_chn;
	/* Associated input device with this fake Wiimote */
	void *usrdata;
	const input_device_ops_t *input_device_ops;
	/* Reporting mode */
	u8 reporting_mode;
	bool reporting_continuous;
	/* Status */
	struct {
		u8 leds : 4;
		u8 ir : 1;
		u8 speaker : 1;
	} status;
	/* Input, IR, accelerometer and extension state */
	u16 buttons;
	bool input_dirty;
	/* Rumble */
	bool rumble_on;
	/* IR camera */
	struct wiimote_ir_camera_registers_t ir_regs;
	struct ir_dot_t ir_dots[2];
	u8 ir_valid_dots;
	/* Extension */
	struct wiimote_extension_registers_t extension_regs;
	struct wiimote_encryption_key_t extension_key;
	enum wiimote_mgr_ext_u cur_extension;
	enum wiimote_mgr_ext_u new_extension;
	bool extension_key_dirty;
	/* EEPROM */
	union wiimote_usable_eeprom_data_t eeprom;
	/* Current in-progress "memory read request" */
	struct {
		u8 space;
		u8 slave_address;
		u16 address;
		u16 size;
	} read_request;
} fake_wiimote_t;

static fake_wiimote_t fake_wiimotes[MAX_FAKE_WIIMOTES];

/* Helper functions */

static inline bool fake_wiimote_is_connected(const fake_wiimote_t *wiimote)
{
	return wiimote->baseband_state == BASEBAND_STATE_COMPLETE;
}

/* Channel bookkeeping */

static inline u16 generate_l2cap_channel_id(void)
{
	/* "Identifiers from 0x0001 to 0x003F are reserved" */
	static u16 starting_id = 0x40;
	return starting_id++;
}

static inline bool l2cap_channel_is_accepted(const l2cap_channel_info_t *info)
{
	return info->valid && (info->remote_cid != L2CAP_NULL_CID);
}

static inline bool l2cap_channel_is_is_remote_configured(const l2cap_channel_info_t *info)
{
	return info->valid && (info->remote_mtu != 0);
}

static inline bool l2cap_channel_is_complete(const l2cap_channel_info_t *info)
{
	return info->valid &&
	       l2cap_channel_is_accepted(info) &&
	       l2cap_channel_is_is_remote_configured(info) &&
	       (info->state == L2CAP_CHANNEL_STATE_INACTIVE_COMPLETE);
}

static l2cap_channel_info_t *get_channel_info(fake_wiimote_t *dev, u16 local_cid)
{
	if (dev->psm_sdp_chn.valid && (local_cid == dev->psm_sdp_chn.local_cid)) {
		return &dev->psm_sdp_chn;
	} else if (dev->psm_hid_cntl_chn.valid && (local_cid == dev->psm_hid_cntl_chn.local_cid)) {
		return &dev->psm_hid_cntl_chn;
	} else if (dev->psm_hid_intr_chn.valid && (local_cid == dev->psm_hid_intr_chn.local_cid)) {
		return &dev->psm_hid_intr_chn;
	}
	return NULL;
}

static void l2cap_channel_info_setup(l2cap_channel_info_t *info, u16 psm, u16 local_cid)
{
	info->psm = psm;
	info->state = L2CAP_CHANNEL_STATE_INACTIVE_INACTIVE;
	info->local_cid = local_cid;
	info->remote_cid = L2CAP_NULL_CID;
	info->remote_mtu = 0;
	info->valid = true;
}

/* HID reports */

static int send_hid_data(u16 hci_con_handle, u16 dcid, u8 hid_type, const void *data, u32 size)
{
	u8 buf[WIIMOTE_MAX_PAYLOAD];
	assert(size <= (WIIMOTE_MAX_PAYLOAD - 1));
	buf[0] = hid_type;
	memcpy(&buf[1], data, size);
	return l2cap_send_msg(hci_con_handle, dcid, buf, size + 1);
}

static inline int send_hid_input_report(u16 hci_con_handle, u16 dcid, u8 report_id,
					const void *data, u32 size)
{
	u8 buf[WIIMOTE_MAX_PAYLOAD - 1];
	assert(size <= (WIIMOTE_MAX_PAYLOAD - 2));
	buf[0] = report_id;
	memcpy(&buf[1], data, size);
	return send_hid_data(hci_con_handle, dcid, (HID_TYPE_DATA << 4) | HID_PARAM_INPUT, buf, size + 1);
}

static int wiimote_send_ack(const fake_wiimote_t *wiimote, u8 rpt_id, u8 error_code)
{
	struct wiimote_input_report_ack_t ack;
	ack.buttons = wiimote->buttons;
	ack.rpt_id = rpt_id;
	ack.error_code = error_code;
	return send_hid_input_report(wiimote->hci_con_handle, wiimote->psm_hid_intr_chn.remote_cid,
				     INPUT_REPORT_ID_ACK, &ack, sizeof(ack));
}

static int wiimote_send_input_report_status(const fake_wiimote_t *wiimote)
{
	struct wiimote_input_report_status_t status;
	status.buttons = wiimote->buttons;
	status.leds = wiimote->status.leds;
	status.ir = wiimote->status.ir;
	status.speaker = 0;
	status.extension = fake_wiimotes->cur_extension != WIIMOTE_MGR_EXT_NONE;
	status.battery_low = 0;
	status.battery = 0xFF;
	return send_hid_input_report(wiimote->hci_con_handle, wiimote->psm_hid_intr_chn.remote_cid,
				     INPUT_REPORT_ID_STATUS, &status, sizeof(status));
}

/* Disconnection helper functions */

static inline int disconnect_l2cap_channel(u16 hci_con_handle, l2cap_channel_info_t *info)
{
	int ret;
	ret = l2cap_send_disconnect_req(hci_con_handle, info->remote_cid, info->local_cid);
	info->valid = false;
	return ret;
}

static int fake_wiimote_disconnect(fake_wiimote_t *wiimote)
{
	int ret = 0, ret2;

	/* If we had a L2CAP Interrupt channel connection, notify the driver of disconnection */
	if (l2cap_channel_is_complete(&wiimote->psm_hid_intr_chn)) {
		if (wiimote->input_device_ops->disconnect)
			wiimote->input_device_ops->disconnect(wiimote->usrdata);
	}

	if (l2cap_channel_is_accepted(&wiimote->psm_sdp_chn)) {
		ret2 = disconnect_l2cap_channel(wiimote->hci_con_handle, &wiimote->psm_sdp_chn);
		if (ret2 < 0 && ret == 0)
			ret = ret2;
	}

	if (l2cap_channel_is_accepted(&wiimote->psm_hid_cntl_chn)) {
		ret2 = disconnect_l2cap_channel(wiimote->hci_con_handle, &wiimote->psm_hid_cntl_chn);
		if (ret2 < 0 && ret == 0)
			ret = ret2;
	}

	if (l2cap_channel_is_accepted(&wiimote->psm_hid_intr_chn)) {
		ret2 = disconnect_l2cap_channel(wiimote->hci_con_handle, &wiimote->psm_hid_intr_chn);
		if (ret2 < 0 && ret == 0)
			ret = ret2;
	}

	if (wiimote->baseband_state == BASEBAND_STATE_COMPLETE) {
		ret2 = enqueue_hci_event_discon_compl(wiimote->hci_con_handle,
						      0, 0x13 /* User Ended Connection */);
		if (ret2 < 0 && ret == 0)
			ret = ret2;
	}

	wiimote->active = false;

	return ret;
}

/* Init state */

void fake_wiimote_mgr_init(void)
{
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		fake_wiimotes[i].active = false;
		/* We can set it now, since it's permanent */
		fake_wiimotes[i].bdaddr = FAKE_WIIMOTE_BDADDR(i);
	}
}

static inline u8 calculate_calibration_data_checksum(const u8 *data, u8 size)
{
	u8 sum = 0x55;

	for (u8 i = 0; i < size; i++)
		sum += data[i];

	return sum;
}

static void eeprom_init(union wiimote_usable_eeprom_data_t *eeprom)
{
	u8 ir_checksum, accel_checksum;
	memset(eeprom->data, 0, sizeof(eeprom->data));

	static const u8 ir_calibration[10] = {
		/* Point 1 */
		IR_LOW_X & 0xFF,
		IR_LOW_Y & 0xFF,
		/* Mix */
		((IR_LOW_Y & 0x300) >> 2) | ((IR_LOW_X & 0x300) >> 4) | ((IR_LOW_Y & 0x300) >> 6) |
			((IR_HIGH_X & 0x300) >> 8),
		/* Point 2 */
		IR_HIGH_X & 0xFF,
		IR_LOW_Y & 0xFF,
		/* Point 3 */
		IR_HIGH_X & 0xFF,
		IR_HIGH_Y & 0xFF,
		/* Mix */
		((IR_HIGH_Y & 0x300) >> 2) | ((IR_HIGH_X & 0x300) >> 4) | ((IR_HIGH_Y & 0x300) >> 6) |
			((IR_LOW_X & 0x300) >> 8),
		/* Point 4 */
		IR_LOW_X & 0xFF,
		IR_HIGH_Y & 0xFF,
	};

	ir_checksum = calculate_calibration_data_checksum(ir_calibration, sizeof(ir_calibration));
	/* Copy to IR calibration data 1 */
	memcpy(eeprom->ir_calibration_1, ir_calibration, sizeof(ir_calibration));
	eeprom->ir_calibration_1[sizeof(eeprom->ir_calibration_1) - 1] = ir_checksum;
	/* Copy to IR calibration data 2 */
	memcpy(eeprom->ir_calibration_2, ir_calibration, sizeof(ir_calibration));
	eeprom->ir_calibration_2[sizeof(eeprom->ir_calibration_2) - 1] = ir_checksum;

	static const u8 accel_calibration[9] = {
		ACCEL_ZERO_G, ACCEL_ZERO_G, ACCEL_ZERO_G, 0,
		ACCEL_ONE_G,  ACCEL_ONE_G,  ACCEL_ONE_G,  0,
		0,
	};

	accel_checksum = calculate_calibration_data_checksum(accel_calibration, sizeof(accel_calibration));
	/* Copy to accelerometer calibration data 1 */
	memcpy(eeprom->accel_calibration_1, ir_calibration, sizeof(ir_calibration));
	eeprom->accel_calibration_1[sizeof(eeprom->accel_calibration_1) - 1] = accel_checksum;
	/* Copy to accelerometer calibration data 2 */
	memcpy(eeprom->accel_calibration_2, ir_calibration, sizeof(ir_calibration));
	eeprom->accel_calibration_2[sizeof(eeprom->accel_calibration_2) - 1] = accel_checksum;
}

bool fake_wiimote_mgr_add_input_device(void *usrdata, const input_device_ops_t *ops)
{
	/* Find an inactive fake Wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (fake_wiimotes[i].active)
			continue;

		fake_wiimotes[i].baseband_state = BASEBAND_STATE_REQUEST_CONNECTION;
		fake_wiimotes[i].acl_state = L2CAP_CHANNEL_STATE_INACTIVE_INACTIVE;
		fake_wiimotes[i].psm_sdp_chn.valid = false;
		fake_wiimotes[i].psm_hid_cntl_chn.valid = false;
		fake_wiimotes[i].psm_hid_intr_chn.valid = false;
		fake_wiimotes[i].usrdata = usrdata;
		fake_wiimotes[i].input_device_ops = ops;
		fake_wiimotes[i].status.leds = 0;
		fake_wiimotes[i].status.ir = 0;
		fake_wiimotes[i].status.speaker = 0;
		fake_wiimotes[i].buttons = 0;
		fake_wiimotes[i].input_dirty = false;
		fake_wiimotes[i].rumble_on = false;
		memset(&fake_wiimotes[i].ir_regs, 0, sizeof(fake_wiimotes[i].ir_regs));
		fake_wiimotes[i].ir_valid_dots = 0;
		memset(&fake_wiimotes[i].extension_regs, 0, sizeof(fake_wiimotes[i].extension_regs));
		memset(&fake_wiimotes[i].extension_key, 0, sizeof(fake_wiimotes[i].extension_key));
		fake_wiimotes[i].cur_extension = WIIMOTE_MGR_EXT_NONE;
		fake_wiimotes[i].new_extension = WIIMOTE_MGR_EXT_NONE;
		fake_wiimotes[i].extension_key_dirty = true;
		eeprom_init(&fake_wiimotes[i].eeprom);
		fake_wiimotes[i].read_request.size = 0;
		fake_wiimotes[i].reporting_mode = INPUT_REPORT_ID_BTN;
		fake_wiimotes[i].reporting_continuous = false;
		fake_wiimotes[i].active = true;
		return true;
	}
	return false;
}

bool fake_wiimote_mgr_remove_input_device(fake_wiimote_t *wiimote)
{
	return fake_wiimote_disconnect(wiimote) == IOS_OK;
}

void fake_wiimote_mgr_set_extension(fake_wiimote_t *wiimote, enum wiimote_mgr_ext_u ext)
{
	wiimote->new_extension = ext;
}

void fake_wiimote_mgr_report_input(fake_wiimote_t *wiimote, u16 buttons)
{
	bool btn_changed = (wiimote->buttons ^ buttons) != 0;

	if (btn_changed) {
		wiimote->buttons = buttons;
		wiimote->input_dirty = true;
	}
}

void fake_wiimote_mgr_report_ir_dots(fake_wiimote_t *wiimote, u8 num_dots, struct ir_dot_t *dots)
{
	u8 *ir_data = wiimote->ir_regs.camera_data;
	struct ir_dot_t ir_dots[2];

	/* TODO: For now we only care about 1 reported dot... */
	if (num_dots == 0) {
		ir_dots[0].x = 1023;
		ir_dots[0].y = 1023;
		ir_dots[1].x = 1023;
		ir_dots[1].y = 1023;
	} else {
		ir_dots[0].x = (IR_HIGH_X - dots[0].x) - IR_HORIZONTAL_OFFSET;
		ir_dots[0].y = dots[0].y + IR_VERTICAL_OFFSET;
		ir_dots[1].x = (IR_HIGH_X - dots[0].x) + IR_HORIZONTAL_OFFSET;
		ir_dots[1].y = dots[0].y + IR_VERTICAL_OFFSET;
	}

	switch (wiimote->ir_regs.mode) {
	case IR_MODE_BASIC:
		ir_data[0] = ir_dots[0].x & 0xFF;
		ir_data[1] = ir_dots[0].y & 0xFF;
		ir_data[2] = (((ir_dots[0].y >> 8) & 3) << 6) |
			     (((ir_dots[0].x >> 8) & 3) << 4) |
			     (((ir_dots[1].y >> 8) & 3) << 2) |
			      ((ir_dots[1].x >> 8) & 3);
		ir_data[3] = ir_dots[1].x & 0xFF;
		ir_data[4] = ir_dots[1].y & 0xFF;
		ir_data[5] = 0xFF;
		ir_data[6] = 0xFF;
		ir_data[7] = 0xFF;
		ir_data[8] = 0xFF;
		ir_data[9] = 0xFF;
		break;
	case IR_MODE_EXTENDED:
		ir_data[0] = ir_dots[0].x & 0xFF;
		ir_data[1] = ir_dots[0].y & 0xFF;
		ir_data[2] = ((ir_dots[0].y & 0x300) >> 2) |
			     ((ir_dots[0].x & 0x300) >> 4) |
			     IR_DOT_SIZE;
		ir_data[3] = ir_dots[1].x & 0xFF;
		ir_data[4] = ir_dots[1].y & 0xFF;
		ir_data[5] = ((ir_dots[1].y & 0x300) >> 2) |
			     ((ir_dots[1].x & 0x300) >> 4) |
			     IR_DOT_SIZE;
		ir_data[6] = 0xFF;
		ir_data[7] = 0xFF;
		ir_data[8] = 0xF0;
		ir_data[9] = 0xFF;
		ir_data[10] = 0xFF;
		ir_data[11] = 0xF0;
		break;
	case IR_MODE_FULL:
		ir_data[0] = ir_dots[0].x & 0xFF;
		ir_data[1] = ir_dots[0].y & 0xFF;
		ir_data[2] = ((ir_dots[0].y & 0x300) >> 2) |
			     ((ir_dots[0].x & 0x300) >> 4) |
			     IR_DOT_SIZE;
		ir_data[3] = 0;
		ir_data[4] = 0x7F;
		ir_data[5] = 0;
		ir_data[6] = 0x7F;
		ir_data[7] = 0;
		ir_data[8] = 0xFF;
		ir_data[9] = ir_dots[1].x & 0xFF;
		ir_data[10] = ir_dots[1].y & 0xFF;
		ir_data[11] = ((ir_dots[1].y & 0x300) >> 2) |
			      ((ir_dots[1].x & 0x300) >> 4) |
			      IR_DOT_SIZE;
		ir_data[12] = 0;
		ir_data[13] = 0x7F;
		ir_data[14] = 0;
		ir_data[15] = 0x7F;
		ir_data[16] = 0;
		ir_data[17] = 0xFF;
		memset(&ir_data[18], 0xFF, 2 * 9);
		break;
	default:
		/* This seems to be fairly common, 0xff data is sent in this case */
		memset(ir_data, 0xFF, sizeof(wiimote->ir_regs.camera_data));
		break;
	}
}

void fake_wiimote_mgr_report_input_ext(fake_wiimote_t *wiimote, u16 buttons, const void *ext_data, u8 ext_size)
{
	u8 *ext_controller_data = wiimote->extension_regs.controller_data;
	bool btn_changed = (wiimote->buttons ^ buttons) != 0;
	int ext_cmp = memmismatch(ext_controller_data, ext_data, ext_size);

	if (btn_changed || (ext_cmp != ext_size)) {
		wiimote->buttons = buttons;
		/* If there are changes to the extension bytes, copy them */
		if (ext_cmp != ext_size)
			memcpy(ext_controller_data + ext_cmp, ext_data + ext_cmp, ext_size - ext_cmp);
		wiimote->input_dirty = true;
	}
}

static void check_send_config_for_new_channel(u16 hci_con_handle, l2cap_channel_info_t *info)
{
	int ret;

	if (l2cap_channel_is_accepted(info) &&
	    (info->state == L2CAP_CHANNEL_STATE_INACTIVE_INACTIVE)) {
		ret = l2cap_send_config_req(hci_con_handle, info->remote_cid,
					    WII_REQUEST_MTU, L2CAP_FLUSH_TIMO_DEFAULT);
		if (ret == IOS_OK) {
			info->state = L2CAP_CHANNEL_STATE_INACTIVE_CONFIG_PEND;
		}
	}
}

static inline bool ir_camera_read_data(fake_wiimote_t *wiimote, void *dst, u16 address, u16 size)
{
	if (address + size > sizeof(wiimote->ir_regs))
		return false;

	/* Copy the requested data from the IR camera registers */
	memcpy(dst, (u8 *)&wiimote->ir_regs + address, size);

	return true;
}

static inline bool ir_camera_write_data(fake_wiimote_t *wiimote, const void *src, u16 address, u16 size)
{
	if (address + size > sizeof(wiimote->ir_regs))
		return false;

	/* Copy the requested data to the IR camera registers */
	memcpy((u8 *)&wiimote->ir_regs + address, src, size);

	return true;
}

static bool extension_read_data(fake_wiimote_t *wiimote, void *dst, u16 address, u16 size)
{
	if (address + size > sizeof(wiimote->extension_regs))
		return false;

	/* Copy the requested data from the extension registers */
	memcpy(dst, (u8 *)&wiimote->extension_regs + address, size);

	/* Encrypt data read from extension registers (if necessary) */
	if (wiimote->extension_regs.encryption == ENCRYPTION_ENABLED) {
		if (wiimote->extension_key_dirty) {
			wiimote_crypto_generate_key_from_extension_key_data(&wiimote->extension_key,
						wiimote->extension_regs.encryption_key_data);
			wiimote->extension_key_dirty = false;
		}
		wiimote_crypto_encrypt(dst, &wiimote->extension_key, address, size);
	}

	return true;
}

static bool extension_write_data(fake_wiimote_t *wiimote, const void *src, u16 address, u16 size)
{
	if (address + size > sizeof(wiimote->extension_regs))
		return false;

	if ((address + size > ENCRYPTION_KEY_DATA_BEGIN) && (address < ENCRYPTION_KEY_DATA_END)) {
		/* We just run the key generation on all writes to the key area */
		wiimote->extension_key_dirty = true;
	}

	/* Copy the requested data to the extension registers */
	memcpy((u8 *)&wiimote->extension_regs + address, src, size);
	return true;
}

static bool fake_wiimote_process_read_request(fake_wiimote_t *wiimote)
{
	struct wiimote_input_report_read_data_t reply;
	u8 error = ERROR_CODE_SUCCESS;
	u16 address, read_size = MIN2(16, wiimote->read_request.size);

	if (read_size == 0)
		return false;

	address = wiimote->read_request.address;
	memset(&reply.data, 0, sizeof(reply.data));

	switch (wiimote->read_request.space) {
	case ADDRESS_SPACE_EEPROM:
		if (address + wiimote->read_request.size > EEPROM_FREE_SIZE)
			error = ERROR_CODE_INVALID_ADDRESS;
		else
			memcpy(reply.data, &wiimote->eeprom.data[address], read_size);
		break;
	case ADDRESS_SPACE_I2C_BUS:
	case ADDRESS_SPACE_I2C_BUS_ALT:
		/* Attempting to access the EEPROM directly over i2c results in error 8 */
		if (wiimote->read_request.slave_address == EEPROM_I2C_ADDR) {
			error = ERROR_CODE_INVALID_ADDRESS;
		} else if (wiimote->read_request.slave_address == EXTENSION_I2C_ADDR) {
			if (!extension_read_data(wiimote, reply.data, address, read_size))
				error = ERROR_CODE_NACK;
		} else if (wiimote->read_request.slave_address == CAMERA_I2C_ADDR) {
			if (!ir_camera_read_data(wiimote, reply.data, address, read_size))
				error = ERROR_CODE_NACK;
		}
		break;
	default:
		error = ERROR_CODE_INVALID_SPACE;
		break;
	}

	/* Stop processing request on read error */
	if (error != ERROR_CODE_SUCCESS) {
		wiimote->read_request.size = 0;
		/* Real wiimote seems to set size to max value on read errors */
		read_size = 16;
	} else {
		wiimote->read_request.address += read_size;
		wiimote->read_request.size -= read_size;
	}

	reply.buttons = wiimote->buttons;
	reply.size_minus_one = read_size - 1;
	reply.error = error;
	reply.address = address;
	send_hid_input_report(wiimote->hci_con_handle, wiimote->psm_hid_intr_chn.remote_cid,
			      INPUT_REPORT_ID_READ_DATA_REPLY, &reply, sizeof(reply));
	return true;
}

static void fake_wiimote_process_write_request(fake_wiimote_t *wiimote,
					       struct wiimote_output_report_write_data_t *write)
{
	u8 error = ERROR_CODE_SUCCESS;

	if (write->size == 0 || write->size > 16) {
		/* A real wiimote silently ignores such a request */
		return;
	}

	switch (write->space) {
	case ADDRESS_SPACE_EEPROM:
		if (write->address + write->size > EEPROM_FREE_SIZE)
			error = ERROR_CODE_INVALID_ADDRESS;
		else
			memcpy(&wiimote->eeprom.data[write->address], write->data, write->size);
		break;
	case ADDRESS_SPACE_I2C_BUS:
	case ADDRESS_SPACE_I2C_BUS_ALT:
		/* Attempting to access the EEPROM directly over i2c results in error 8 */
		if (write->slave_address == EEPROM_I2C_ADDR) {
			error = ERROR_CODE_INVALID_ADDRESS;
		} else if (write->slave_address == EXTENSION_I2C_ADDR) {
			if (!extension_write_data(wiimote, write->data, write->address, write->size))
				error = ERROR_CODE_NACK;
		} else if (write->slave_address == CAMERA_I2C_ADDR) {
			if (!ir_camera_write_data(wiimote, write->data, write->address, write->size))
				error = ERROR_CODE_NACK;
		}
		break;
	default:
		error = ERROR_CODE_INVALID_SPACE;
		break;
	}

	/* Real wiimotes seem to always ACK data writes */
	wiimote_send_ack(wiimote, OUTPUT_REPORT_ID_WRITE_DATA, error);
}

static inline bool fake_wiimote_process_extension_change(fake_wiimote_t *wiimote)
{
	const u8 *id_code = NULL;

	if (wiimote->new_extension == wiimote->cur_extension)
		return false;

	switch (wiimote->new_extension) {
	case WIIMOTE_MGR_EXT_NUNCHUK:
		id_code = EXT_ID_CODE_NUNCHUNK;
		break;
	case WIIMOTE_MGR_EXT_CLASSIC:
		id_code = EXP_ID_CODE_CLASSIC_CONTROLLER;
		break;
	case WIIMOTE_MGR_EXT_CLASSIC_WIIU_PRO:
		id_code = EXP_ID_CODE_CLASSIC_WIIU_PRO;
		break;
	case WIIMOTE_MGR_EXT_GUITAR:
		id_code = EXP_ID_CODE_GUITAR;
		break;
	case WIIMOTE_MGR_EXT_MOTION_PLUS:
		id_code = EXP_ID_CODE_MOTION_PLUS;
		break;
	default:
		break;
	}

	if (id_code) {
		memcpy(wiimote->extension_regs.identifier, id_code,
		       sizeof(wiimote->extension_regs.identifier));
	}
	wiimote->cur_extension = wiimote->new_extension;

	/* Following a connection or disconnection event on the Extension Port, data reporting
	 * is disabled and the Data Reporting Mode must be reset before new data can arrive */
	wiimote->reporting_mode = INPUT_REPORT_ID_REPORT_DISABLED;
	wiimote_send_input_report_status(wiimote);

	return true;
}

static void fake_wiimote_send_data_report(fake_wiimote_t *wiimote)
{
	u8 report_data[CONTROLLER_DATA_BYTES] ATTRIBUTE_ALIGN(4);
	u16 buttons;
	bool has_btn;
	u16 acc_x, acc_y, acc_z;
	u8 acc_size, acc_offset;
	u8 ext_size, ext_offset;
	u8 ir_size, ir_offset;
	u8 report_size;

	if (wiimote->reporting_mode == INPUT_REPORT_ID_REPORT_DISABLED) {
		/* The wiimote is in this disabled state after an extension change.
		   Input reports are not sent, even on button change. */
		return;
	}

	if (wiimote->reporting_continuous || wiimote->input_dirty) {
		buttons = wiimote->buttons;
		has_btn = input_report_has_btn(wiimote->reporting_mode);
		acc_size = input_report_acc_size(wiimote->reporting_mode);
		acc_offset = input_report_acc_offset(wiimote->reporting_mode);
		ext_size = input_report_ext_size(wiimote->reporting_mode);
		ext_offset = input_report_ext_offset(wiimote->reporting_mode);
		ir_size = input_report_ir_size(wiimote->reporting_mode);
		ir_offset = input_report_ir_offset(wiimote->reporting_mode);
		report_size = (has_btn ? 2 : 0) + acc_size + ext_size + ir_size;

		if (acc_size) {
			/* TODO: Use "real" accelerometer data */
			acc_x = ACCEL_ZERO_G << 2;
			acc_y = ACCEL_ZERO_G << 2;
			acc_z = ACCEL_ONE_G << 2;

			report_data[acc_offset + 0] = (acc_x >> 2) & 0xFF;
			report_data[acc_offset + 1] = (acc_y >> 2) & 0xFF;
			report_data[acc_offset + 2] = (acc_z >> 2) & 0xFF;
			buttons |= ((acc_x & 3) << 5) | ((acc_y & 2) << 12) | ((acc_z & 2) << 13);
		}

		if (ir_size)
			memcpy(&report_data[ir_offset], wiimote->ir_regs.camera_data, ir_size);

		if (ext_size) {
			/* Takes care of encrypting the extension data if necessary */
			extension_read_data(wiimote, report_data + ext_offset, 0, ext_size);
		}

		if (has_btn)
			memcpy(report_data, &buttons, sizeof(buttons));

		send_hid_input_report(wiimote->hci_con_handle, wiimote->psm_hid_intr_chn.remote_cid,
				      wiimote->reporting_mode, report_data, report_size);

		wiimote->input_dirty = false;
	}
}

static inline void fake_wiimote_update_rumble(fake_wiimote_t *wiimote, bool rumble_on)
{
	if (rumble_on != wiimote->rumble_on) {
		wiimote->rumble_on = rumble_on;
		if (wiimote->input_device_ops->set_rumble)
			wiimote->input_device_ops->set_rumble(wiimote->usrdata, rumble_on);
	}
}

static void fake_wiimote_tick(fake_wiimote_t *wiimote)
{
	int ret;
	bool req;

	if (wiimote->baseband_state == BASEBAND_STATE_REQUEST_CONNECTION) {
		req = hci_request_connection(&wiimote->bdaddr, WIIMOTE_HCI_CLASS_0,
					     WIIMOTE_HCI_CLASS_1, WIIMOTE_HCI_CLASS_2,
					     HCI_LINK_ACL);
		/* After a connection request is visible to the controller switch to inactive */
		if (req)
			wiimote->baseband_state = BASEBAND_STATE_INACTIVE;
	} else if (wiimote->baseband_state == BASEBAND_STATE_COMPLETE) {
		/* "If the connection originated from the device (Wiimote) it will create
		 * HID control and interrupt channels (in that order)." */
		if (wiimote->acl_state == ACL_STATE_LINKING) {
			/* If-else-if cascade to avoid sending too many packets on the same "tick" */
			if (!wiimote->psm_hid_cntl_chn.valid) {
				u16 local_cid = generate_l2cap_channel_id();
				ret = l2cap_send_connect_req(wiimote->hci_con_handle, L2CAP_PSM_HID_CNTL,
							     local_cid);
				assert(ret == IOS_OK);
				l2cap_channel_info_setup(&wiimote->psm_hid_cntl_chn, L2CAP_PSM_HID_CNTL, local_cid);
				DEBUG("Generated local CID for HID CNTL: 0x%x\n", local_cid);
			} else if (!wiimote->psm_hid_intr_chn.valid) {
				u16 local_cid = generate_l2cap_channel_id();
				ret = l2cap_send_connect_req(wiimote->hci_con_handle, L2CAP_PSM_HID_INTR,
							     local_cid);
				assert(ret == IOS_OK);
				l2cap_channel_info_setup(&wiimote->psm_hid_intr_chn, L2CAP_PSM_HID_INTR, local_cid);
				DEBUG("Generated local CID for HID INTR: 0x%x\n", local_cid);
			} else if (l2cap_channel_is_complete(&wiimote->psm_hid_cntl_chn) &&
				   l2cap_channel_is_complete(&wiimote->psm_hid_intr_chn)) {
				wiimote->acl_state = ACL_STATE_INACTIVE;
				/* Call assigned() input_device callback */
				wiimote->input_device_ops->assigned(wiimote->usrdata, wiimote);
			}
			/* Send configuration for any newly connected channels. */
			check_send_config_for_new_channel(wiimote->hci_con_handle, &wiimote->psm_hid_cntl_chn);
			check_send_config_for_new_channel(wiimote->hci_con_handle, &wiimote->psm_hid_intr_chn);
		} else {
			/* Both HID ctrl and intr channels are connected (we only need intr though) */
			if (fake_wiimote_process_read_request(wiimote)) {
				/* Read requests suppress normal input reports.
				 * Don't send any other reports */
				return;
			}

			if (fake_wiimote_process_extension_change(wiimote)) {
				/* Extension port event occurred. Don't send any other reports. */
				return;
			}

			fake_wiimote_send_data_report(wiimote);
		}
	}
}

void fake_wiimote_mgr_tick_devices(void)
{
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (fake_wiimotes[i].active)
			fake_wiimote_tick(&fake_wiimotes[i]);
	}
}

/* Functions called by the HCI state manager */

bool fake_wiimote_mgr_handle_hci_cmd_accept_con(const bdaddr_t *bdaddr, u8 role)
{
	int ret;

	/* Check if the bdaddr belongs to a fake wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (memcmp(bdaddr, &fake_wiimotes[i].bdaddr, sizeof(bdaddr_t) != 0))
			continue;

		/* Connection accepted to our fake wiimote */
		DEBUG("Connection accepted for fake Wiimote %d!\n", i);

		/* The Accept_Connection_Request command will cause the Command Status
		   event to be sent from the Host Controller when the Host Controller
		   begins setting up the connection */

		ret = enqueue_hci_event_command_status(HCI_CMD_ACCEPT_CON);
		assert(ret == IOS_OK);

		fake_wiimotes[i].baseband_state = BASEBAND_STATE_COMPLETE;
		fake_wiimotes[i].hci_con_handle = hci_con_handle_virt_alloc();
		DEBUG("Fake Wiimote %d got HCI con_handle: 0x%x\n", i, fake_wiimotes[i].hci_con_handle);

		/* We can start the ACL (L2CAP) linking now */
		fake_wiimotes[i].acl_state = ACL_STATE_LINKING;

		if (role == HCI_ROLE_MASTER) {
			ret = enqueue_hci_event_role_change(bdaddr, HCI_ROLE_MASTER);
			assert(ret == IOS_OK);
		}

		/* In addition, when the Link Manager determines the connection is established,
		 * the Host Controllers on both Bluetooth devices that form the connection
		 * will send a Connection Complete event to each Host */
		ret = enqueue_hci_event_con_compl(bdaddr, fake_wiimotes[i].hci_con_handle, 0);
		assert(ret == IOS_OK);

		DEBUG("Connection complete sent, starting ACL linking!\n");
		return true;
	}

	return false;
}

bool fake_wiimote_mgr_handle_hci_cmd_disconnect(u16 hci_con_handle, u8 reason)
{
	/* Check if the HCI connection handle belongs to a fake wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (!fake_wiimote_is_connected(&fake_wiimotes[i]) ||
		    (fake_wiimotes[i].hci_con_handle != hci_con_handle))
			continue;

		/* Host wants disconnection to our fake wiimote. Disconnect */
		DEBUG("Host requested disconnection of fake Wiimote %d.\n", i);
		fake_wiimote_disconnect(&fake_wiimotes[i]);
		return true;
	}

	return false;
}

bool fake_wiimote_mgr_handle_hci_cmd_reject_con(const bdaddr_t *bdaddr, u8 reason)
{
	/* Check if the bdaddr belongs to a fake wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (memcmp(bdaddr, &fake_wiimotes[i].bdaddr, sizeof(bdaddr_t) != 0))
			continue;

		/* Connection rejected to our fake wiimote. Disconnect */
		DEBUG("Connection to fake Wiimote %d rejected!\n", i);
		fake_wiimote_disconnect(&fake_wiimotes[i]);
		return true;
	}

	return false;
}

bool fake_wiimote_mgr_hci_handle_belongs_to_fake_wiimote(u16 hci_con_handle)
{
	/* Check if the HCI connection handle belongs to a fake wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (fake_wiimote_is_connected(&fake_wiimotes[i]) &&
		    (fake_wiimotes[i].hci_con_handle == hci_con_handle))
		return true;
	}

	return false;
}

static void handle_l2cap_config_req(fake_wiimote_t *wiimote, u8 ident, u16 dcid, u16 flags,
				    const u8 *options, u16 options_size)
{
	u8 tmp[256];
	u32 opt_size;
	l2cap_channel_info_t *info;
	l2cap_cfg_opt_t *opt;
	l2cap_cfg_opt_val_t *val;
	l2cap_cfg_rsp_cp *rsp = (l2cap_cfg_rsp_cp *)tmp;
	u32 offset = 0;
	u32 resp_len = 0;
	/* If the option is not provided, configure the default. */
	u16 remote_mtu = L2CAP_MTU_DEFAULT;

	assert(flags == 0x00);
	assert(options_size <= sizeof(tmp));

	info = get_channel_info(wiimote, dcid);
	assert(info);

	/* Response to the config request */
	rsp->scid = htole16(info->remote_cid);
	rsp->flags = htole16(0x00);
	rsp->result = htole16(L2CAP_SUCCESS);
	resp_len += sizeof(l2cap_cfg_rsp_cp);

	/* Read configuration options. */
	while (offset < options_size) {
		opt = (l2cap_cfg_opt_t *)&options[offset];
		offset += sizeof(l2cap_cfg_opt_t);
		val = (l2cap_cfg_opt_val_t *)&options[offset];

		switch (opt->type) {
		case L2CAP_OPT_MTU:
			assert(opt->length == L2CAP_OPT_MTU_SIZE);
			remote_mtu = le16toh(val->mtu);
			DEBUG("      MTU configured to: 0x%x\n", remote_mtu);
			break;
		/* We don't care what the flush timeout is. Our packets are not dropped. */
		case L2CAP_OPT_FLUSH_TIMO:
			assert(opt->length == L2CAP_OPT_FLUSH_TIMO_SIZE);
			DEBUG("      Flush timeout configured to 0x%x\n", val->flush_timo);
			break;
		default:
			DEBUG("      Unknown Option: 0x%02x", opt->type);
			break;
		}

		offset += opt->length;
		opt_size = sizeof(l2cap_cfg_opt_t) + opt->length;
		memcpy(&tmp[resp_len], options, opt_size);
		resp_len += opt_size;
	}

	/* Send Respone */
	l2cap_send_config_rsp(wiimote->hci_con_handle, dcid, ident, tmp, resp_len);

	/* Set the MTU */
	info->remote_mtu = remote_mtu;
}

static void handle_l2cap_signal_channel(fake_wiimote_t *wiimote, u8 code, u8 ident,
					const void *payload, u16 size)
{
	l2cap_channel_info_t *info;

	DEBUG("  signal channel: code: 0x%x, ident: 0x%x\n", code, ident);

	switch (code) {
	case L2CAP_CONNECT_REQ: {
		const l2cap_con_req_cp *req = payload;
		u16 psm = le16toh(req->psm);
		u16 scid = le16toh(req->scid);
		UNUSED(psm);
		UNUSED(scid);
		DEBUG("  L2CAP_CONNECT_REQ: psm: 0x%x, scid: 0x%x\n", psm, scid);
		/* TODO */
		break;
	}
	case L2CAP_CONNECT_RSP: {
		const l2cap_con_rsp_cp *rsp = payload;
		u16 dcid = le16toh(rsp->dcid);
		u16 scid = le16toh(rsp->scid);
		u16 result = le16toh(rsp->result);
		u16 status = le16toh(rsp->status);
		DEBUG("  L2CAP_CONNECT_RSP: dcid: 0x%x, scid: 0x%x, result: 0x%x, status: 0x%x\n",
			dcid, scid, result, status);

		/* libogc/master/lwbt/l2cap.c#L318 sets it to the "dcid" and not to
		 * the "result" field (and scid to 0)... */
		if ((result != L2CAP_SUCCESS) || ((dcid == L2CAP_PSM_NOT_SUPPORTED) &&
						  (scid == 0))) {
			fake_wiimote_disconnect(wiimote);
			break;
		}

		assert(status == L2CAP_NO_INFO);
		info = get_channel_info(wiimote, scid);
		assert(info);

		/* Save endpoint's Destination CID  */
		info->remote_cid = dcid;
		break;
	}
	case L2CAP_CONFIG_REQ: {
		const l2cap_cfg_req_cp *rsp = payload;
		u16 dcid = le16toh(rsp->dcid);
		u16 flags = le16toh(rsp->flags);
		const void *options = (const void *)((u8 *)rsp + sizeof(l2cap_cfg_req_cp));
		UNUSED(flags);

		DEBUG("  L2CAP_CONFIG_REQ: dcid: 0x%x, flags: 0x%x\n", dcid, flags);
		handle_l2cap_config_req(wiimote, ident, dcid, flags, options,
					size - sizeof(l2cap_cfg_req_cp));
		break;
	}
	case L2CAP_CONFIG_RSP: {
		const l2cap_cfg_rsp_cp *rsp = payload;
		u16 scid = le16toh(rsp->scid);
		u16 flags = le16toh(rsp->flags);
		u16 result = le16toh(rsp->result);
		UNUSED(flags);
		DEBUG("  L2CAP_CONFIG_RSP: scid: 0x%x, flags: 0x%x, result: 0x%x\n",
			scid, flags, result);

		assert(result == L2CAP_SUCCESS);
		info = get_channel_info(wiimote, scid);
		assert(info);

		/* Mark channel as complete!  */
		info->state = L2CAP_CHANNEL_STATE_INACTIVE_COMPLETE;
		break;
	}
	case L2CAP_DISCONNECT_REQ: {
		const l2cap_discon_req_cp *req = payload;
		u16 dcid = le16toh(req->dcid);
		u16 scid = le16toh(req->scid);
		DEBUG("  L2CAP_DISCONNECT_REQ: dcid: 0x%x, scid: 0x%x\n", dcid, scid);

		info = get_channel_info(wiimote, scid);
		assert(info);

		/* If it's the L2CAP Interrupt channel connection, notify the driver of disconnection */
		if ((info->psm == L2CAP_PSM_HID_INTR) && l2cap_channel_is_complete(info)) {
			if (wiimote->input_device_ops->disconnect)
				wiimote->input_device_ops->disconnect(wiimote->usrdata);
			info->valid = false;
		}

		/* Send disconnect response */
		l2cap_send_disconnect_rsp(wiimote->hci_con_handle, ident, dcid, scid);
		break;
	}
	}
}

static void handle_l2cap_signal_channel_request(fake_wiimote_t *wiimote, const void *data, u16 length)
{
	const l2cap_cmd_hdr_t *cmd_hdr;
	const void *cmd_payload;
	u16 cmd_len;

	while (length >= sizeof(l2cap_cmd_hdr_t)) {
		cmd_hdr = (const void *)data;
		cmd_len = le16toh(cmd_hdr->length);
		cmd_payload = (const void *)((u8 *)data + sizeof(*cmd_hdr));

		handle_l2cap_signal_channel(wiimote, cmd_hdr->code, cmd_hdr->ident,
					    cmd_payload, cmd_len);

		data += sizeof(l2cap_cmd_hdr_t) + cmd_len;
		length -= sizeof(l2cap_cmd_hdr_t) + cmd_len;
	}
}

static void handle_hid_intr_data_output(fake_wiimote_t *wiimote, const u8 *data, u16 size)
{
	DEBUG("handle_hid_intr_data_output: size: 0x%x, 0x%x\n", size, *(u32 *)(data-1));

	if (size == 0)
		return;

	/* Setting the LSB (bit 0) of the first byte of any output report
	 * will activate the rumble motor, and unsetting it will deactivate it */
	fake_wiimote_update_rumble(wiimote, data[1] & 1);

	switch (data[0]) {
	case OUTPUT_REPORT_ID_LED: {
		struct wiimote_output_report_led_t *led = (void *)&data[1];
		wiimote->status.leds = led->leds;
		/* Call set_leds() input_device callback */
		if (wiimote->input_device_ops->set_leds)
			wiimote->input_device_ops->set_leds(wiimote->usrdata, led->leds);
		if (led->ack)
			wiimote_send_ack(wiimote, OUTPUT_REPORT_ID_LED, ERROR_CODE_SUCCESS);
		break;
	}
	case OUTPUT_REPORT_ID_REPORT_MODE: {
		struct wiimote_output_report_mode_t *mode = (void *)&data[1];
		DEBUG("  Report mode: 0x%02x, cont: %d, rumble: %d, ack: %d\n",
			mode->mode, mode->continuous, mode->rumble, mode->ack);
		wiimote->reporting_mode = mode->mode;
		wiimote->reporting_continuous = mode->continuous;
		if (mode->ack)
			wiimote_send_ack(wiimote, OUTPUT_REPORT_ID_REPORT_MODE, ERROR_CODE_SUCCESS);
		break;
	}
	case OUTPUT_REPORT_ID_IR_ENABLE: {
		struct wiimote_output_report_enable_feature_t *feature = (void *)&data[1];
		wiimote->status.ir = feature->enable;
		/* TODO: Enable/disable "camera" logic */
		if (feature->ack)
			wiimote_send_ack(wiimote, OUTPUT_REPORT_ID_IR_ENABLE, ERROR_CODE_SUCCESS);
		break;
	}
	case OUTPUT_REPORT_ID_STATUS: {
		wiimote_send_input_report_status(wiimote);
		break;
	}
	case OUTPUT_REPORT_ID_WRITE_DATA: {
		struct wiimote_output_report_write_data_t *write = (void *)&data[1];
		DEBUG("  Write data to slave 0x%02x, address: 0x%x, size: 0x%x 0x%x\n",
			write->slave_address, write->address, write->size, write->data[0]);

		fake_wiimote_process_write_request(wiimote, write);
		break;
	}
	case OUTPUT_REPORT_ID_READ_DATA: {
		struct wiimote_output_report_read_data_t *read = (void *)&data[1];
		DEBUG("  Read data from slave 0x%02x, addrspace: %d, address: 0x%x, size: 0x%x\n\n",
			read->slave_address, read->space, read->address, read->size);

		/* There is already an active read being processed */
		if (wiimote->read_request.size) {
			wiimote_send_ack(wiimote, OUTPUT_REPORT_ID_READ_DATA, ERROR_CODE_BUSY);
			break;
		}

		 /* Save the request and process it on the next "tick()" call(s) */
		wiimote->read_request.space = read->space;
		wiimote->read_request.slave_address = read->slave_address;
		wiimote->read_request.address = read->address;
		/* A zero size request is just ignored, like on the real wiimote */
		wiimote->read_request.size = read->size;

		/* Send first "read-data reply". If more data needs to be sent,
		 * it will happen on the next "tick()" */
		fake_wiimote_process_read_request(wiimote);
	}
	case OUTPUT_REPORT_ID_IR_ENABLE2: {
		struct wiimote_output_report_enable_feature_t *feature = (void *)&data[1];
		if (feature->ack)
			wiimote_send_ack(wiimote, OUTPUT_REPORT_ID_IR_ENABLE2, ERROR_CODE_SUCCESS);
		break;
	}
	default:
		DEBUG("Unhandled output report: 0x%x\n", data[0]);
		break;
	}
}

bool fake_wiimote_mgr_handle_acl_data_out_request_from_host(u16 hci_con_handle, const hci_acldata_hdr_t *acl)
{
	const l2cap_hdr_t *header;
	u16 dcid, length;
	const u8 *payload;

	/* Check if the HCI connection handle belongs to a fake wiimote */
	for (int i = 0; i < MAX_FAKE_WIIMOTES; i++) {
		if (!fake_wiimote_is_connected(&fake_wiimotes[i]) ||
		    (fake_wiimotes[i].hci_con_handle != hci_con_handle))
			continue;

		/* L2CAP header */
		header  = (const void *)((u8 *)acl + sizeof(hci_acldata_hdr_t));
		length  = le16toh(header->length);
		dcid    = le16toh(header->dcid);
		payload = (u8 *)header + sizeof(l2cap_hdr_t);

		DEBUG("FD ACL OUT: con_handle: 0x%x, dcid: 0x%x, len: 0x%x\n", hci_con_handle, dcid, length);

		if (dcid == L2CAP_SIGNAL_CID) {
			handle_l2cap_signal_channel_request(&fake_wiimotes[i], payload, length);
		} else {
			l2cap_channel_info_t *info = get_channel_info(&fake_wiimotes[i], dcid);
			if (info) {
				switch (info->psm) {
				case L2CAP_PSM_SDP:
					/* TODO */
					DEBUG("  PSM HID SDP\n");
					break;
				case L2CAP_PSM_HID_CNTL:
					/* TODO */
					DEBUG("  PSM HID CNTL\n");
					break;
				case L2CAP_PSM_HID_INTR:
					if (payload[0] == ((HID_TYPE_DATA << 4) | HID_PARAM_OUTPUT))
						handle_hid_intr_data_output(&fake_wiimotes[i],
									    &payload[1],
									    length - 1);
					break;
				}
			} else {
				DEBUG("Received L2CAP packet to unknown channel: 0x%x\n", dcid);
			}
		}
		return true;
	}

	return false;
}
