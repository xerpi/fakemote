#ifndef UTILS_H
#define UTILS_H

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hci.h"

#define le16toh(x) __builtin_bswap16(x)
#define htole16(x) __builtin_bswap16(x)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define MIN2(x, y)	(((x) < (y)) ? (x) : (y))
#define MAX2(x, y)	(((x) > (y)) ? (x) : (y))
#define ROUNDUP32(x)	(((u32)(x) + 0x1f) & ~0x1f)
#define ROUNDDOWN32(x)	(((u32)(x) - 0x1f) & ~0x1f)

#define UNUSED(x) (void)(x)
#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

#define STRINGIFY(x)	#x
#define TOSTRING(x)	STRINGIFY(x)

#ifdef assert
#undef assert
#endif
#define assert(exp) ( (exp) ? (void)0 : my_assert_func(__FILE__, __LINE__, __FUNCTION__, #exp))

//#define DEBUG(...) printf(__VA_ARGS__)
#define DEBUG(...) (void)0

extern void my_assert_func(const char *file, int line, const char *func, const char *failedexpr);

static inline int memmismatch(const void *restrict a, const void *restrict b, int size)
{
	int i = 0;
	while (size) {
		if (((u8 *)a)[i] != ((u8 *)b)[i])
			return i;
		i++;
		size--;
	}
	return i;
}

/* HCI event enqueue helpers */
int enqueue_hci_event_command_status(u16 opcode);
int enqueue_hci_event_command_compl(u16 opcode, const void *payload, u32 payload_size);
int enqueue_hci_event_con_req(const bdaddr_t *bdaddr, u8 uclass0, u8 uclass1, u8 uclass2, u8 link_type);
int enqueue_hci_event_discon_compl(u16 con_handle, u8 status, u8 reason);
int enqueue_hci_event_con_compl(const bdaddr_t *bdaddr, u16 con_handle, u8 status);
int enqueue_hci_event_role_change(const bdaddr_t *bdaddr, u8 role);
int enqueue_hci_event_mode_change(u16 con_handle, u8 unit_mode, u16 interval);
int enqueue_hci_event_num_compl_pkts(u8 num_con_handles, const u16 *con_handles, const u16 *compl_pkts);
int enqueue_hci_event_return_link_keys(u8 num_keys, const bdaddr_t *bdaddr, const u8 key[][HCI_KEY_SIZE]);
int enqueue_hci_event_con_pkt_type_changed(u16 con_handle, u16 pkt_type);
int enqueue_hci_event_auth_compl(u8 status, u16 con_handle);
int enqueue_hci_event_remote_name_req_compl(u8 status, const bdaddr_t *bdaddr, const char *name);
int enqueue_hci_event_read_remote_features_compl(u16 con_handle, const u8 features[static HCI_FEATURES_SIZE]);
int enqueue_hci_event_read_remote_ver_info_compl(u16 con_handle, u8 lmp_version, u16 manufacturer,
						 u16 lmp_subversion);
int enqueue_hci_event_read_clock_offset_compl(u16 con_handle, u16 clock_offset);

/* L2CAP event enqueue helpers */
int l2cap_send_msg(u16 hci_con_handle, u16 dcid, const void *data, u16 size);
int l2cap_send_connect_req(u16 hci_con_handle, u16 psm, u16 scid);
int l2cap_send_disconnect_req(u16 hci_con_handle, u16 dcid, u16 scid);
int l2cap_send_disconnect_rsp(u16 hci_con_handle, u8 ident, u16 dcid, u16 scid);
int l2cap_send_config_req(u16 hci_con_handle, u16 remote_cid, u16 mtu, u16 flush_time_out);
int l2cap_send_config_rsp(u16 hci_con_handle, u16 remote_cid, u8 ident, const u8 *options, u32 options_len);

#endif
