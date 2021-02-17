/* pvfs_exit.c copyright (c) 1996 Rob Ross, all rights reserved.
	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as published
	by the Free Software Foundation; either version 2 of the License, or
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of 
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
	Contact:  Rob Ross  	rbross@parl.eng.clemson.edu
				 Matt Cettei mcettei@parl.eng.clemson.edu

 */

static char pvfs_exit_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_exit.c,v 1.2 1999/08/13 23:26:20 rbross Exp $";

#include <lib.h>

extern fdesc_p pfds[];
extern int errno;

void *pvfs_exit(void)
{
	int i;

	for (i = 0; i < NR_OPEN; i++) {
		if (!pfds[i] || pfds[i]->fs != FS_PVFS) continue;
		pvfs_close(i);
	}
	LOG("Closed open files\n");
	return(0);
}	
