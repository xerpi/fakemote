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


#include <string.h>

#include "types.h"

void FAT_Escape(char *dst, const char *src)
{
	char c;

	/* Escape invalid FAT characters */
	while ((c = *(src++)) != '\0') {
		char *esc;

		/* Check character */
		switch (c) {
		case '"': esc = "&qt;"; break;   // Escape double quote
		case '*': esc = "&st;"; break;   // Escape star
		case ':': esc = "&cl;"; break;   // Escape colon
		case '<': esc = "&lt;"; break;   // Escape lesser than
		case '>': esc = "&gt;"; break;   // Escape greater than
		case '?': esc = "&qm;"; break;   // Escape question mark
		case '|': esc = "&vb;"; break;   // Escape vertical bar
		default: *(dst++) = c; continue; // Copy valid FAT character
		}

		/* Replace escape sequence */
		strcpy(dst, esc);
		dst += 4;
	}

	/* End of string */
	*dst = '\0';
}

/*
 * Unescape in place and return the length of the unescaped path
 */
s32 FAT_Unescape(char *path)
{
	char *src = path;
	char *dst = path;
	char c;

	/* Unescape invalid FAT characters */
	while ((c = *(src++)) != '\0') {

		/* Check character */
		if (c == '&') {
			if      (!strncmp(src, "qt;", 3)) c = '"'; // Unescape double quote     
			else if (!strncmp(src, "st;", 3)) c = '*'; // Unescape star             
			else if (!strncmp(src, "cl;", 3)) c = ':'; // Unescape colon            
			else if (!strncmp(src, "lt;", 3)) c = '<'; // Unescape lesser than      
			else if (!strncmp(src, "gt;", 3)) c = '>'; // Unescape greater than     
			else if (!strncmp(src, "qm;", 3)) c = '?'; // Unescape question mark    
			else if (!strncmp(src, "vb;", 3)) c = '|'; // Unescape vertical bar     

			/* Skip matched escape sequence */
			if (c != '&')
				src += 3;
		} 

		/* Copy character */
		*(dst++) = c;
	}

	/* End of string */
	*dst = '\0';

	/* Return length */
	return dst - path;
}

