#ifndef HCI_STATE_H
#define HCI_STATE_H

#include <stdbool.h>

void hci_state_init(void);

/* Used by fakedev */
u16 hci_con_handle_virt_alloc(void);
bool hci_request_connection(const bdaddr_t *bdaddr, u8 uclass0, u8 uclass1, u8 uclass2,
			    u8 link_type);


/* Used by the main request-handling loop */
int hci_state_handle_hci_command(void *data, u32 length, int *fwd_to_usb);
void hci_state_handle_hci_event(void *data, u32 length);
int hci_state_handle_acl_data_out(void *data, u32 size, int *fwd_to_usb);
void hci_state_handle_acl_data_in(void *data, u32 length);

#endif
