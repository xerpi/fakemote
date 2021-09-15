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

#include <string.h>

#include "ipc.h"
#include "mem.h"
#include "stealth.h"
#include "syscalls.h"
#include "timer.h"
#include "types.h"


int main(void)
{
	// s32 ret;

	/* Print info */
	//svc_write("Hello world!\n");
	svc_write("$IOSVersion: TEST:  " __DATE__ " " __TIME__ " 64M " __D2XL_VER__ " $\n");
	//svc_write("Hello world!\n");


	while (1) {
		///os_thread_stop(os_get_thread_id());
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
