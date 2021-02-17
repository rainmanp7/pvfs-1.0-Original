/* pvfs_chown.c copyright (c) 1996 Clemson University, all rights reserved.
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
	Contact:  Rob Ross   rbross@parl.clemson.edu or
	          Walt Ligon walt@parl.clemson.edu

 */

static char pvfs_chown_c[] = "$Header:
/projects/pvfs2/lib/RCS/pvfs_chown.c,v 1.3 1996/12/05 19:06:03 mcettei Exp $";

/* This file contains PVFS library call for chown.   		*/
/* It will determine if file is UNIX or PVFS and then 	*/
/* make the proper request.										*/

#include <lib.h>
#include <meta.h>

extern int errno;
extern int pvfs_checks_disabled;

int pvfs_chown(const char* pathname, uid_t owner, gid_t group)
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
	if (pvfs_checks_disabled) return chown(pathname, owner, group);

	/* Check to see if .pvfsdir exists in directory  	*/
	if ((i = pvfs_detect(pathname, &fn, &saddr, &fs_ino)) < 1) {
		if (i < 0) {
			PERROR("pvfs_chown: finding file");
			return(-1);
		}	
		return chown(pathname, owner, group);
	}

	/* Prepare request for file system  */
	/* here's this hack again! */
	request.dsize = strlen(fn);
	request.uid = getuid();
	request.gid = getgid();
	request.type = MGR_CHOWN;
	request.req.chown.fname = 0;
	request.req.chown.owner = (int)owner;
	request.req.chown.group = (int)group;

	/* Send request to mgr */	
	if ((s = send_mreq_saddr(saddr, &request, fn, &ack)) < 0) {
		PERROR("send_mreq");
		return(-1);
	}

	return (0);
}
