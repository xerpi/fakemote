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

#ifndef _STEALTH_H_
#define _STEALTH_H_ 

#include "types.h"

/* Constants */
#define STEALTH_RUNNING_TITLE  0x01
#define STEALTH_ES_REQUEST     0x02

#define TID_RIGHTS_FORCE_REAL_NAND  0x01
#define TID_RIGHTS_OPEN_FAT         0x02


/* Prototypes */
s32  Stealth_CheckRunningTitle(const char* command);
s32  Stealth_CheckEsRequest(const char* command);
void Stealth_Log(u32 type, const char* command);

#endif
