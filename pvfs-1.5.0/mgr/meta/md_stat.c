/* md_stat.c copyright (c) 1996 Rob Ross and Matt Cettei, all rights reserved.
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

static char md_stat_c[] = "$Header: /projects/cvsroot/pvfs/mgr/meta/md_stat.c,v 1.8 2001/01/15 21:11:59 rbross Exp $";
/* This file contains the fstat metadata file call for the PVFS	*/
/* Also assumes space for request has been allocated 					*/

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

int md_stat(char *name, mreq_p request, fmeta_p metar_p)
{
	fmeta meta1;
	int fd, length;
	dmeta dir;
	char temp[MAXPATHLEN];

	/* zero the metadata */
	memset(metar_p, 0, sizeof(fmeta));

	/* Open metadata file */	
	if ((fd = meta_open(name, O_RDONLY)) < 0) {
		if (errno != ENOENT) perror("md_stat, meta_open");
		return (-1);
	}

	/* Read metadata file - read is done before access to determine if its a
	 * directory or not (if its directory we need to do execute permission
	 * checking on the entire directory and not just the parent)  */	
	if (meta_read(fd, &meta1) < (int) sizeof(fmeta)) {
		if (errno != EISDIR) {
			perror("md_stat: meta_read");
			meta_close(fd); /* try to close */
			return(-1);
		}
		else /* directory - just return the normal stats */ {
			/* check entire directory path for execute permissions */
			/* we know its a directory, so setting 0 here inplace of fd allows
			 * meta_access to forgo that check */
			if (meta_access(0, name, request->uid,
				request->gid, X_OK) < 0) {
				perror("md_stat: meta_access");
				meta_close(fd); /* try to close */
				return(-1);
			}
			/* get stats and fill in pvfs specific stuff */
			if (fstat(fd, &(metar_p->u_stat)) < 0) {
				perror("md_stat: fstat");
				meta_close(fd); /* try to close */
				return(-1);
			}
			/* Close metadata file */
			if (meta_close(fd) < 0) {
				perror("md_stat: meta_close");
				return(-1);
			}	
			if (get_dmeta(name, &dir) < 0) {
				perror("md_stat: get_dmeta");
				return(-1);
			}
			metar_p->u_stat.st_mode =
				S_IFDIR | ((S_IRWXO | S_IRWXG | S_IRWXU) & dir.dr_mode);

			metar_p->u_stat.st_uid = dir.dr_uid;
			metar_p->u_stat.st_gid = dir.dr_gid;
			return(0);
		}
	}

	/* ok, we know its a file */
	/* check execute permission on parent directory */
	strncpy(temp, name, MAXPATHLEN);
	length = get_parent(temp); /* strip off filename */
	/* if length<0, CWD is being used and directory permissions will
	 * not be checked pursuant to the UNIX method */
	if (length >= 0) {
		/* check for permissions to read from directory */
		if (meta_access(0, temp, request->uid, request->gid, X_OK) < 0) {
			perror("md_stat: meta_access");
			meta_close(fd); /* try to close */
			return(-1);
		}
	}

	/* Close metadata file */
	if (meta_close(fd) < 0) {
		perror("md_stat: meta_close");
		return(-1);
	}	

	*metar_p = meta1;
	metar_p->u_stat.st_mode =
	S_IFREG|((S_IRWXO|S_IRWXG|S_IRWXU) & metar_p->u_stat.st_mode);
	return (0);
}
