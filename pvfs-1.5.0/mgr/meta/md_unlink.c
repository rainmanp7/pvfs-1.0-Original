/* md_unlink.c copyright (c) 1996,1997 Clemson University, all rights reserved.
 * 
 * Written by Matt Cettei, modified by Rob Ross.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * Contact:  Rob Ross    rbross@parl.eng.clemson.edu
 *           Matt Cettei mcettei@parl.eng.clemson.edu
 * 
 */

static char md_unlink_c[] = "$Header: /projects/cvsroot/pvfs/mgr/meta/md_unlink.c,v 1.2 1999/08/13 23:26:24 rbross Exp $";
/*This file contains the unlink metadata file call for the PVFS*/
/*Assumes that request and return data has been allocated */

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <linux/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <meta.h>
#include <req.h>

extern int errno;

/* md_unlink(request, metadata_ptr) - takes a manager request,
 * determines the name of the metadata file to be accessed, checks
 * permissions for unlinking the file, and unlinks it.
 *
 * On failure the file is closed and -1 is returned.
 *
 * Revision 1.4:
 * On success the file is removed, but a file descriptor is returned
 * for the file (therefore the return value will be >=0).
 */
int md_unlink(mreq_p request, fmeta_p metar_p)
{
	fmeta meta1;
	int fd,length;
	char temp[MAXPATHLEN];

   if ((fd = meta_open(request->req.unlink.fname, O_RDONLY)) < 0) {
		perror("md_unlink: meta_open");
		return (-1);
	}

	/* check execute permission on directory */
	/* strip off file name */
	strncpy(temp, request->req.open.fname, MAXPATHLEN);
	length = get_parent(temp);
	/* if length<0, CWD is being used and directory permissions will 
	 * not be checked pursuant to the UNIX method */
	if (length >= 0) {
		/* check for permissions to write to directory */
		if (meta_access(0, temp, request->uid, request->gid, X_OK) 
			< 0) {
			perror("md_unlink: meta_access");
			return(-1);
		}
	}

	/* Read metadata file */	
	if (meta_read(fd, &meta1) < 0) {
		perror("md_unlink: meta_read");
		meta_close(fd);
		return (-1);
	}

	/* Check file ownership */
	/* No need to check permissions, the user code will do that before
	 * calling this function as in UNIX */
	if ((request->uid == 0) || (request->uid == meta1.u_stat.st_uid)) {
		if (meta_unlink(request->req.unlink.fname) < 0) {
			perror ("md_unlink: meta_unlink");
			meta_close(fd);
			return(-1);
		}
	}
	else {
		fprintf(stderr, "md_unlink: permission check failed\n");
		errno = EPERM;
		meta_close(fd);
		return(-1);
	}		
			
	*metar_p = meta1;
	return(fd);
}
