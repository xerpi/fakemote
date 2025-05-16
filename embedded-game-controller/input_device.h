#ifndef INPUT_DEVICE_H
#define INPUT_DEVICE_H

#include <stdbool.h>

typedef struct fake_wiimote_t fake_wiimote_t;
typedef struct input_device_t input_device_t;

typedef struct input_device_ops_t {
    int (*resume)(void *usrdata, fake_wiimote_t *wiimote);
    int (*suspend)(void *usrdata);
    int (*set_leds)(void *usrdata, int leds);
    int (*set_rumble)(void *usrdata, bool rumble_on);
    bool (*report_input)(void *usrdata);
} input_device_ops_t;

void input_devices_init(void);
void input_devices_tick(void);

/** Used by input devices **/

bool input_devices_add(void *usrdata, const input_device_ops_t *ops,
                       input_device_t **assigned_input_device);
void input_devices_remove(input_device_t *input_device);

/** Used by fake Wiimotes and fake Wiimote manager **/

input_device_t *input_device_get_unassigned(void);

void input_device_assign_wiimote(input_device_t *input_device, fake_wiimote_t *wiimote);
void input_device_release_wiimote(input_device_t *input_device);

int input_device_resume(input_device_t *input_device);
int input_device_suspend(input_device_t *input_device);
int input_device_set_leds(input_device_t *input_device, int leds);
int input_device_set_rumble(input_device_t *input_device, bool rumble_on);
bool input_device_report_input(input_device_t *input_device);

#endif
