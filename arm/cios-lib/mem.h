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

#ifndef _MEM_H_
#define _MEM_H_

#include "ios.h"
#include "types.h"

/* Prototypes */
s32   Mem_Init(u32 *heapspace, u32 heapspaceSize);
#ifdef CIOSLIB_DEBUG
void *Mem_Alloc_Debug(u32 size, const char* func, u32 line);
#define Mem_Alloc(S) Mem_Alloc_Debug(S, __FUNCTION__, __LINE__)
#else
void *Mem_Alloc(u32 size);
#endif
void  Mem_Free(void *ptr);

#endif
