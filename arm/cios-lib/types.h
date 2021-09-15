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

#ifndef _IOS_TYPES_H_
#define _IOS_TYPES_H_

#include <stdint.h>

/* NULL constant */
#ifndef NULL
# define NULL			((void *)0)
#endif

/* Data types */
typedef char			s8;
typedef short			s16;
typedef long			s32;
typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;
typedef volatile unsigned char	vu8;
typedef volatile unsigned short	vu16;
typedef volatile unsigned int	vu32;

typedef uint32_t		sec_t;

typedef int			bool;

/* Boolean constants */
#define true			1
#define false			0

/* Attributes */
#ifndef ATTRIBUTE_ALIGN
# define ATTRIBUTE_ALIGN(v)	__attribute__((aligned(v)))
#endif
#ifndef ATTRIBUTE_PACKED
# define ATTRIBUTE_PACKED	__attribute__((packed))
#endif

/* Stack align */
#define STACK_ALIGN(type, name, cnt, alignment)	\
	u8 _al__##name[((sizeof(type)*(cnt)) + (alignment) + (((sizeof(type)*(cnt))%(alignment)) > 0 ? ((alignment) - ((sizeof(type)*(cnt))%(alignment))) : 0))]; \
	type *name = (type*)(((u32)(_al__##name)) + ((alignment) - (((u32)(_al__##name))&((alignment)-1))))

#endif