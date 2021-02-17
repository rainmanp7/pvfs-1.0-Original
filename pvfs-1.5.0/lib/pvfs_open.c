/* pvfs_open.c copyright (c) 1996 R. Ross and M. Cettei, all rights reserved.
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

static char pvfs_open_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_open.c,v 1.10 2000/10/30 14:44:49 rbross Exp $";

/* This file contains PVFS library call for open.   		*/
/* Should work to create and open files 						*/

#include <lib.h>
#include <signal.h>

#include <sockset.h>

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

extern int errno;
extern sockset socks;
extern jlist_p active_p;


fdesc_p pfds[NR_OPEN] = {NULL};
int pvfs_checks_disabled=0;

static int send_open_req(const char* pathname, char *serverfilename,
	struct sockaddr *saddr, mreq_p);
int unix_open(const char *, int, mode_t, fpart_p);
int pvfs_open(const char*, int, mode_t, void *, void *);

int pvfs_open64(const char* pathname, int flag, mode_t mode,
	void *ptr1, void *ptr2)
{
	return(pvfs_open(pathname, flag, mode | O_LARGEFILE, ptr1, ptr2));
}

/* pvfs_open() - PVFS open command.
 * pathname - file to be opened
 * flag - flags (O_RDONLY, etc.) to be used in open
 * mode - if creating file, mode to be used
 * ptr1, ptr2 - these point to metadata and partition information,
 *    if provided.  The flags (O_META and O_PART) must be set if one
 *    is to be used, and if both metadata and partition information
 *    are provided then the metadata pointer MUST PRECEDE the partition
 *    information.
 */
int pvfs_open(const char* pathname, int flag, mode_t mode,
	void *ptr1, void *ptr2)
{
	int 		i, fd, myerr;
	mreq 		req;
	pvfs_filestat_p meta_p;
	fpart_p     part_p;
	struct sockaddr *saddr;
	char *fn;
	int64_t fs_ino;

	atexit((void *)pvfs_exit);

	if (!pathname) {
		errno = EFAULT;
		return(-1);
	}
	if (!pvfs_checks_disabled) {
		/* Check to see if file is a PVFS file */
		i = pvfs_detect(pathname, &fn, &saddr, &fs_ino);
		if (i < 0 || i > 2) /* error */ {
			PERROR("Error finding file");
			return(-1);
		}
	}
	else i = 0;

	if (i == 0) /* not a PVFS file or directory */ {
		if (flag & O_PART) {
			if (flag & O_META) part_p = ptr2;
			else part_p = ptr1;
		}
		else part_p = NULL;
		return(unix_open(pathname, flag, mode, part_p));
	}
	if (i == 2) /* PVFS directory */ {
		if (flag & (O_WRONLY | O_RDWR)) {
			errno = EISDIR;
			return(-1);
		}

		if ((fd = open("/dev/null", O_RDONLY, 0)) < 0) {
			myerr = errno;
			PERROR("pvfs_open: error opening /dev/null instead of metafile");
			errno = myerr;
			return(-1);
		}

		if (!(pfds[fd] = (fdesc_p)malloc(sizeof(fdesc)))) {
			myerr = errno;
			close(fd);
			errno = myerr;
			return(-1);
		}

		/* save the file name in terms manager would understand */
		if (!(pfds[fd]->fn_p = (char *)malloc(strlen(fn)+1))) {
			myerr = errno;
			free(pfds[fd]);
			pfds[fd] = NULL;
			close(fd);
			errno = myerr;
			return(-1);
		}
		strcpy(pfds[fd]->fn_p, fn);

#ifdef STRICT_DESC_CHECK
		pfds[fd]->checkval = FDESC_CHECKVAL;
#endif
		pfds[fd]->fs = FS_PDIR;
		pfds[fd]->part_p    = NULL;
		pfds[fd]->dim_p     = NULL;
		pfds[fd]->fd.off    = 0;
		pfds[fd]->fd.grp_nr = -1;
		pfds[fd]->fd.flag   = flag;
		pfds[fd]->fd.cap    = 0;
		pfds[fd]->fd.ref    = 1;

		/* store the manager connection info */
		memcpy(&pfds[fd]->fd.meta.mgr, saddr, sizeof(struct sockaddr));
		return(fd);
	}

	/* otherwise we have a PVFS file... */
	/* Prepare request for file system */
	if (flag & O_META) {
		meta_p = ptr1;
		req.req.open.meta.p_stat.base = meta_p->base;
		req.req.open.meta.p_stat.pcount = meta_p->pcount;
		req.req.open.meta.p_stat.ssize = meta_p->ssize;
		req.req.open.meta.p_stat.soff  = meta_p->soff;
		req.req.open.meta.p_stat.bsize = meta_p->bsize;
	}
	else {
		req.req.open.meta.p_stat.base = -1;
		req.req.open.meta.p_stat.pcount = -1;
		req.req.open.meta.p_stat.ssize = -1;
		req.req.open.meta.p_stat.soff  = 0;
		req.req.open.meta.p_stat.bsize = -1;
	}

	req.uid = getuid();
	req.gid = getgid();
	req.type = MGR_OPEN;

	req.dsize = strlen(fn);
	req.req.open.fname = 0;
	req.req.open.flag = flag;
	req.req.open.mode  = mode;
	req.req.open.meta.fs_ino = fs_ino;
	req.req.open.meta.u_stat.st_uid = req.uid;
	req.req.open.meta.u_stat.st_gid = req.gid;
	req.req.open.meta.u_stat.st_mode = mode;

	if (flag & O_APPEND) 
		req.req.open.flag = flag & ~(O_APPEND);

	if ((fd = send_open_req(pathname, fn, saddr, &req)) < 0) return(-1);

	/* set partition (if provided) */
	if (flag & O_PART) {
		if (flag & O_META) part_p = ptr2;
		else part_p = ptr1;
		
		if (!(pfds[fd]->part_p = malloc(sizeof(fpart)))) {
			PERROR("pvfs_open: malloc");
			return(-1);
		}
		*(pfds[fd]->part_p) = *part_p;
	}

	/* open all IOD connections (if requested) */
#ifndef __ALWAYS_CONN__
	if (flag & O_CONN) {
#else
	if (1) {
#endif
		ireq iodreq;

		iodreq.majik_nr   = IOD_MAJIK_NR;
		iodreq.type       = IOD_NOOP;
		iodreq.dsize      = 0;

		/* build job to send reqs and recv acks to/from iods */
		if (!active_p) active_p = jlist_new();
		initset(&socks); /* clear out the socket set */

		if (build_simple_jobs(pfds[fd], &iodreq) < 0) {
			ERR("pvfs_open: build_simple_jobs failed...continuing\n");
			return(fd);
		}

		/* call do_job */
		while (!jlist_empty(active_p)) {
			if (do_jobs(active_p, &socks, -1) < 0) {
				ERR("pvfs_open: do_jobs failed...continuing\n");
				return(fd); /* we'll let it slide for now */
			}
		}
		/* don't bother looking for errors */
	}

	/* move to end of file (if necessary) */
	if (flag & O_APPEND) {
		pvfs_lseek(fd, 0, SEEK_END);
	}

	/* tell the sockset library to start with random sockets if enabled */
	randomnextsock(1);
	return(fd);
}

int pvfs_creat(const char* pathname, mode_t mode)
{
	int flags;
	
	/* call pvfs_open with correct flags            */
	flags  =  O_CREAT|O_WRONLY|O_TRUNC;
	return(pvfs_open(pathname, flags, mode, NULL, NULL)); 
}

/* send_open_req() - sends request to the manager, sets up FD structure,
 * returns file descriptor number
 *
 * servername is actually the filename in terms the server can
 * understand, not the name of the server.
 *
 * Returns -1 on error.
 */
static int send_open_req(const char* pathname, char *servername,
	struct sockaddr *saddr, mreq_p req_p)
{
	char send_req = 10;
	int fd, s, i;
	mack ack;

	/* Send request to mgr */
	for (;;) {
		s = send_mreq_saddr(saddr, req_p, servername, &ack);
		/* Check ack */
		if (s >= 0 && !ack.status) break; /* went through */

		/* problems... */
		errno = ack.eno;
		return(-1);
	}

	/* always just open /dev/null as a placeholder */
	if ((fd = open("/dev/null", O_RDONLY, 0)) < 0) {
		int myerr = errno;
		PERROR("pvfs_open: error opening /dev/null instead of metafile");
		errno = myerr;
		return(-1);
	}
	LOG1("Opening: %s\n", pathname);
	pfds[fd] = (fdesc_p)malloc(sizeof(fdesc) + sizeof(iod_info) * 
		(ack.ack.open.meta.p_stat.pcount - 1));
#ifdef STRICT_DESC_CHECK
	pfds[fd]->checkval  = FDESC_CHECKVAL;
#endif
	pfds[fd]->fs        = FS_PVFS; /* PVFS file type */
	pfds[fd]->fd.off    = 0;
	pfds[fd]->fd.grp_nr = -1;
	pfds[fd]->fd.flag   = req_p->req.open.flag;
	pfds[fd]->fd.meta   = ack.ack.open.meta;
	pfds[fd]->fd.cap    = ack.ack.open.cap;
	pfds[fd]->fd.ref    = 1;
	pfds[fd]->part_p    = NULL;
	pfds[fd]->dim_p     = NULL;
	pfds[fd]->fn_p      = NULL;

	/* Ought to initialize part_p and dim_p of fdesc */	
   if ((i = brecv(s, pfds[fd]->fd.iod, ack.dsize)) < 0) {
      PERROR("pvfs_open: brecv");
      return(-1);
   }
	for (i=0; i < pfds[fd]->fd.meta.p_stat.pcount; i++) {
#ifdef STRICT_DESC_CHECK
		pfds[fd]->fd.iod[i].checkval = IOD_INFO_CHECKVAL;
#endif
		pfds[fd]->fd.iod[i].sock = -1;
	}
	
	/* if this is a passive (group) connection, should call pvfs_gadd */
	return(fd);
}

int unix_open(const char *pathname, int flag, mode_t mode, fpart_p part_p)
{
	int fd, myerr;
	pvfs_filestat p_stat={0,1,8192,8192}; /* metadata */

	if ((fd = open(pathname, flag & ~PVFSMASK, mode)) < 0) {
		return(fd);
	}
	if (!(pfds[fd] = (fdesc_p)malloc(sizeof(fdesc)))) {
		myerr = errno;
		close(fd);
		errno = myerr;
		return(-1);
	}
#ifdef STRICT_DESC_CHECK
	pfds[fd]->checkval = FDESC_CHECKVAL;
#endif
	pfds[fd]->fs = FS_UNIX;
	pfds[fd]->part_p    = NULL;
	pfds[fd]->dim_p     = NULL;
	pfds[fd]->fn_p      = NULL;
	pfds[fd]->fd.off    = 0;
	pfds[fd]->fd.grp_nr = -1;
	pfds[fd]->fd.flag   = flag;
	pfds[fd]->fd.cap    = 0;
	pfds[fd]->fd.ref    = 1;
	pfds[fd]->fd.meta.p_stat = p_stat; /* fill in some dummy values */
	/* set partition (if provided) */
	if (flag & O_PART) {
		if (!(pfds[fd]->part_p = malloc(sizeof(fpart)))) {
			myerr = errno;
			close(fd);
			errno = myerr;
			return(-1);
		}
		*(pfds[fd]->part_p) = *part_p;
	}
	return(fd);
}

#ifdef STRICT_DESC_CHECK
int do_fdesc_check(int fd)
{
	int i, err=0;
	if (fd < 0 || !pfds[fd]) {
		fprintf(stderr, "do_fdesc_check: invalid fd (fd = %d)\n", fd);
		return(-1);
	}
	if (pfds[fd]->checkval != FDESC_CHECKVAL) {
		fprintf(stderr, "do_fdesc_check: fdesc struct (fd = %d) trashed?\n", fd);
		return(-1);
	}
	if (pfds[fd]->fs != FS_PVFS) return(0);

	for (i=0; i < pfds[fd]->fd.meta.p_stat.pcount; i++) {
		if (pfds[fd]->fd.iod[i].checkval != IOD_INFO_CHECKVAL) {
			err = -1;
			fprintf(stderr, "do_fdesc_check: iod_info struct"
				" (fd = %d, iod = %d) trashed?\n", fd, i);
		}
	}
	return(err);
}
#endif
