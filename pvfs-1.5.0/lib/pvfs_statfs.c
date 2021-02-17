/* copyright (c) 1999-2000 Clemson University, all rights reserved.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as published
 *	by the Free Software Foundation; either version 2 of the License, or
 *	any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *	General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *	Contact:  Rob Ross  	rbross@parl.eng.clemson.edu
 */


static char pvfs_statfs_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_statfs.c,v 1.5 2000/10/30 14:44:49 rbross Exp $";

#include <lib.h>
#include <meta.h>
#include <sys/vfs.h>
#include <linux/ext2_fs.h>
#include <pvfs_config.h>

extern int errno;
extern fdesc_p pfds[];
extern int pvfs_checks_disabled;

int pvfs_fstatfs(int fd, struct statfs *buf)
{
	/* check for badness */
	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  
	/* check for UNIX */
	if (!pfds[fd] || pfds[fd]->fs==FS_UNIX) {
		return fstatfs(fd, buf);
	}

	if (pfds[fd]->fs == FS_PDIR) {
		buf->f_type = PVFS_SUPER_MAGIC;
		buf->f_bsize = DEFAULT_SSIZE;
		buf->f_blocks = -1;
		buf->f_bfree = -1;
		buf->f_bavail = -1;
		buf->f_files = -1;
		buf->f_ffree = -1;
		/* don't know what to do with fsid...leaving it be... */
		buf->f_namelen = EXT2_NAME_LEN;
		buf->f_spare[0] = -1;
	}
	if (pfds[fd]->fs == FS_PVFS) {
		buf->f_type = PVFS_SUPER_MAGIC;
		buf->f_bsize = pfds[fd]->fd.meta.p_stat.ssize; /* return stripe size */
		buf->f_blocks = -1;
		buf->f_bfree = -1;
		buf->f_bavail = -1;
		buf->f_files = -1;
		buf->f_ffree = -1;
		/* don't know what to do with fsid...leaving it be... */
		buf->f_namelen = EXT2_NAME_LEN;
		buf->f_spare[0] = -1;
	}

	return(0);
}

int pvfs_statfs(char *path, struct statfs *buf)
{
	int i, ret, myerr;
	mreq request;
	mack ack;
	char *fn;

	struct sockaddr *saddr;
	int64_t fs_ino;

	if (pvfs_checks_disabled || pvfs_detect(path, &fn, &saddr, &fs_ino) < 1)
	{
		/* not PVFS */
		return statfs(path, buf);
	}

	request.dsize = strlen(fn);
	request.uid = getuid();
	request.gid = getgid();
	request.type = MGR_STATFS;

	if (send_mreq_saddr(saddr, &request, fn, &ack) < 0) {
		myerr = errno;
		PERROR("pvfs_statfs: send_mreq");
		errno = myerr;
		return -1;
	}

	/* send_mreq_saddr() checks return values from mgr, so it succeeded */
	buf->f_type = PVFS_SUPER_MAGIC;
	buf->f_bsize = DEFAULT_SSIZE; /* return stripe size */
	buf->f_blocks = (ack.ack.statfs.tot_bytes / (int64_t) DEFAULT_SSIZE);
	buf->f_bfree = (ack.ack.statfs.free_bytes / (int64_t) DEFAULT_SSIZE);
	buf->f_bavail = (ack.ack.statfs.free_bytes / (int64_t) DEFAULT_SSIZE);
	buf->f_files = ack.ack.statfs.tot_files;
	buf->f_ffree = ack.ack.statfs.free_files;
	/* don't know what to do with fsid...leaving it be... */
	buf->f_namelen = ack.ack.statfs.namelen;
	buf->f_spare[0] = -1;
	

	return(0);
}


