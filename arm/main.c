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
#include "syscalls.h"
#include "vsprintf.h"
#include "tools.h"
#include "types.h"

#define OH1_SYSCALL_RECEIVE_MESSAGE_ADDR1 0x138b365c
#define OH1_SYSCALL_RECEIVE_MESSAGE_ADDR2 0x138b366c
#define OH1_DEV_OH1_QUEUEID_ADDR          0x138b5004

char *moduleName = "TST";

static int OH1_IOS_ReceiveMessage_hook(int queueid, ipcmessage **message, u32 flags)
{
	int ret = os_message_queue_receive(queueid, (void *)message, flags);

	svc_printf("Hook!: %d vs %d\n", queueid, *(int *)OH1_DEV_OH1_QUEUEID_ADDR);

	return ret;
}

static s32 Patch_OH1UsbModule(void)
{
	u32 addr;

	/* Check version */
	u32 bytes = *(u16 *)OH1_SYSCALL_RECEIVE_MESSAGE_ADDR1;
	if (bytes == 0x4778)
		addr = OH1_SYSCALL_RECEIVE_MESSAGE_ADDR1;
	else if (bytes == 0xbd00)
		addr = OH1_SYSCALL_RECEIVE_MESSAGE_ADDR2;
	else
		return -1;
	
	/* Patch syscall handler to jump to our function */
	DCWrite32(addr    , 0x4B004718);
	DCWrite32(addr + 4, (u32)OH1_IOS_ReceiveMessage_hook);

	return 0;
}

int main(void)
{
	int ret;
	u32 i = 0;

	/* Print info */
	svc_write("Hello world from Starlet!\n");
	
	/* System patchers */
	patcher patchers[] = {
		{Patch_OH1UsbModule, 0},
	};
	
	/* Initialize plugin */
	ret = IOS_InitSystem(patchers, sizeof(patchers));
	svc_printf("IOS_InitSystem(): %d\n", ret);
	
	svc_printf("Patch status: %d\n", patchers[0].status);

	svc_write("System patches applied\n");

	while (1) {
		if ((i++ % (256*1024)) == 0) {
			//svc_printf("Ping: %d\r\n", i >> 17);
		}
		os_thread_yield();
	}

	return 0;
}
