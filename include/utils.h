#ifndef UTILS_H
#define UTILS_H

#include <assert.h>
#include <stdio.h>
#include "hci.h"

#define bswap16 __builtin_bswap16
#define le16toh bswap16
#define htole16 bswap16

#define MAC_STR_LEN 18

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define MIN2(x, y)	(((x) < (y)) ? (x) : (y))
#define ROUNDUP32(x)	(((u32)(x) + 0x1f) & ~0x1f)
#define ROUNDDOWN32(x)	(((u32)(x) - 0x1f) & ~0x1f)

#define UNUSED(x) (void)(x)

#ifdef assert
#undef assert
#endif
#define assert(exp) ( (exp) ? (void)0 : my_assert_func(__FILE__, __LINE__, __FUNCTION__, #exp))

//#define DEBUG(...) printf(__VA_ARGS__)
#define DEBUG(...) (void)0

extern void my_assert_func(const char *file, int line, const char *func, const char *failedexpr);

static inline void bdaddr_to_str(char str[static MAC_STR_LEN], const bdaddr_t *bdaddr)
{
	snprintf(str, MAC_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x", bdaddr->b[5], bdaddr->b[4],
		bdaddr->b[3], bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);
}

/* HCI event enqueue helpers */
int enqueue_hci_event_command_status(u16 opcode);
int enqueue_hci_event_command_compl(u16 opcode, const void *payload, u32 payload_size);
int enqueue_hci_event_con_req(const bdaddr_t *bdaddr, u8 uclass0, u8 uclass1, u8 uclass2, u8 link_type);
int enqueue_hci_event_discon_compl(u16 con_handle, u8 status, u8 reason);
int enqueue_hci_event_con_compl(const bdaddr_t *bdaddr, u16 con_handle, u8 status);
int enqueue_hci_event_role_change(const bdaddr_t *bdaddr, u8 role);

/* L2CAP event enqueue helpers */
int l2cap_send_msg(u16 hci_con_handle, u16 dcid, const void *data, u16 size);
int l2cap_send_connect_req(u16 hci_con_handle, u16 psm, u16 scid);
int l2cap_send_disconnect_req(u16 hci_con_handle, u16 dcid, u16 scid);
int l2cap_send_disconnect_rsp(u16 hci_con_handle, u8 ident, u16 dcid, u16 scid);
int l2cap_send_config_req(u16 hci_con_handle, u16 remote_cid, u16 mtu, u16 flush_time_out);
int l2cap_send_config_rsp(u16 hci_con_handle, u16 remote_cid, u8 ident, const u8 *options, u32 options_len);

#endif
