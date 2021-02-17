/* pvfs_ftruncate.c copyright (c) 1998 Clemson University, all rights reserved.

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

static char pvfs_ftruncate_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_ftruncate.c,v 1.2 1999/08/13 23:26:21 rbross Exp $";

#include <lib.h>
#include <meta.h>

extern int errno;
extern fdesc_p pfds[];
extern sockset socks;
extern jlist_p active_p;

#define PCNT pfds[fd]->fd.meta.p_stat.pcount
#define FINO pfds[fd]->fd.meta.u_stat.st_ino

int pvfs_ftruncate(int fd, size_t length)
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
		return(syscall(SYS_ftruncate, fd, length));
	}

	if (pfds[fd]->fs == FS_PDIR) {
		errno = EISDIR;
		return(-1);
	}

	req.type                 = IOD_FTRUNCATE;
	req.majik_nr             = IOD_MAJIK_NR;
	req.dsize                = 0;
	req.req.ftruncate.f_ino  = FINO;
	req.req.ftruncate.cap    = pfds[fd]->fd.cap;
	req.req.ftruncate.length = length;

	/* build job to send reqs and recv acks */
	if (build_simple_jobs(pfds[fd], &req) < 0) {
		ERR("pvfs_ftruncate: build_simple_jobs failed\n");
		return(-1);
	}

	/* call do_job */
	while (!jlist_empty(active_p)) {
		if (do_jobs(active_p, &socks, -1) < 0) {
			ERR("pvfs_ftruncate: do_jobs failed\n");
			return(-1);
		}
	}

	/* check acks from iods */
	for (i=0; i < PCNT; i++) {
		if (pfds[fd]->fd.iod[i].ack.status) {
			ERR1("pvfs_ftruncate: non-zero status returned from iod %d\n", i);
			errno = pfds[fd]->fd.iod[i].ack.eno;
			return(-1);
		}
	}
	return(0);
}
