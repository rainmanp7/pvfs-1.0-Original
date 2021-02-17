/* pvfs_fcntl.c copyright (c) 1996 Rob Ross, all rights reserved.
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

static char pvfs_fcntl_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_fcntl.c,v 1.4 2000/10/30 14:44:49 rbross Exp $";

/* UNIX INCLUDES */
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

/* PVFS INCLUDES */
#include <lib.h>

/* GLOBALS/EXTERNS */
extern fdesc_p pfds[];

int pvfs_fcntl(int fd, int cmd, long arg)
{
	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  
	if (!pfds[fd] || pfds[fd]->fs == FS_UNIX) /* not a PVFS file */ {
		return fcntl(fd, cmd, arg);
	}

	if (pfds[fd]->fs == FS_PDIR) /* PVFS directory -- just make the call */ {
		return fcntl(fd, cmd, arg);
	}

	/* we could try to lock on the NFS metadata here for F_SETLKW or
	 * F_SETLK, but we need to make sure I didn't just open /dev/null in
	 * that case...
	 */
	switch (cmd) {
	case F_DUPFD :
		errno = EINVAL;
		ERR("pvfs_fcntl: command not yet implemented\n");
		return(-1);
	case F_GETFD :
		return(1); /* PVFS files will not remain open on exec */
	case F_SETFD :
		if (arg & 1 != 0) return(0);
		errno = EINVAL;
		ERR("pvfs_fcntl: command not yet implemented\n");
		return(-1);
	case F_GETFL :
		return(pfds[fd]->fd.flag);
	case F_SETFL :
	case F_GETLK :
	case F_SETLK :
	case F_SETLKW :
	case F_GETOWN :
	case F_SETOWN :
	default :
		errno = EINVAL;
		ERR("pvfs_fcntl: command not yet implemented\n");
		return(-1);
	}
}
