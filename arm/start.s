/*
	Custom IOS Module (USB)

	Copyright (C) 2008 neimod.
	Copyright (C) 2009 WiiGator.
	Copyright (C) 2009 Waninkoko.
	Copyright (C) 2009 davebaol.

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

	.section ".init"
	.arm

	.EQU	ios_thread_arg,		4
	.EQU	ios_thread_priority,	0x48
	.EQU	ios_thread_stacksize,	0x2000


	.global _start
_start:
	mov	r0, #0		@ int argc
	mov	r1, #0		@ char *argv[]
	ldr	r3, =main
	bx	r3



/*
 * IOS bss
 */
	.section ".ios_bss", "a", %nobits

	.space	ios_thread_stacksize
	.global ios_thread_stack	/* stack decrements from high address.. */
ios_thread_stack:


/*
 * IOS info table
 */
	.section ".ios_info_table", "ax", %progbits

	.global ios_info_table
ios_info_table:
	.long	0x0
	.long	0x28			@ numentries * 0x28
	.long	0x6

	.long	0xB
	.long	ios_thread_arg		@ passed to thread entry func, maybe module id

	.long	0x9
	.long	_start

	.long	0x7D
	.long	ios_thread_priority

	.long	0x7E
	.long	ios_thread_stacksize

	.long	0x7F
	.long	ios_thread_stack
