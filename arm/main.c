/*
	Custom IOS Module (USB)

	Copyright (C) 2008 neimod.
	Copyright (C) 2009 WiiGator.
	Copyright (C) 2009 Waninkoko.
	Copyright (C) 2011 davebaol.
	Copyright (C) 2020 Leseratte.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "ipc.h"
#include "mem.h"
#include "stealth.h"
#include "syscalls.h"
#include "vsprintf.h"
#include "timer.h"
#include "tools.h"
#include "types.h"

#define printf svc_printf

static unsigned long arm_gen_branch_thumb2(unsigned long pc,
					   unsigned long addr, bool link)
{
	unsigned long s, j1, j2, i1, i2, imm10, imm11;
	unsigned long first, second;
	long offset;

	offset = (long)addr - (long)(pc + 4);
	if (offset < -16777216 || offset > 16777214)
		return 0;

	s	= (offset >> 24) & 0x1;
	i1	= (offset >> 23) & 0x1;
	i2	= (offset >> 22) & 0x1;
	imm10	= (offset >> 12) & 0x3ff;
	imm11	= (offset >>  1) & 0x7ff;

	j1 = (!i1) ^ s;
	j2 = (!i2) ^ s;

	first = 0xf000 | (s << 10) | imm10;
	second = 0x9000 | (j1 << 13) | (j2 << 11) | imm11;
	if (link)
		second |= 1 << 14;

	first  = __builtin_bswap16(first);
	second = __builtin_bswap16(second);

	return __builtin_bswap32(first | (second << 16));
}

static inline u32 read32(u32 addr)
{
	return *(vu32 *)addr;
}

static void hook()
{
	while (1)
		printf("Hook!\n");
}

int main(void)
{
	int i = 0;

	/* Print info */
	svc_write("Hello world from Starlet!\n");

	#define BL_ADDR	0x138b23c6

	u32 orig_bl_insn = read32(BL_ADDR);
	printf("orig_bl_insn: 0x%08X\n", orig_bl_insn);

	// 0x138b365c
	printf("hook addr: 0x%08X\n", (uintptr_t)&hook);
	u32 new_bl_insn = arm_gen_branch_thumb2(BL_ADDR, (uintptr_t)&hook, true);
	printf("new_bl_insn: 0x%08X\n", new_bl_insn);

	printf("Before hook\n");

	u32 perms = Perms_Read();
	Perms_Write(0xFFFFFFFF);
	DCWrite32(BL_ADDR, new_bl_insn);
	ICInvalidate();
	Perms_Write(perms);

	printf("Hooked!\n");

	while (1) {
		///os_thread_stop(os_get_thread_id());
		//svc_printf("i: %d\n", i++);
		os_thread_yield();
	}

#if 0
	/* Initialize module */
	ret = __USB_Initialize();
	if (ret < 0)
		return ret;

	/* Main loop */
	while (1) {
		ipcmessage *message = NULL;

		/* Wait for message */
		os_message_queue_receive(queuehandle, (void *)&message, 0);

		/* Check callback */
		ret = __USB_Callback((u32)message);
		if (!ret)
			continue;

		switch (message->command) {
		case IOS_OPEN: {

			/* Block opening request if a title is running */
			ret = Stealth_CheckRunningTitle(NULL);
			if (ret) {
				ret = IPC_ENOENT;
				break;
			}

			/* Check device path */
			if (!strcmp(message->open.device, DEVICE_NAME))
				ret = message->open.resultfd;
			else
				ret = IPC_ENOENT;

			break;
		}

		case IOS_CLOSE: {
			/* Do nothing */
			ret = 0;
			break;
		}

		case IOS_IOCTLV: {
			ioctlv *vector = message->ioctlv.vector;
			u32     inlen  = message->ioctlv.num_in;
			u32     iolen  = message->ioctlv.num_io;
			u32     cmd    = message->ioctlv.command;

			/* Parse IOCTLV message */
			ret = __USB_Ioctlv(cmd, vector, inlen, iolen);

			break;
		}

		default:
			/* Unknown command */
			ret = IPC_EINVAL;
		}

		/* Acknowledge message */
		os_message_queue_ack(message, ret);
	}
#endif
	return 0;
}
