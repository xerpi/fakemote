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

	.text

	.align 4
	.code 32

/* Direct syscalls */
	.global DCInvalidateRange
	.type DCInvalidateRange STT_FUNC
DCInvalidateRange:
	mcr	p15, 0, r0, c7, c6, 1
	add	r0, #0x20
	subs	r1, #1
	bne	DCInvalidateRange
	bx	lr

	.global DCFlushRange
	.type DCFlushRange STT_FUNC
DCFlushRange:
	mcr	p15, 0, r0, c7, c10, 1
	add	r0, #0x20
	subs	r1, #1
	bne	DCFlushRange
	bx	lr

	.global ICInvalidate
	.type ICInvalidate STT_FUNC
ICInvalidate:
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0
	bx	lr


/* MLoad syscalls */
	.global Swi_MLoad
	.type Swi_MLoad STT_FUNC
Swi_MLoad:
	svc	0xcc
	bx	lr


/* ARM permissions */
	.global Perms_Read
	.type Perms_Read STT_FUNC
Perms_Read:
	mrc	p15, 0, r0, c3, c0
	bx	lr

	.global Perms_Write
	.type Perms_Write STT_FUNC
Perms_Write:
	mcr	p15, 0, r0, c3, c0
	bx	lr


/* MEM2 routines */
	.global MEM2_Prot
	.type MEM2_Prot STT_FUNC
MEM2_Prot:
	ldr	r1, =0xD8B420A
	strh	r0, [r1]
	bx	lr


/* Tools */
	.global VirtToPhys
	.type VirtToPhys STT_FUNC
VirtToPhys:
	and	r0, #0x7fffffff
	bx	lr

	.global PhysToVirt
	.type PhysToVirt STT_FUNC
PhysToVirt:
	orr	r0, #0x80000000
	bx	lr
