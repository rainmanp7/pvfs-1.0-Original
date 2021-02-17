/* pvfs_fchmod.c copyright (c) 1996 Clemson University, all rights reserved.
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

static char pvfs_fchmod_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_fchmod.c,v 1.3 2000/10/30 14:44:49 rbross Exp $";

/* This file contains PVFS library call for fchmod.  		*/
/* It will determine if file is UNIX or PVFS and then 	*/
/* make the proper request.										*/

#include <lib.h>
#include <meta.h>

extern int errno;
extern fdesc_p pfds[];

int pvfs_fchmod(int fd, mode_t mode)
{
	int s;
	struct sockaddr saddr;
	mreq request;
	mack ack;

	/* check for badness */
	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  

	/* check for UNIX */
	if (!pfds[fd] || pfds[fd]->fs==FS_UNIX) {
		return fchmod(fd, mode);
	}

	/* PVFS file or directory -- Prepare request for file system  */
	request.uid = getuid();
	request.gid = getgid();
	request.type = MGR_FCHMOD;
	request.dsize = 0;
	request.req.fchmod.mode = mode;
	request.req.fchown.file_ino = pfds[fd]->fd.meta.u_stat.st_ino;
	request.req.fchown.fs_ino = pfds[fd]->fd.meta.fs_ino;


	/* Send request to mgr */	
	saddr = pfds[fd]->fd.meta.mgr;
	if ((s = send_mreq_saddr(&saddr, &request, NULL, &ack)) < 0) {
		PERROR("send_mreq_saddr");
		return(-1);
	}
	
	/* Don't close manager connection */

	return (0);
}
