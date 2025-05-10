#include "client.h"

#include "internals.h"

bool _usb_hid_sensor_bar_position_top = false;

void ogc_usb_hid_set_sensor_bar_position_top(bool on_top)
{
    _usb_hid_sensor_bar_position_top = on_top;
}
