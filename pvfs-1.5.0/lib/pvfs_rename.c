/* pvfs_rename.c copyright (c) 1996 Rob Ross and Matt Cettei,
   all rights reserved.

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

static char pvfs_unlink_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_rename.c,v 1.6 2000/10/30 14:44:49 rbross Exp $";

#include <sys/param.h>
#include <lib.h>
#include <meta.h>

extern int errno;
extern int pvfs_checks_disabled;

int pvfs_rename(const char *oldname, const char *newname)
{
	int j,i, s, index;
	mreq request;
	mack ack;
	struct sockaddr *saddr;
	char *fn = NULL;
	int64_t newfs_ino, oldfs_ino;
	char bothnames[MAXPATHLEN+MAXPATHLEN+2];
	int len, bothlen;

	if (!oldname || !newname) {
		errno = EFAULT;
		return(-1);
	}

	if (pvfs_checks_disabled) return rename(oldname, newname);

	/* check to see if file is a pvfs file; if so, we get a pointer to a
	 * static region with the canonicalized name back in fn.  This will
	 * get overwritten on the next call.
	 */
	if ((j = pvfs_detect(oldname, &fn, &saddr, &oldfs_ino)) < 0) {
		errno = ENOENT;
		return -1;
	}
	if (fn != NULL) strncpy(bothnames, fn, MAXPATHLEN);

	if ((i = pvfs_detect(newname, &fn, &saddr, &newfs_ino)) < 0) {
		errno = ENOENT;
		return -1;
	}
	if (j > 0 && i > 0) {
		/* get both strings into one happy buffer */
		len = strlen(bothnames);
		bothlen = strlen(fn) + len + 1; /* don't count final terminator */
		strncpy(&bothnames[len+1], fn, MAXPATHLEN);
	}

	if (i==0 && j==0) {
		/* nonpvfs stuff */
		return rename(oldname, newname);
	}
	if (i == 0 || j == 0 || (newfs_ino != oldfs_ino)) {
		/* across file systems, error */
		errno = EXDEV; /* not on same file system! */
		return(-1);
	}

	/* Prepare request for file system  */
	request.uid = getuid();
	request.gid = getgid();
	request.type = MGR_RENAME;
	request.req.rename.oname = 0;
	request.req.rename.nname = 0;
	request.dsize = bothlen;

	/* Send request to mgr */	
	if ((s = send_mreq_saddr(saddr, &request, bothnames, &ack)) < 0) {
		int myeno = errno;
		PERROR("send_mreq");
		errno = myeno;
		return(-1);
	}

	return(0);
}
