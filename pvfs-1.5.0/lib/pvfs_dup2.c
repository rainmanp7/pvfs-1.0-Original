/* pvfs_dup2.c copyright (c) 1996 Clemson University, all rights reserved.
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

static char pvfs_dup2_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_dup2.c,v 1.4 2000/10/30 14:44:49 rbross Exp $";

#include <lib.h>

extern fdesc_p pfds[];
extern int errno;

int pvfs_dup2(int old_fd, int new_fd)
{
	int ret;


	if (old_fd < 0 || old_fd >= NR_OPEN || new_fd < 0 || new_fd >= NR_OPEN
	|| (pfds[old_fd] && pfds[old_fd]->fs == FS_RESV) 
	|| (pfds[new_fd] && pfds[new_fd]->fs == FS_RESV))
	{
		errno = EBADF;
		return(-1);
	}  
	
	/* calling pvfs_close() for PVFS directories now too */
	if (pfds[new_fd] && (pfds[new_fd]->fs == FS_PVFS
	|| pfds[new_fd]->fs == FS_PDIR))
	{
		pvfs_close(new_fd);
	}
	if ((ret = dup2(old_fd, new_fd)) == -1) {
		return(-1);
	}
	else {
		if (pfds[old_fd] && (pfds[old_fd]->fs == FS_PVFS ||
		pfds[old_fd]->fs == FS_PDIR))
		{
			pfds[ret] = pfds[old_fd];
			pfds[ret]->fd.ref++;
		}
		return(ret);
	}
}
