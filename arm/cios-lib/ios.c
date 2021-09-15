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
#include "str_utils.h"
#include "syscalls.h"
#include "swi_mload.h"
#include "tools.h"
#include "types.h"

#define getNumPatchers(S)	((S) / sizeof(patcher))
#define getModuleVersion(ERR)   (((u32 *)(&ios))[-(ERR)])

/* IOS information */
iosInfo ios = { 0, 0, 0, 0, 0 };

/* 
 * NOTE:
 * This function MUST execute in supervisor mode.
 * It means you can't use svc_write or any other supervisor call
 * from inside this function.
 * That's why we need to collect patch status and
 * write it to the log only after returning from supervisor mode.
*/
s32 IOS_PatchModules(patcher patchers[], u32 size)
{
	u32 perms, cnt;

	/* Invalidate cache */
	ICInvalidate();

	/* Apply permissions */
	perms = Perms_Read();
	Perms_Write(0xFFFFFFFF);

	/* Patch modules */
	for(cnt = 0; cnt < getNumPatchers(size); cnt++) {
		if (patchers[cnt].function != NULL) {
			patchers[cnt].status = patchers[cnt].function();
		}
	}

	/* Restore permissions */
	Perms_Write(perms);

	return 0;
}

s32 IOS_CheckPatches(patcher patchers[], u32 size)
{
#ifdef CIOSLIB_DEBUG
	static char buffer[10];
#endif

	s32 ret = 0;
	u32 cnt;

	/* Check module patches */
	for(cnt = 0; cnt < getNumPatchers(size); cnt++) {
		if (patchers[cnt].status == 0)
			continue;

		svc_write(moduleName);
		svc_write(": Error in patcher");
#ifdef CIOSLIB_DEBUG
		svc_write("[");
		svc_write(itoa(cnt, buffer, 10));
		svc_write("] ");
#endif
		svc_write(": ");
		if (patchers[cnt].status > 0)
			svc_write((char *)patchers[cnt].status);
		else {
			svc_write("Unknown module version");
#ifdef CIOSLIB_DEBUG
			svc_write(" 0x");
			svc_write(itoa(getModuleVersion(patchers[cnt].status), buffer, 16));
#endif
		}
		svc_write(".\n");
		ret = -1;
	}

	return ret;
}

s32 IOS_InitSystem(patcher patchers[], u32 size)
{
	s32 ret = 0;

	/* Get IOS info */
	if (ios.syscallBase == 0)
		Swi_GetIosInfo(&ios);

	if (patchers != NULL) {
		/* Patch system through swi vector */
		ret = Swi_CallFunc((void *)IOS_PatchModules, patchers, (void *)size);

		/* Check modules patch */
		if (!ret)
			ret = IOS_CheckPatches(patchers, size);
	}

	return ret;
}
