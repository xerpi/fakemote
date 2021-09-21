#ifndef HCI_STATE_H
#define HCI_STATE_H


void hci_state_init(void);
int hci_state_handle_hci_command(void *data, u32 length, int *fwd_to_usb);
void hci_state_handle_hci_event(void *data, u32 length);
int hci_state_handle_acl_data_out(void *data, u32 size, int *fwd_to_usb);
void hci_state_handle_acl_data_in(void *data, u32 length);

#endif
