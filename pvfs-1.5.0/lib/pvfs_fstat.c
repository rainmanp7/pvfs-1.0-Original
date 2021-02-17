/* pvfs_fstat.c copyright (c) 1996 Rob Ross, all rights reserved.
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

static char pvfs_fstat_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_fstat.c,v 1.5 2000/10/30 14:44:49 rbross Exp $";

/* This file contains PVFS library call for fstat.   		*/
/* It will determine if file is UNIX or PVFS and then 	*/
/* make the proper request.										*/

#include <lib.h>

extern int errno;
extern fdesc_p pfds[];

int pvfs_fstat(int fd, struct stat *buf)
{
	int s;
	struct sockaddr saddr;
	mack ack;
	mreq req;

	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  

	if (!pfds[fd] || pfds[fd]->fs == FS_UNIX) {
		return(unix_fstat(fd, buf));
	}	

	if (pfds[fd]->fs == FS_PDIR) {
		/* PVFS directory, send a stat request */
		req.uid = getuid();
		req.gid = getgid();
		req.type = MGR_STAT;
		req.dsize = strlen(pfds[fd]->fn_p);
		req.req.stat.fname = 0;

		saddr = pfds[fd]->fd.meta.mgr;
		if ((s=send_mreq_saddr(&saddr, &req, pfds[fd]->fn_p, &ack)) < 0) {
			perror("send_mreq_saddr");
			return(-1);
		}
		*buf = ack.ack.stat.meta.u_stat;
	}
	else {
		/* otherwise it is a PVFS file -- prepare request */
		req.uid  = getuid();
		req.gid  = getgid();
		req.type = MGR_FSTAT;
		req.dsize= 0;
		req.req.fstat.meta = pfds[fd]->fd.meta;

		/* send request to mgr */
		saddr = pfds[fd]->fd.meta.mgr;
		if ((s = send_mreq_saddr(&saddr, &req, NULL, &ack)) < 0) {
			perror("send_mreq_saddr");
			return(-1);
		}
		*buf = ack.ack.fstat.meta.u_stat;
	}

	/* don't close manager connection */

	return (0);
}
