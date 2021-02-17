/* resv_name.c copyright (c) 2000 Clemson University, all rights reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * Contact:  Rob Ross  	rbross@parl.eng.clemson.edu
 * 
 */

#include <stdio.h>
#include <string.h>

extern int errno;

/* resv_name(name)
 *
 * Returns non-zero if a name corresponds to a PVFS reserved name, 0
 * otherwise.
 *
 * At this time, the names ".iodtab" and ".pvfsdir" are the only PVFS
 * reserved names.
 */
int resv_name(char *name)
{
	int len, i;

	len = strlen(name); /* not including terminator */
	if (len < 7) return 0; /* our names are longer than this one */

	if (name[0] == '/' || (name[0] == '.' && name[1] == '.' && name[2] == '/')
	|| (name[0] == '.' && name[1] == '/'))
	{
		/* gotta handle directories */
		for (i = len-1; i >= 0; i--) if (name[i] == '/') break;
		/* position i of name has '/' */
		if (i > len - 7) return 0; /* again, our names are longer than this */

		i++; /* point to the first character of the name */
		if (!strcmp(&name[i], ".iodtab")
		|| !strcmp(&name[i], ".pvfsdir"))
			return 1;
	}
	else {
		/* no directories */
		if (!strcmp(name, ".iodtab") || !strcmp(name, ".pvfsdir")) return 1;
	}

	return 0;
}
