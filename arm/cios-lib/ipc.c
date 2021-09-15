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

#include "ipc.h"
#include "syscalls.h"
#include "types.h"


void InvalidateVector(ioctlv *vector, u32 inlen, u32 iolen)
{
	u32 cnt;

	for (cnt = 0; cnt < (inlen + iolen); cnt++) {
		void *buffer = vector[cnt].data;
		u32   len    = vector[cnt].len;

		/* Invalidate cache */
		os_sync_before_read(buffer, len);
	}
}

void FlushVector(ioctlv *vector, u32 inlen, u32 iolen)
{
	u32 cnt;

	for (cnt = inlen; cnt < (inlen + iolen); cnt++) {
		void *buffer = vector[cnt].data;
		u32   len    = vector[cnt].len;

		/* Flush cache */
		os_sync_after_write(buffer, len);
	}
}
