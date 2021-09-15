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

#ifndef _IOS_H_
#define _IOS_H_

#include "types.h"

//#define CIOSLIB_DEBUG

#define IOS_ERROR_DIP -1
#define IOS_ERROR_ES  -2
#define IOS_ERROR_FFS -3
#define IOS_ERROR_IOP -4

/* IOS info structure */
typedef struct {
	/* Syscall base */
	u32 syscallBase;

	/* Module versions */
	u32 dipVersion;
	u32 esVersion;
	u32 ffsVersion;
	u32 iopVersion;
} iosInfo;

/* Module patcher function */
typedef s32(*PatcherFunc)(void);

/* Patcher structure */
typedef struct {
    PatcherFunc function;
    s32         status;
} patcher;


/* Extern global variable */
extern iosInfo ios;
extern char *moduleName;

/* Prototypes */
s32 IOS_InitSystem(patcher patchers[], u32 size);
s32 IOS_CheckPatches(patcher patchers[], u32 size);
s32 IOS_PatchModules(patcher patchers[], u32 size);

#endif
