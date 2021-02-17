/*
 * md_chown.c copyright (c) 1996 Rob Ross, all rights reserved.
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

static char md_chown_c[] = "$Header: /projects/cvsroot/pvfs/mgr/meta/md_chown.c,v 1.4 2001/01/15 21:11:59 rbross Exp $";

/*This file contains the chown metadata file call for the PVFS*/
/*Assumes that space for request has been allocated. */

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
#include <grp.h>
#include <pwd.h>
#include <debug.h>

extern int errno;

int md_chown(mreq_p request)
{
	fmeta meta1;
	int fd,length,i;
	char temp[MAXPATHLEN];
	dmeta dir;
	struct passwd *pw;
	struct group *grp;


	if ((fd = meta_open(request->req.chown.fname, O_RDWR)) < 0) {
		if (errno != EISDIR) {
			perror("md_chown: meta_open");
			return (-1);
		}
	}

   /* check execute permissions on directory */
   strncpy(temp, request->req.open.fname, MAXPATHLEN);
	length = get_parent(temp);
	/* length < 0 means CWD */
   if (length >= 0) {
		if (meta_access(0, temp, request->uid, request->gid, X_OK) < 0) {
			perror("md_open: meta_access");
			return(-1);
		}
	}
	
	/* Read metadata file */	
	if (fd >= 0) {
		if (meta_read(fd, &meta1) < (int) sizeof(fmeta)) {
			perror("md_chown: meta_read");
			meta_close(fd);
			return (-1);
		}
		lseek(fd, 0, SEEK_SET);
	}
	else { /* directory */
		get_dmeta(request->req.stat.fname, &dir);
		meta1.u_stat.st_uid = dir.dr_uid;
		meta1.u_stat.st_gid = dir.dr_gid;
	}

	/* Change owner if necessary */
	if (request->req.chown.owner >= 0) {
		/* root can change this or if uid = file owner uid,
		 * let group chown be performed */
		if ((request->uid != 0) && (request->uid != request->req.chown.owner)) {
			errno = EPERM;
			perror("md_chown: permission denied");
			if (fd >= 0)
				meta_close(fd);
			return(-1);
		}
		meta1.u_stat.st_uid = request->req.chown.owner;
	}
	
	/* Change group if necessary */
	if (request->req.chown.group >= 0) {
		if (request->uid) /* not root, check perms */ {
			/* grab group info from /etc/group or wherever */
			if (!(grp = getgrgid(request->req.chown.group))) {
				PERROR("md_chown: getgrgid");
				if (fd >= 0) meta_close(fd);
				errno = EINVAL;
				return(-1);
			}
			/* see if user is a member in /etc/group or /etc/passwd */
			if (!(pw = getpwuid(request->uid))) {
				PERROR("md_chown: getpwuid");
				if (fd >= 0) meta_close(fd);
				errno = EINVAL;
				return(-1);
			}

			for(i=0;grp->gr_mem[i] && strcmp(pw->pw_name, grp->gr_mem[i]); i++);
			if (!grp->gr_mem[i] && pw->pw_gid != request->req.chown.group) {
				ERR("md_chown: change group permission denied");
				if (fd >= 0) meta_close(fd);
				errno = EPERM;
				return(-1);
			}
		}
		meta1.u_stat.st_gid = request->req.chown.group;
	}	

	/* Write metadata back */
	if (fd >= 0) {
		if (meta_write(fd, &meta1, sizeof(fmeta)) < (int) sizeof(fmeta)) {
			perror("md_chown: meta_write.");
			meta_close(fd);
			return(-1);
		}
	}
	else { /* directory */
		dir.dr_uid = meta1.u_stat.st_uid;
		dir.dr_gid = meta1.u_stat.st_gid;
		put_dmeta(request->req.stat.fname, &dir);
	}

	/* Close metadata file */
	if (fd >= 0 && (meta_close(fd)) < 0) {
		perror("md_chown: meta_close");
		return(-1);
	}	

	/* Do acknowledge and return */
	return (0);
}
