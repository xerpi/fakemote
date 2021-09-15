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

#ifndef _SWI_MLOAD_H_
#define _SWI_MLOAD_H_

#include "ios.h"
#include "tools.h"
#include "types.h"

typedef s32(*TRCheckFunc)(s32 tid, u32 rights);

/* Macros */
#define Swi_SetRegister(A, V)                         Swi_MLoad(  5, (A), (V), 0)
#define Swi_ClearRegister(A, V)                       Swi_MLoad(  6, (A), (V), 0)
#define Swi_GetSyscallBase()                          Swi_MLoad( 17,   0,   0, 0)
#define Swi_SetRunningTitle(V)                        Swi_MLoad( 32, (V),   0, 0)
#define Swi_GetRunningTitle()                         Swi_MLoad( 33,   0,   0, 0)
#define Swi_SetEsRequest(V)                           Swi_MLoad( 34, (V),   0, 0)
#define Swi_GetEsRequest()                            Swi_MLoad( 35,   0,   0, 0)
#define Swi_AddThreadRights(T, R)                     Swi_MLoad( 36, (T), (R), 0)
#define Swi_GetThreadRightsCheckFunc()  ((TRCheckFunc)Swi_MLoad( 37,   0,   0, 0))
#define Swi_LedOn()                                   Swi_MLoad(128,   0,   0, 0)
#define Swi_LedOff()                                  Swi_MLoad(129,   0,   0, 0)
#define Swi_LedBlink()                                Swi_MLoad(130,   0,   0, 0)

/* Prototypes */
void Swi_Memcpy(void *dst, void *src, s32 len);
void Swi_uMemcpy(void *dst, void *src, s32 len);
s32  Swi_CallFunc(s32 (*func)(void *in, void *out), void *in, void *out);
u32  Swi_GetIosInfo(iosInfo *buffer);

#endif
