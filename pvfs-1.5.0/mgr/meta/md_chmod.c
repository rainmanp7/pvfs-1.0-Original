/* md_chmod.c copyright (c) 1996 Clemson University, all rights reserved.
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

static char md_chmod_c[] = "$Header: /projects/cvsroot/pvfs/mgr/meta/md_chmod.c,v 1.3 2001/01/15 21:11:59 rbross Exp $";
/*This file contains the chmod metadata file call for the PVFS*/

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

int md_chmod(mreq_p request)
{
	fmeta meta1;
	int fd,i,mask,length, isdir=0;
	char temp[MAXPATHLEN];
	dmeta dir;

	/* Open file listed in request */
   if ((fd = meta_open(request->req.chmod.fname, O_RDWR)) < 0) {
		if (errno != EISDIR) {
			perror("md_chmod: meta_open");
			meta_close(fd);
			return (-1);
		}
		else isdir=1;
	}

	/* check execute permissions on directory */
	strncpy(temp, request->req.open.fname, MAXPATHLEN);
	length = get_parent(temp);
	/* length < 0 means CWD */
	if (length >= 0) {
		if (meta_access(0, request->req.chmod.fname, request->uid,
			request->gid, X_OK) < 0) {
			perror("md_chmod: meta_access");
			return(-1);
		}
	}
	
	/* Read metadata file */
	if (!isdir) {
		if ((i = meta_read(fd, &meta1)) < (int) sizeof(fmeta)) {
			perror("md_chmod: meta_read");
			meta_close(fd);
			return (-1);
		}
	}
	else { /* directory */
		get_dmeta(request->req.stat.fname, &dir);
		meta1.u_stat.st_uid = dir.dr_uid;
	}

	/* Change  if necessary */
	if ((request->uid != 0)&&(request->uid != meta1.u_stat.st_uid)) {
		errno = EPERM;
		perror("md_chmod");
		if (!isdir) meta_close(fd);
		return(-1);
	}

	/* had to add OR to get the high bits set right */
	meta1.u_stat.st_mode = request->req.chmod.mode | S_IFREG;

	/* Write metadata back */
	if (!isdir) {
		if ((i = meta_write(fd, &meta1, sizeof(fmeta))) < (int) sizeof(fmeta)) {
			perror("md_chmod: meta_write");
			meta_close(fd);
			return(-1);
		}
	}
	else { /* directory */
		dir.dr_mode = (meta1.u_stat.st_mode & ~S_IFREG) | S_IFDIR;
		put_dmeta(request->req.chmod.fname, &dir);
	}
	
	/* Close metadata file */
	if (!isdir) meta_close(fd);

	return (0);
}
