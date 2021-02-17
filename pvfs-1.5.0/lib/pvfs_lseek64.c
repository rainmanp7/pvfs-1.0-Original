/* pvfs_llseek.c copyright (c) 1995 Clemson University, all rights reserved.
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

/* PVFS_LLSEEK.C - parallel seek call, 64-bit
 *
 * Upon successful completion, pvfs_lseek() returns the current offset, 
 * measured in bytes from the beginning of the file.  If it fails, 
 * -1 is returned and the offset is not changed.
 */

static char pvfs_llseek_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_lseek64.c,v 1.8 2000/12/14 15:59:52 rbross Exp $";

/* UNIX INCLUDE FILES */
#include <errno.h>
#include <unistd.h>

/* PVFS INCLUDE FILES */
#include <lib.h>

/* GLOBALS */
extern int errno;
extern fdesc_p pfds[];
extern sockset socks;
extern jlist_p active_p;

int64_t pvfs_lseek64(int fd, int64_t off, int whence);

/* FUNCTIONS */
int64_t pvfs_llseek(int fd, int64_t off, int whence)
{
	return(pvfs_lseek64(fd, off, whence));
}

int64_t pvfs_lseek64(int fd, int64_t off, int whence)
{
	int i, val, myeno;
	int64_t result;
	ireq iodreq;

	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  

	/* don't call llseek() for directories any more */
	if (!pfds[fd] || pfds[fd]->fs == FS_UNIX) {
		if (pfds[fd]) /* gotta keep our info up to date */ {
#ifdef __ALPHA__
			val = lseek(fd, off, whence);
#else
			val = syscall(SYS__llseek, fd, (u_int32_t)(off >> 32),
				(u_int32_t)(off & 0xffffffff), &result, whence);
#endif
			pfds[fd]->fd.off = val ? val : result;
			return(pfds[fd]->fd.off);
		}
		else {
#ifdef __ALPHA__
			val = lseek(fd, off, whence);
#else
			val = syscall(SYS__llseek, fd, (u_int32_t)(off >> 32),
				(u_int32_t)(off & 0xffffffff), &result, whence);
#endif
			return(val ? val : result);
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
			/* this isn't implemented for directories as of yet */
			if (pfds[fd]->fs == FS_PDIR) {
				errno = EINVAL;
				return(-1);
			}
			/* find the actual end of the file */
			/* HERE WE NEED TO TALK TO THE IODS AND GET THE FILE SIZE */
			iodreq.majik_nr = IOD_MAJIK_NR;
			iodreq.type = IOD_STAT;
			iodreq.dsize = 0;
			iodreq.req.stat.fs_ino = pfds[fd]->fd.meta.fs_ino;
			iodreq.req.stat.f_ino = pfds[fd]->fd.meta.u_stat.st_ino;
			if (build_simple_jobs(pfds[fd], &iodreq) < 0) {
				PERROR("building job");
				return(-1);
			}

			while (!jlist_empty(active_p)) {
				if (do_jobs(active_p, &socks, -1) < 0) {
					myeno = errno;
					ERR("pvfs_llseek: do_jobs failed\n");
					errno = myeno;
					return(-1);
				}
			}
			for (i=0; i < pfds[fd]->fd.meta.p_stat.pcount; i++) {
				if (pfds[fd]->fd.iod[i].ack.status) {
					errno = pfds[fd]->fd.iod[i].ack.eno;
					return(-1);
				}
			}
			result = off;
			for (i=0; i < pfds[fd]->fd.meta.p_stat.pcount; i++) {
				result += pfds[fd]->fd.iod[i].ack.ack.stat.fsize;
			}
			if (result >= 0) return(pfds[fd]->fd.off = result);
		/* let error here and default drop through...behavior is the same */
	}
	errno = EINVAL;
	return(-1);
} /* end of PVFS_LSEEK64() */

