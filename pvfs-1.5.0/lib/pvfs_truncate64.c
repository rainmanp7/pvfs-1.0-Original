/*
	pvfs_truncate64.c copyright (c) 1999 Clemson University
   all rights reserved.

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

static char pvfs_truncate64_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_truncate64.c,v 1.4 2000/01/13 05:52:07 rbross Exp $";

#include <lib.h>
#include <meta.h>

extern int errno;
extern int pvfs_checks_disabled;

int pvfs_truncate64(const char *path, int64_t length)
{
	int i, s;
	mreq request;
	mack ack;
	struct sockaddr *saddr;
	char *fn;
	int64_t fs_ino;

	if (!path) {
		errno = EFAULT;
		return(-1);
	}
	
	/* sticking to 32-bit for UNIX for now... */
	if (pvfs_checks_disabled)
		return(syscall(SYS_truncate, path, (off_t)length));

	/* check to see if file is a pvfs file */
	if ((i = pvfs_detect(path, &fn, &saddr, &fs_ino)) != 1) {
		if (i < 0) {
			errno = ENOENT;
			return(-1);
		}
		if (i == 2) {
			errno = EISDIR;
			return(-1);
		}
		/* only 32-bits for UNIX for now */
		if ((off_t) length == length) {
			return(syscall(SYS_truncate, path, (off_t)length));
		}
		else {
			errno = EINVAL;
			return(-1);
		}
	}

	/* Prepare request for file system  */
	request.uid = getuid();
	request.gid = getgid();
	request.type = MGR_TRUNCATE;
	request.dsize = strlen(fn);
	request.req.truncate.fname = 0;
	request.req.truncate.length = length;

	/* Send request to mgr */	
	if ((s = send_mreq_saddr(saddr, &request, fn, &ack)) < 0) {
		PERROR("send_mreq");
		return(-1);
	}

	return(0);
}
