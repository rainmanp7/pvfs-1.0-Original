/* pvfs_fsync.c copyright (c) 1998 Clemson University, all rights reserved.

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
	          Walt Ligon walt@eng.clemson.edu
 */

static char pvfs_fsync_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_fsync.c,v 1.3 2000/10/30 14:44:49 rbross Exp $";

#include <lib.h>
#include <meta.h>

extern int errno;
extern fdesc_p pfds[];

#define PCNT pfds[fd]->fd.meta.p_stat.pcount
#define FINO pfds[fd]->fd.meta.u_stat.st_ino

/* pvfs_fsync() - sends sync request to iods; uses FDATASYNC request
 */
int pvfs_fsync(int fd)
{
	/* check for badness */
	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  

	/* check for UNIX */
	if (!pfds[fd] || pfds[fd]->fs==FS_UNIX) {
		return fsync(fd);
	}

	if (pfds[fd]->fs==FS_PDIR) {
		/* call the syscall for PVFS directories */
		return fsync(fd);
	}

	return(pvfs_fdatasync(fd));
}
