/* pvfs_rmdir.c copyright (c) 1996 Clemson University, all rights reserved.

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

static char pvfs_rmdir_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_rmdir.c,v 1.5 2000/10/30 14:44:49 rbross Exp $";
/* This file contains PVFS library call for unlink.   	*/
/* It will determine if file is UNIX or PVFS and then 	*/
/* make the proper request.										*/

#include <lib.h>

extern int errno;
extern int pvfs_checks_disabled;

int pvfs_rmdir(const char* pathname)
{
	int i, s;
	mreq request;
	mack ack;
	struct sockaddr *saddr;
	char *fn;
	int64_t fs_ino;

	if (!pathname) {
		errno = EFAULT;
		return(-1);
	}
	
	if (pvfs_checks_disabled) return rmdir(pathname);

	/* Check to see if .pvfsdir exists in directory  	*/
	if ((i = pvfs_detect(pathname, &fn, &saddr, &fs_ino)) < 1) {
		if (i < 0) {
			PERROR("pvfs_rmdir: finding dotfile");
			return(-1);
		}	
		return rmdir(pathname);
	}
	if (i == 1) {
		errno = ENOTDIR;
		return(-1);
	}

	/* Prepare request for file system  */
	request.dsize = strlen(fn);
	request.uid = getuid();
	request.gid = getgid();
	request.type = MGR_RMDIR;
	request.req.rmdir.fname = 0;

	/* Send request to mgr */	
	if ((s = send_mreq_saddr(saddr, &request, fn, &ack)) < 0) {
		PERROR("send_mreq");
		return(-1);
	}

	return (0);
}
