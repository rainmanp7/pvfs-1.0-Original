/* pvfs_write.c copyright (c) 1996 Rob Ross, all rights reserved.
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

static char pvfs_write_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_write.c,v 1.4 2000/10/30 14:44:49 rbross Exp $";

#include <lib.h>

extern fdesc_p pfds[];
extern jlist_p active_p;
extern sockset socks;

int unix_write(int fd, char *buf, size_t count);

#define PCOUNT pfd_p->fd.meta.p_stat.pcount

int pvfs_write(int fd, char *buf, size_t count)
{
	int i;
	int64_t size = 0;
	fdesc_p pfd_p = pfds[fd];

	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  

	if (!pfd_p || pfd_p->fs == FS_UNIX) return(unix_write(fd, buf, count));
	if (pfd_p->fs == FS_PDIR) return(unix_write(fd, buf, count));
#ifdef STRICT_FDESC_CHECK
	if (do_fdesc_check(fd) < 0) {
		fprintf(stderr, "check failed at beginning of pvfs_write()\n");
	}
#endif

	for (i = 0; i < PCOUNT; i++) {
		pfd_p->fd.iod[i].ack.dsize  = 0;
		pfd_p->fd.iod[i].ack.status = 0;
	}	

	/* build jobs, including requests and acks */
	if (build_rw_jobs(pfd_p, buf, count, IOD_RW_WRITE) < 0) {
		ERR("pvfs_write: build_rw_jobs failed\n");
		return(-1);
	}
	
	/* send requests; receive data and acks */
	while (!jlist_empty(active_p)) {
		if (do_jobs(active_p, &socks, -1) < 0) {
			PERROR("pvfs_write: do_jobs");
			return(-1);
		}
	}
	LOG("pvfs_write: finished with write jobs\n");
	for (i=0; i < PCOUNT; i++) {
		if (pfd_p->fd.iod[i].ack.status) {
			LOG1("pvfs_write: non-zero status returned from iod %d\n", i);
			/* this is likely to be a ENOSPC on one node */
			errno = pfd_p->fd.iod[i].ack.eno;
			return(-1);
		}
		size += pfd_p->fd.iod[i].ack.dsize;
	}
	/* update modification time meta data */
	pfd_p->fd.meta.u_stat.st_mtime = time(NULL);
	pfd_p->fd.off += size;
	LOG2("pvfs_write: completed %Ld bytes; new offset = %Ld\n", size,
	 	pfd_p->fd.off);

#ifdef STRICT_FDESC_CHECK
	if (do_fdesc_check(fd) < 0) {
		fprintf(stderr, "check failed at end of pvfs_write()\n");
	}
#endif

	return(size);
}

#define OFFSET req.req.rw.off
#define FSIZE  req.req.rw.fsize
#define GSIZE  req.req.rw.gsize
#define LSIZE  req.req.rw.lsize
#define GCOUNT req.req.rw.gcount
#define STRIDE req.req.rw.stride

int unix_write(int fd, char *buf, size_t count)
{
	fdesc_p fd_p = pfds[fd];
	ireq req;
	int off, ret;
	char *orig_buf = buf;

	if (!fd_p || !fd_p->part_p) return write(fd, buf, count);

	build_rw_req(fd_p, count, &req, IOD_RW_WRITE, 0);

	off = OFFSET;

	if (FSIZE) {
		if (lseek(fd, off, SEEK_SET) < 0) return(-1);
		if ((ret = write(fd, buf, FSIZE)) < 0) return(-1);
		buf += ret;
		if (ret < FSIZE) /* this is all we're writing */ {
			fd_p->fd.off += buf-orig_buf;
			return(buf-orig_buf);
		}
		off += FSIZE + STRIDE - GSIZE;
	}
	while (GCOUNT-- > 0) {
		if (lseek(fd, off, SEEK_SET) < 0) return(-1);
		if ((ret = write(fd, buf, GSIZE)) < 0) return(-1);
		buf += ret;
		if (ret < GSIZE) /* this is all we're writing */ {
			fd_p->fd.off += buf-orig_buf;
			return(buf-orig_buf);
		}
		off += STRIDE;
	}
	if (LSIZE) {
		if (lseek(fd, off, SEEK_SET) < 0) return(-1);
		if ((ret = write(fd, buf, LSIZE)) < 0) return(-1);
		buf += ret;
	}
	fd_p->fd.off += buf-orig_buf;
	return(buf-orig_buf);
}

