#include "fake_wiimote.h"
#include "input_device.h"
#include "utils.h"
#include "types.h"

#define MAX_INPUT_DEVS	2
#define RECONNECT_DELAY	200 /* 1s @ 200Hz */

static struct input_device_t {
	bool valid;
	/* NULL if no assigned fake Wiimote */
	fake_wiimote_t *assigned_wiimote;
	const input_device_ops_t *ops;
	void *usrdata;
	u32 reconnect_delay;
} input_devices[MAX_INPUT_DEVS];

void input_devices_init(void)
{
	for (int i = 0; i < ARRAY_SIZE(input_devices); i++)
		input_devices[i].valid = false;
}

bool input_devices_add(void *usrdata, const input_device_ops_t *ops,
		      input_device_t **assigned_input_device)
{
	/* Find a free input device slot */
	for (int i = 0; i < ARRAY_SIZE(input_devices); i++) {
		if (!input_devices[i].valid) {
			/* No assigned fake Wiimote yet */
			input_devices[i].assigned_wiimote = NULL;
			input_devices[i].ops = ops;
			input_devices[i].usrdata = usrdata;
			input_devices[i].reconnect_delay = 0;
			input_devices[i].valid = true;
			*assigned_input_device = &input_devices[i];
			return true;
		}
	}
	return false;
}

void input_devices_remove(input_device_t *input_device)
{
	fake_wiimote_t *wiimote = input_device->assigned_wiimote;

	/* Check and disconnect if a fake wiimote is assigned to this input device */
	if (wiimote) {
		/* First unassign the input device so that disconnect() doesn't send USB requests */
		fake_wiimote_release_input_device(wiimote);
		fake_wiimote_disconnect(wiimote);
	}
	input_devices->valid = false;
}

void input_devices_tick(void)
{
	for (int i = 0; i < ARRAY_SIZE(input_devices); i++) {
		if (input_devices[i].valid && !input_devices[i].assigned_wiimote) {
			if (input_devices[i].reconnect_delay > 0)
				input_devices[i].reconnect_delay--;
		}
	}
}

input_device_t *input_device_get_unassigned(void)
{
	for (int i = 0; i < ARRAY_SIZE(input_devices); i++) {
		if (input_devices[i].valid &&
		    !input_devices[i].assigned_wiimote &&
		    input_devices[i].reconnect_delay == 0) {
			return &input_devices[i];
		}
	}

	return NULL;
}

void input_device_assign_wiimote(input_device_t *input_device, fake_wiimote_t *wiimote)
{
	input_device->assigned_wiimote = wiimote;
}

void input_device_release_wiimote(input_device_t *input_device)
{
	input_device->assigned_wiimote = NULL;
	input_device->reconnect_delay = RECONNECT_DELAY;
	input_device->ops->suspend(input_device->usrdata);
}

int input_device_resume(input_device_t *input_device)
{
	return input_device->ops->resume(input_device->usrdata, input_device->assigned_wiimote);
}

int input_device_suspend(input_device_t *input_device)
{
	return input_device->ops->suspend(input_device->usrdata);
}

int input_device_set_leds(input_device_t *input_device, int leds)
{
	return input_device->ops->set_leds(input_device->usrdata, leds);
}

int input_device_set_rumble(input_device_t *input_device, bool rumble_on)
{
	return input_device->ops->set_rumble(input_device->usrdata, rumble_on);
}

bool input_device_report_input(input_device_t *input_device)
{
	return input_device->ops->report_input(input_device->usrdata);
}
