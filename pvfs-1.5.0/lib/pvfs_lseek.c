/* pvfs_lseek.c copyright (c) 1995 Clemson University, all rights reserved.
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

/* PVFS_LSEEK.C - parallel seek call
 *
 * pvfs_lseek() moves the read-write file offset on a parallel file in a 
 * manner similar to the unix lseek() call.  It accepts 3 parameters: a
 * parallel file descriptor, an offset value, and a "whence" parameter 
 * used to specify how to interpret the offset.
 *
 * whence may have 1 of two values, SEEK_SET or SEEK_CUR.  SEEK_SET
 * specifies to set the offset to the value given.  SEEK_CUR specifies that
 * the offset value should be added to the current offset.  Note that the
 * offset value may be negative.
 *
 * update: whence may now have the value of SEEK_END.  I finally got around
 * to fixing it.
 *
 * Upon successful completion, pvfs_lseek() returns the current offset, 
 * measured in bytes from the beginning of the file.  If it fails, 
 * -1 is returned and the offset is not changed.
 */

static char pvfs_lseek_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_lseek.c,v 1.4 2000/10/30 14:44:49 rbross Exp $";

/* UNIX INCLUDE FILES */
#include <errno.h>
#include <unistd.h>

/* PVFS INCLUDE FILES */
#include <lib.h>

/* GLOBALS */
extern int errno;
extern fdesc_p pfds[];

/* FUNCTIONS */
int pvfs_lseek(int fd, int off, int whence)
{
	int i, val;
	struct stat filestat;

	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  

	/* we're handling directories within PVFS now, so don't call lseek() */
	/* for them. */
	if (!pfds[fd] || pfds[fd]->fs == FS_UNIX) {
		if (pfds[fd]) /* gotta keep our info up to date */ {
			return(pfds[fd]->fd.off = lseek(fd, off, whence));
		}
		else {
			return lseek(fd, off, whence);
		}
	}

	switch(whence) {
		case SEEK_SET:
			if (off >= 0) return(pfds[fd]->fd.off = off);
			errno = EINVAL;
			return(-1);
		case SEEK_CUR:
			if (pfds[fd]->fd.off + off >= 0) return(pfds[fd]->fd.off += off);
			errno = EINVAL;
			return(-1);
		case SEEK_END:
			/* for directories, this doesn't work yet */
			if (pfds[fd]->fs == FS_PDIR) {
				errno = EINVAL;
				return(-1);
			}
			/* find the actual end of the file */
			if ((i = pvfs_fstat(fd, &filestat)) < 0) {
				PERROR("Getting file size");
				return(-1);
			}
			if (off + filestat.st_size >= 0)
				return(pfds[fd]->fd.off = off + filestat.st_size);
		/* let error here and default drop through...behavior is the same */
	}
	errno = EINVAL;
	return(-1);
} /* end of PVFS_LSEEK() */
