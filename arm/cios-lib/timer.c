/*   
	Custom IOS Library

	Copyright (C) 2008 neimod.
	Copyright (C) 2009 WiiGator.
	Copyright (C) 2009 Waninkoko.
	Copyright (C) 2010 Hermes.
	Copyright (C) 2011 davebaol.

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

#include "ios.h"
#include "mem.h"
#include "syscalls.h"
#include "types.h"

/* Variables */
static s32 queuehandle = -1;
static s32 timerId     = -1;

// Fix d2x v1
// Back to timing logic from rev19 and below.
// This solves the controller lag introduced by rev20 for certain HDD
// when the watchdog is reading.
s32 Timer_Init(void)
{
	void *queuespace = NULL;

	/* Queue already created */
	if (queuehandle >= 0)
		return 0;

	/* Alloc queue space */
	queuespace = Mem_Alloc(0x40);
	if (!queuespace)
		return IPC_ENOMEM;

	/* Create queue */
	queuehandle = os_message_queue_create(queuespace, 16);
	if (queuehandle < 0) {
		Mem_Free(queuespace);
		return queuehandle;
	}

	/* Create timer */
	timerId = os_create_timer(1000000, 1, queuehandle, 0x666);
	if (timerId < 0) {
		os_message_queue_destroy(queuehandle);
		queuehandle = -1;
		Mem_Free(queuespace);
		return timerId;
	}

	/* Stop timer */
	os_stop_timer(timerId);

	return 0;
}

void Timer_Sleep(u32 time)
{
	u32 message;

#ifdef CIOSLIB_DEBUG
	if (queuehandle == -1 || timerId == -1) {
		svc_write(moduleName);
		svc_write(": Timer susbsystem not initialized.\n");
	}
#endif

	/* Send message */
	os_message_queue_send(queuehandle, 0x555, 0);

	/* Restart timer */
	os_restart_timer(timerId, time, 0);

	while (1) {
		/* Wait to receive message */
		os_message_queue_receive(queuehandle, (void *)&message, 0);

		/* Message received */
		if (message == 0x555)
			break;
	}

	/* Wait to receive message */
	os_message_queue_receive(queuehandle, (void *)&message, 0);

	/* Stop timer */
	os_stop_timer(timerId);
}
