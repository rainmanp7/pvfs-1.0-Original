/*
	pvfs_getdents.c copyright (c) 1999 Clemson University, all rights reserved.

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

static char pvfs_getdents_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_getdents.c,v 1.8 2000/10/30 14:44:49 rbross Exp $";

#include <string.h>
#include <lib.h>
#include <linux/types.h>
#include <linux/dirent.h>

#if 0
/* gnu defines are WRONG. */
#include <direntry.h>
#endif

extern fdesc_p pfds[];
extern jlist_p active_p;
extern sockset socks;

#define BUFSZ 4096

static int do_getdents(int fd, struct dirent *buf, int size);

/* pvfs_getdents()
 *
 * This call hooks into the getdents syscall.  It returns structures in
 * the format passed back by the kernel.  This is NOT the format that
 * the libc calls pass back, so you shouldn't call this directly.
 */
int pvfs_getdents(int fd, struct dirent *userp, unsigned int count)
{
	int ret, size = 0;
	fdesc_p pfd_p = pfds[fd];
	struct dirent *dp, *last=0;
	char buf[BUFSZ];

	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  

	if (!pfd_p || pfd_p->fs == FS_UNIX) {
		return(syscall(SYS_getdents, fd, userp, count));
	}

	if (pfd_p->fs == FS_PVFS) {
		errno = ENOTDIR;
		return(-1);
	}

	size = do_getdents(fd, userp, count);
	return size;
}

/* DO_GETDENTS() - handles actually getting the getdents data for a PVFS
 * directory.
 *
 * Returns -1 on failure, the amount read on success.
 */
static int do_getdents(int fd, struct dirent *buf, int size)
{
	int i, s, myeno;
	mreq req;
	mack ack;
	struct sockaddr saddr;

	if (!pfds[fd]->fn_p) {
		/* we need the directory name in terms the manager can understand */
		errno = EINVAL;
		return(-1);
	}

	req.req.getdents.offset = pfds[fd]->fd.off;
	req.req.getdents.length = size;
	req.uid = getuid();
	req.gid = getgid();
	req.type = MGR_GETDENTS;
	req.dsize = strlen(pfds[fd]->fn_p);

	saddr = pfds[fd]->fd.meta.mgr;
	if ((s = send_mreq_saddr(&saddr, &req, pfds[fd]->fn_p, &ack)) < 0) {
		myeno = errno;
		PERROR("do_getdents: send_mreq_saddr");
		errno = myeno;
		return(-1);
	}
	if (ack.status) /* error */ {
		errno = ack.eno;
		return(-1);
	}

	if (!ack.dsize) /* hit EOF */ {
		return(0);
	}

	if ((i = brecv(s, buf, ack.dsize)) < 0) {
		myeno = errno;
		PERROR("do_getdents: brecv (getting data)");
		errno = myeno;
		return(-1);
	}
	pfds[fd]->fd.off = ack.ack.getdents.offset;
	return(i);
}
