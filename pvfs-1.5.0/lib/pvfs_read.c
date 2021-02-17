/*
 * pvfs_read.c copyright (c) 1996 Rob Ross, all rights reserved.
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

/*
 * PVFS_READ.C - function to perform read access to files
 *
 */

static char pvfs_read_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_read.c,v 1.4 2000/10/30 14:44:49 rbross Exp $";

#include <lib.h>

extern fdesc_p pfds[];
extern jlist_p active_p;
extern sockset socks;

int unix_read(int fd, char *buf, size_t count);

#define PCOUNT pfd_p->fd.meta.p_stat.pcount

int pvfs_read(int fd, char *buf, size_t count)
{
	int i;
	int64_t size = 0;
	fdesc_p pfd_p = pfds[fd];

	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}  

	if (!pfd_p || pfd_p->fs == FS_UNIX) return(unix_read(fd, buf, count));
	if (pfd_p->fs == FS_PDIR) return(unix_read(fd, buf, count));

#ifdef STRICT_FDESC_CHECK
	if (do_fdesc_check(fd) < 0) {
		fprintf(stderr, "check failed at start of pvfs_read()\n");
	}
#endif

	for (i = 0; i < PCOUNT; i++) {
		pfd_p->fd.iod[i].ack.status = 0;
		pfd_p->fd.iod[i].ack.dsize  = 0;
	}	


	/* build jobs, including requests and acks */
	if (build_rw_jobs(pfd_p, buf, count, IOD_RW_READ) < 0) {
		ERR("build_rw_jobs failed in pvfs_read\n");
		return(-1);
	}
	
	/* send requests; receive data and acks */
	while (!jlist_empty(active_p)) {
		if (do_jobs(active_p, &socks, -1) < 0) {
			PERROR("do_jobs");
			return(-1);
		}
	}
	/* ACKS SHOULD HAVE SIZE OF DATA SENT/RECEIVED */
	/* check acks to make sure things worked right */
	for (i=0; i < PCOUNT; i++) {
		if (pfd_p->fd.iod[i].ack.status) {
			ERR1("pvfs_read: non-zero status returned from iod %d\n", i);
			errno = pfd_p->fd.iod[i].ack.eno;
			return(-1);
		}
		size += pfd_p->fd.iod[i].ack.dsize;
	}
	pfd_p->fd.off += size;
	pfd_p->fd.meta.u_stat.st_atime = time(NULL);
	LOG2("pvfs_read: completed %Ld bytes; new offset = %Ld\n", size,
		pfd_p->fd.off);

#ifdef STRICT_FDESC_CHECK
	if (do_fdesc_check(fd) < 0) {
		fprintf(stderr, "check failed at end of pvfs_read()\n");
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

int unix_read(int fd, char *buf, size_t count)
{
	fdesc_p fd_p = pfds[fd];
	ireq req;
	int off, ret;
	char *orig_buf = buf;

	if (!fd_p || !fd_p->part_p)
		return read(fd, buf, count);

	build_rw_req(fd_p, count, &req, IOD_RW_READ, 0);

	/* Notes: request parameters are in terms of the WHOLE file,
	 * whereas the offset stored in fd_p->fd.off is in terms of the
	 * PARTITIONED file.
	 *
	 * Because of this, we're just adding to the partition offset
	 * however much data gets read, while the offset into the whole
	 * file has to skip around a lot.
	 *
	 * We should be able to add a couple of checks to avoid some of
	 * these lseek()s.
	 */
	off = OFFSET;

	if (FSIZE) {
		if (lseek(fd, off, SEEK_SET) < 0) return(-1);
		if ((ret = read(fd, buf, FSIZE)) < 0) return(-1);
		buf += ret;
		if (ret < FSIZE) /* this is all we're getting */ {
			fd_p->fd.off += buf-orig_buf;
			return(buf-orig_buf);
		}
		off += FSIZE + STRIDE - GSIZE;
	}
	while (GCOUNT-- > 0) {
		if (lseek(fd, off, SEEK_SET) < 0) return(-1);
		if ((ret = read(fd, buf, GSIZE)) < 0) return(-1);
		buf += ret;
		if (ret < GSIZE) /* this is all we're getting */ {
			fd_p->fd.off += buf-orig_buf;
			return(buf-orig_buf);
		}
		off += STRIDE;
	}
	if (LSIZE) {
		if (lseek(fd, off, SEEK_SET) < 0) return(-1);
		if ((ret = read(fd, buf, LSIZE)) < 0) return(-1);
		buf += ret;
	}
	fd_p->fd.off += buf-orig_buf;
	return(buf-orig_buf);
}
