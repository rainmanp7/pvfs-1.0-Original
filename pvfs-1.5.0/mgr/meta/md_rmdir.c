/* md_rmdir.c copyright (c) 1996 Clemson University, all rights reserved.
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

static char md_rmdir_c[] = "$Header: /projects/cvsroot/pvfs/mgr/meta/md_rmdir.c,v 1.3 2000/10/30 14:51:58 rbross Exp $";
/*This file contains the chown metadata file call for the PVFS*/
/*Assumes that space for request has been allocated. */

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <fcntl.h>
#include <linux/types.h>
#include <unistd.h>
#include <meta.h>
#include <req.h>
#include <dirent.h>

extern int errno;

int md_rmdir(mreq_p req_p)
{
	DIR *dp;
	struct dirent *dep;
	char temp[MAXPATHLEN];
	int length;

	/* check for permissions */
   strncpy(temp, req_p->req.rmdir.fname, MAXPATHLEN);
	length = get_parent(temp);
   /* if length<0, CWD is being used and directory permissions will
    * not be checked pursuant to the UNIX method */
   if (length >= 0) {
      /* check for permissions to write to directory */
      if (meta_access(0, temp, req_p->uid, req_p->gid, X_OK | W_OK)
         < 0) {
         perror("md_rmdir: meta_access");
         return(-1);
      }
   }

	if (!(dp = opendir(req_p->req.rmdir.fname))) return(-1);

	/* read the directory and make sure that our dot files are all that
	 * is left
	 */
	while(dep = readdir(dp)) {
		if (strcmp(dep->d_name,".pvfsdir") &&
		    strcmp(dep->d_name,".") &&
		    strcmp(dep->d_name,"..")) {
			errno = ENOTEMPTY;
			return(-1);
		}
	}
	closedir(dp);
	/* remove the dotfile */
	strncpy(temp, req_p->req.rmdir.fname, MAXPATHLEN);
	strcat(temp,"/.pvfsdir"); /* should check bounds... */
	if (unlink(temp)) {
		return(-1);
	}
	/* remove the directory */
	if (rmdir(req_p->req.rmdir.fname)) {
		return(-1);
	}
	/* return success */
	return(0);
}
