#ifndef INPUT_DEVICE_H
#define INPUT_DEVICE_H

typedef struct fake_wiimote_t fake_wiimote_t;

typedef struct input_device_ops_t {
	int (*assigned)(void *usrdata, fake_wiimote_t *wiimote);
	int (*disconnect)(void *usrdata);
	int (*set_leds)(void *usrdata, int leds);
} input_device_ops_t;

#endif
