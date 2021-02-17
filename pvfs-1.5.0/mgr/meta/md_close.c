/*
 * md_close.c copyright (c) 1996 Rob Ross, all rights reserved.
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

static char md_close_c[] = "$Header: /projects/cvsroot/pvfs/mgr/meta/md_close.c,v 1.5 2001/01/15 21:11:59 rbross Exp $";
/*This file contains the close metadata file call for the PVFS*/
/*File assumes request has been allocated */

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <linux/types.h>
#include <sys/timeb.h>
#include <fcntl.h>
#include <unistd.h>
#include <meta.h>
#include <req.h>

extern int errno;

int md_close(mreq_p req, char *fname)
{
	time_t ctime;
	fmeta meta;
	int fd;

	/* get fd for meta file */
	if ((fd = meta_open(fname, O_RDWR)) < 0) {
		perror("md_close: meta_open");
		return(-1);
	}

	/* get metadata */
	if (meta_read(fd, &meta) < 0) {
		perror("md_close: meta_read");
		return(-1);
	}

	/* seek back to position 0 */
	if (lseek(fd, 0, SEEK_SET) < 0) return(-1);

	/* get current time for meta stats */
	if ((ctime = time(NULL)) < 0) {
		perror("md_close: time");
		return(-1);
	}

	/* update the modification and access times to whatever the mgr tells
	 * us
	 */
	meta.u_stat.st_atime = req->req.close.meta.u_stat.st_atime;
	meta.u_stat.st_mtime = req->req.close.meta.u_stat.st_mtime;
	/* don't change the creation time. */

/* Write metadata back */
	if (meta_write(fd, &meta, sizeof(fmeta)) < (int) sizeof(fmeta)) {
		perror("md_close: meta_write.");
		return(-1);
	}	

/* Close metadata file */
	if (meta_close(fd) < 0) {
		perror ("File close");
		return(-1);
	}
	
	return (0);

}	
