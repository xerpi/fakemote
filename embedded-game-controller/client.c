#include "client.h"

#include "internals.h"

bool _egc_sensor_bar_position_top = false;

void egc_set_sensor_bar_position_top(bool on_top)
{
    _egc_sensor_bar_position_top = on_top;
}
