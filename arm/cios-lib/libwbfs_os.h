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

#ifndef _LIBWBFS_OS_H_
#define _LIBWBFS_OS_H_

#include "mem.h"
#include "types.h"

#define debug_printf(fmt, ...)

#define wbfs_fatal(x)		do { debug_printf("\nwbfs panic:%s\n\n", x); while(1); } while(0)
#define wbfs_error(x)		do { debug_printf("\nwbfs error:%s\n\n", x); } while(0)
#define wbfs_malloc(x)		Mem_Alloc(x)
#define wbfs_free(x)		Mem_Free(x)
#define wbfs_ioalloc(x)		Mem_Alloc(x)
#define wbfs_iofree(x)		Mem_Free(x)
#define wbfs_ntohl(x)		(x)
#define wbfs_htonl(x)		(x)
#define wbfs_ntohs(x)		(x)
#define wbfs_htons(x)		(x)

#include <string.h>

#define wbfs_memcmp(x,y,z)	memcmp(x,y,z)
#define wbfs_memcpy(x,y,z)	memcpy(x,y,z)
#define wbfs_memset(x,y,z)	memset(x,y,z)


#endif
