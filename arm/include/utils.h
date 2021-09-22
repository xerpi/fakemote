#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#define bswap16 __builtin_bswap16
#define le16toh bswap16
#define htole16 bswap16

#define MAC_STR_LEN 18

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define ROUNDUP32(x)	(((u32)(x) + 0x1f) & ~0x1f)
#define ROUNDDOWN32(x)	(((u32)(x) - 0x1f) & ~0x1f)

#ifdef assert
#undef assert
#endif
#define assert(exp) ( (exp) ? (void)0 : my_assert_func(__FILE__, __LINE__, __FUNCTION__, #exp))

#define svc_printf printf

extern void my_assert_func(const char *file, int line, const char *func, const char *failedexpr);

static inline void bdaddr_to_str(char str[static MAC_STR_LEN], const bdaddr_t *bdaddr)
{
	snprintf(str, MAC_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x", bdaddr->b[5], bdaddr->b[4],
		bdaddr->b[3], bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);
}

/* HCI event enqueue helpers */
int enqueue_hci_event_con_req(const bdaddr_t *bdaddr, u8 uclass0, u8 uclass1, u8 uclass2, u8 link_type);
int enqueue_hci_event_command_status(u16 opcode);
int enqueue_hci_event_con_compl(const bdaddr_t *bdaddr, u16 con_handle, u8 status);
int enqueue_hci_event_role_change(const bdaddr_t *bdaddr, u8 role);

#endif
