#ifndef INPUT_DEVICE_H
#define INPUT_DEVICE_H

typedef struct fake_wiimote_t fake_wiimote_t;

typedef struct input_device_ops_t {
	int (*init)(void *usrdata, fake_wiimote_t *wiimote);
	int (*disconnect)(void *usrdata);
	int (*set_leds)(void *usrdata, int leds);
	int (*set_rumble)(void *usrdata, bool rumble_on);
	bool (*report_input)(void *usrdata);
} input_device_ops_t;

#endif
