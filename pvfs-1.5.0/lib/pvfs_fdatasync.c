/*
	pvfs_fdatasync.c copyright (c) 1998 Clemson University, all rights reserved.

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

static char pvfs_fdatasync_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_fdatasync.c,v 1.3 2000/10/30 14:44:49 rbross Exp $";

#include <lib.h>
#include <meta.h>

extern int errno;
extern fdesc_p pfds[];
extern sockset socks;
extern jlist_p active_p;

#define PCNT pfds[fd]->fd.meta.p_stat.pcount
#define FINO pfds[fd]->fd.meta.u_stat.st_ino

#ifdef __ALPHA__
#define SYS_fdatasync SYS_fsync
#endif

int pvfs_fdatasync(int fd)
{
	int i;
	ireq req;

	/* check for badness */
	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  

	/* check for UNIX */
	if (!pfds[fd] || pfds[fd]->fs==FS_UNIX) {
		return fdatasync(fd);
	}

	/* as usual, normal call for PVFS directory */
	if (pfds[fd]->fs==FS_PDIR) {
		return fdatasync(fd);
	}


	req.type                 = IOD_FDATASYNC;
	req.majik_nr             = IOD_MAJIK_NR;
	req.dsize                = 0;
	req.req.fdatasync.f_ino  = FINO;
	req.req.fdatasync.cap    = pfds[fd]->fd.cap;

	/* build job to send reqs and recv acks */
	if (build_simple_jobs(pfds[fd], &req) < 0) {
		ERR("pvfs_fdatasync: build_simple_jobs failed\n");
		return(-1);
	}

	/* call do_job */
	while (!jlist_empty(active_p)) {
		if (do_jobs(active_p, &socks, -1) < 0) {
			ERR("pvfs_fdatasync: do_jobs failed\n");
			return(-1);
		}
	}

	/* check acks from iods */
	for (i=0; i < PCNT; i++) {
		if (pfds[fd]->fd.iod[i].ack.status) {
			ERR1("pvfs_fdatasync: non-zero status returned from iod %d\n", i);
			errno = pfds[fd]->fd.iod[i].ack.eno;
			return(-1);
		}
	}
	return(0);
}
