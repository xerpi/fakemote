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
#include "log.h"
#include "syscalls.h"
#include "types.h"


static void __dbg_os_sync_check_args(void *ptr, s32 size, const char* func, u32 line)
{
	if (ptr == NULL) {
		if (size > 0)
			LOG_Write("buffer is NULL\n", func, line);
	}
	else if ((u32)ptr & 31)
		LOG_Write("buffer non 32 bytes aligned\n", func, line);
	else if (size & 3)
		LOG_Write("len non 4 bytes aligned\n", func, line);
}

void dbg_os_sync_before_read(void *ptr, s32 size, const char* func, u32 line)
{
	__dbg_os_sync_check_args(ptr, size, func, line);
	__os_sync_before_read(ptr, size);
}

void dbg_os_sync_after_write(void *ptr, s32 size, const char* func, u32 line)
{
	__dbg_os_sync_check_args(ptr, size, func, line);
	__os_sync_after_write(ptr, size);
}