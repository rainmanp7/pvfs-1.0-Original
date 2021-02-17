/* pvfs_utime.c copyright (c) 1996 Rob Ross and Matt Cettei,
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

static char pvfs_utime_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_utime.c,v 1.5 2000/10/30 14:44:49 rbross Exp $";

#include <utime.h>
#include <sys/types.h>
#include <sys/param.h>
#include <lib.h>

extern int errno;
extern int pvfs_checks_disabled;

int pvfs_utime(const char *filename, struct utimbuf *buf)
{
	mreq request;
	mack ack;
	struct sockaddr *saddr;
	char *fn;
	int64_t fs_ino;

	/* check if this file is pvfs */
	if (pvfs_checks_disabled || pvfs_detect(filename, &fn, &saddr, &fs_ino) < 1)
	{
		return utime(filename, buf);
	}

	/* prepare request for manager */
   request.uid = getuid();
   request.gid = getgid();
   request.type = MGR_UTIME;
	request.dsize = strlen(fn);
   request.req.utime.fname = 0;

	/* if buf is null, set to current time */
	if (buf) {
		request.req.utime.buf.actime = buf->actime; /* access time */
		request.req.utime.buf.modtime = buf->modtime; /* modification time */
	}
	else {
		request.req.utime.buf.actime = time(NULL);
		request.req.utime.buf.modtime = request.req.utime.buf.actime;
	}
		
   /* Send request to mgr */
	if (send_mreq_saddr(saddr, &request, fn, &ack) < 0) {
      PERROR("send_mreq");
      return(-1);
   }
	return(0);
}
