/*
 * md_mount.c copyright (c) 1996 Rob Ross, all rights reserved.
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
 * Contact:  Rob Ross  	rbross@parl.eng.clemson.edu
 * 
 */

static char md_mount_c[] = "$Header: /projects/cvsroot/pvfs/mgr/meta/md_mount.c,v 1.2 1999/08/13 23:26:24 rbross Exp $";
/* This file contains the mount metadata file call for the PVFS*/
/* Assumes request has already been allocated */

#include <errno.h>
#include <stdio.h>
#include <sys/mount.h>
#ifdef FS_H_IS_OK
#include <linux/fs.h>
#endif
#include <sys/types.h>
#include <sys/file.h>
#include <linux/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <meta.h>
#include <req.h>

extern int errno;

int md_mount(mreq_p request)
{
	fmeta meta1;
	int size = sizeof(fmeta);
	int fd;
	int i;
	int mode, flag;

	/* Check permission. Must be root */
	if (request->uid != 0) {
		perror("Mount permission denied.");
		errno = EPERM;
		return(-1);
	}	
	
	/* Read dot file */
	flag = O_RDONLY;
	if ((fd = open(request->req.mount.fsname, flag)) < 0) {
		perror("Dot File open.");
		return (-1);
	}	
	
	if ((i = read(fd, &meta1, size)) < 0) {
		perror("Dot file read");
		return(-1);
	}	
	
	/* Close dot file */
	if ((i = close(fd)) < 0) {
		perror("File close");
		return(-1);
	}

	
	/* Do acknowledge and return */
	return (0);
}
