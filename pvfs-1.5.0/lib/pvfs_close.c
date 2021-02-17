/* pvfs_close.c copyright (c) 1996 Rob Ross, all rights reserved.
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
	Contact:  Rob Ross  rbross@parl.eng.clemson.edu  or
				 Matt Cettei mcettei@parl.eng.clemson.edu
 */

static char pvfs_close_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_close.c,v 1.4 2000/10/30 14:44:49 rbross Exp $";

/* This file contains PVFS library call for close.   		*/
/* It will determine if file is UNIX or PVFS and then 	*/
/* make the proper request.										*/
/* Determines if pvfs file and take file off 				*/
/* open list; also, close meta file								*/

#include <lib.h>

extern fdesc_p pfds[];
extern int errno;
extern sockset socks;
extern jlist_p active_p;

int pvfs_close(int fd)
{
	int i, status=0, myeno=0;
	struct sockaddr saddr;
	mreq req;
	mack ack;
	ireq iodreq;

	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}

	/* If we don't know anything about the FD, use the system call */
	if (!pfds[fd]) return close(fd);

	/* Two cases here:
	 * - PVFS file with more references (first case)
	 * - UNIX file
	 * 
	 * NEW CASE:
	 * - PVFS directory
	 *
	 * In all cases, we want to make sure to eliminate any data
	 * structures we held for the file.  Then we want to close the FD.
	 * For PVFS files, this will close the meta file, which will free the
	 * FD slot for later use.  For UNIX files, this will close the actual
	 * file, although there may be more open FD references to the same
	 * file.
	 */
	if ((pfds[fd] && --pfds[fd]->fd.ref) || pfds[fd]->fs == FS_UNIX
	|| pfds[fd]->fs == FS_PDIR)
	{
		if (pfds[fd] && !pfds[fd]->fd.ref) {
			if (pfds[fd]->part_p) free(pfds[fd]->part_p);
			if (pfds[fd]->dim_p)  free(pfds[fd]->dim_p);
			if (pfds[fd]->fn_p)  free(pfds[fd]->fn_p);
			free(pfds[fd]);
		}
		pfds[fd] = NULL;
		return close(fd);
	}

#ifdef STRICT_FDESC_CHECK
	if (do_fdesc_check(fd) < 0) {
		errno = EBADF;
		return(-1);
	}
#endif

	/* Now need to send close directly to iods */
	iodreq.majik_nr   = IOD_MAJIK_NR;
	iodreq.type 		= IOD_CLOSE;
	iodreq.dsize		= 0;
	iodreq.req.close.f_ino	= pfds[fd]->fd.meta.u_stat.st_ino;
	iodreq.req.close.cap	= pfds[fd]->fd.cap;

	/* build job to send reqs and recv acks to/from iods */
	if (!active_p) active_p = jlist_new();
	initset(&socks); /* clear out the socket set */

	if (build_simple_jobs(pfds[fd], &iodreq) < 0) {
		ERR("pvfs_close: build_simple_jobs failed\n");
		goto pvfs_close_try_manager;
	}

	/* call do_job */
	while (!jlist_empty(active_p)) {
		if (do_jobs(active_p, &socks, -1) < 0) {
			ERR("pvfs_close: do_jobs failed\n");
			goto pvfs_close_try_manager;
		}
	}

	/* look for errors from iods */
	for (i=0; i < pfds[fd]->fd.meta.p_stat.pcount; i++) {
		if (pfds[fd]->fd.iod[i].ack.status) {
			myeno = pfds[fd]->fd.iod[i].ack.eno;
			status = -1;
		}
	}

pvfs_close_try_manager:
	/* Prepare request for manager  */
	req.uid 		= getuid();
	req.gid 		= getgid();
	req.type 	= MGR_CLOSE;
	req.dsize 	= 0;
	req.req.close.meta = pfds[fd]->fd.meta;

	if (!(pfds[fd]->fd.flag & O_GADD)) {
		/* Need to connect to mgr, not ready yet */
		saddr = pfds[fd]->fd.meta.mgr;
		if (send_mreq_saddr(&saddr, &req, NULL, &ack) < 0)
		{
			myeno = errno;
			status = -1;
			PERROR("send_mreq_saddr");
		}
	}
#ifdef STRICT_FDESC_CHECK
	if (do_fdesc_check(fd) < 0) {
		errno = EBADF;
		return(-1);
	}
#endif

	/* Close existing iod sockets */
	for (i = 0; i < pfds[fd]->fd.meta.p_stat.pcount; i++) {
		if (pfds[fd]->fd.iod[i].sock >= 0) {
			close(pfds[fd]->fd.iod[i].sock);
		}	
	}	

	if (pfds[fd]->part_p) free(pfds[fd]->part_p);
	if (pfds[fd]->dim_p)  free(pfds[fd]->dim_p);
	if (pfds[fd]->fn_p)  free(pfds[fd]->fn_p);
	free(pfds[fd]);
	pfds[fd] = NULL;
	close(fd); /* close the meta file */

	/* DON'T CLOSE THE MGR SOCKET!!!  IT CONFUSES THE MGRCOMM STUFF! */

	errno = myeno;
	return(status);
}
