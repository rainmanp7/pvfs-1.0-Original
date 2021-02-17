/*
	pvfs_readv.c copyright (c) 1998 Clemson University, all rights reserved.

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

static char pvfs_readv_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_readv.c,v 1.4 2000/10/30 14:44:49 rbross Exp $";

#include <lib.h>
#include <sys/uio.h>

extern fdesc_p pfds[];
extern jlist_p active_p;
extern sockset socks;

static int unix_readv(int fd, const struct iovec *vector, size_t count);

int pvfs_readv(int fd, const struct iovec *vector, size_t count)
{
	fdesc_p pfd_p = pfds[fd];

	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  

	if (!pfd_p || pfd_p->fs == FS_UNIX) return(unix_readv(fd, vector, count));
	if (pfd_p->fs == FS_PDIR) return(unix_readv(fd, vector, count));

	errno = ENOSYS;
	return(-1);
}

#define OFFSET req.req.rw.off
#define FSIZE  req.req.rw.fsize
#define GSIZE  req.req.rw.gsize
#define LSIZE  req.req.rw.lsize
#define GCOUNT req.req.rw.gcount
#define STRIDE req.req.rw.stride

static int unix_readv(int fd, const struct iovec *vector, size_t count)
{
	fdesc_p fd_p = pfds[fd];

	/* if there's no partition, then we don't have to do any work! */
	if (!fd_p || !fd_p->part_p) return(readv(fd, vector, count));

	errno = ENOSYS;
	return(-1);
}
