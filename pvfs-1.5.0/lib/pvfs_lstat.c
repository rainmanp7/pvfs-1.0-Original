/* pvfs_lstat.c copyright (c) 1996 Clemson University, all rights reserved.

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

static char pvfs_lstat_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_lstat.c,v 1.4 2000/01/13 05:56:06 rbross Exp $";

/* This file contains PVFS library call for lstat.   		*/
/* It will determine if file is UNIX or PVFS and then 	*/
/* make the proper request.										*/

#include <lib.h>
#include <meta.h>
#include <sys/stat.h>

extern int errno;
extern int pvfs_checks_disabled;

int pvfs_lstat(char* pathname, struct stat *buf)
{
	int i, s, retval;
	mack ack;
	mreq request;
	char *t_p;
	struct sockaddr *saddr;
	char *fn;
	int64_t fs_ino;

	retval = unix_lstat(pathname, buf);
	if (S_ISLNK(buf->st_mode)) return retval;

	/* check if PVFS file */
	if ((i = pvfs_detect(pathname, &fn, &saddr, &fs_ino)) < 1) {
		if (i < 0) {
			PERROR("Error finding file");
			return(-1);
		}	
		return retval;
	}

	/* Prepare request for file system  */
	request.dsize = strlen(fn);
	request.uid = getuid();
	request.gid = getgid();
	request.type = MGR_STAT;
	request.req.stat.fname = 0;

	/* Send request to mgr */
	if ((s = send_mreq_saddr(saddr, &request, fn, &ack)) < 0) {
		PERROR("send_mreq");
		return(-1);
	}
	*buf = ack.ack.stat.meta.u_stat;

	return (0);
}
