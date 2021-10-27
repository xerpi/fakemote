#ifndef BT_DEVICE_MGR_H
#define BT_DEVICE_MGR_H

#include "types.h"

bool bt_device_mgr_handle_acl_data_in_response_from_controller(u16 hci_con_handle, const void *data);

#endif
