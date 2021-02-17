/*
 * mgr.c copyright (c) 1996-1999 Clemson University, all rights reserved.
 *
 * Written by Rob Ross and Matt Cettei.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Contact:  Rob Ross    rbross@parl.eng.clemson.edu
 *           Matt Cettei mcettei@parl.eng.clemson.edu
 */

static char mgr_c_hdr[] = "$Header: /projects/cvsroot/pvfs/mgr/mgr.c,v 1.33 2001/01/09 20:14:53 rbross Exp $";

/* UNIX INCLUDES */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <syscall.h>
#include <utime.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/vfs.h>

#include <linux/types.h>
#include <linux/dirent.h>

/* PVFS INCLUDES */
#include <mgr.h>
#include <debug.h>
#include <metaio.h>
#include <dfd_set.h>

/* GLOBALS */
static int (*reqfn[])(int, mreq_p, void *, mack_p) = {
	do_chmod,
	do_chown,
	do_close,
	do_stat,
	do_mount,
	do_open,
	do_unlink,
	do_shutdown,
	do_umount,
	do_fstat,
	do_rename,
	do_iod_info,
	do_mkdir,
	do_fchown,
	do_fchmod,
	do_rmdir,
	do_access,
	do_truncate,
	do_utime,
	do_getdents,
	do_statfs
};

static char *reqtext[] = {
	"chmod",
	"chown",
	"close",
	"stat",
	"mount",
	"open",
	"unlink",
	"shutdown",
	"umount",
	"fstat",
	"rename",
	"iod_info",
	"mkdir",
	"fchown",
	"fchmod",
	"rmdir",
	"access",
	"truncate",
	"utime",
	"getdents",
	"statfs",
/*** ADD NEW CALLS ABOVE THIS LINE ***/
	"error",
	"error",
	"error",
	"error",
	"error"
};

fslist_p active_p;
int acc; /* accept socket for mgr */
int debug_on  = 0;
int is_daemon = 1;
int random_base = 0;
int iclose_sock; /* used with implicit closes only */
fsinfo_p iclose_fsp; /* used with implicit closes only */
sockset socks;

extern int errno;

/* PROTOTYPES */

int do_all_implicit_closes(int sock);
int do_implicit_closes(void *v_p);
int check_for_implicit_close(void *v_p);
int invalidate_conn(iod_info *info_p);
int send_req(iod_info iod[], int iods, int base, int pcount, ireq_p req_p, 
    void *data_p, iack_p ack_p);
int send_open_ack(int sock, mack_p ack_p, fsinfo_p fs_p, int cap);
char *dmeta2fname(struct dmeta *d_p);
int get_dmeta(char * fname, dmeta_p dir);
int resv_name(char *);
int startup(int argc, char **argv);
int cleanup(void);
int timeout_fns(void);
static void *do_statusdump(int sig_nr);

void *filter_dirents(void *buf, size_t buflen);


/* FUNCTIONS */
#define MAX_REASONABLE_TRAILER 16384
int main(int argc, char **argv)
{
	int sock, nbytes, ret, opt, timeout = SOCK_TIME_OUT;
	mreq req;
	mack ack;
	char buf_p[MAX_REASONABLE_TRAILER+2];

#ifdef __RANDOM_BASE__
	random_base = 1;
#endif

	while ((opt = getopt(argc, argv,"t:dr")) != EOF) {
		switch (opt) {
			case 't' : timeout = atoi(optarg); break;
			case 'd' : is_daemon=0; break;
			case 'r' : random_base=1; break;
			default	: break;
		}
	}		
	if ((acc = startup(argc, argv)) == -1) cleanup();
	initset(&socks);
	addsockrd(acc, &socks);
	for(;;) /* accept connections and handle them */ {
		if ((ret = check_socks(&socks, timeout)) < 0) {
			/* don't return an error if we got an interrupt */
			if (errno != EINTR) PERROR("check_socks in main");
			continue;
		} else if (ret == 0) {
			timeout_fns();
			continue;
		}
		while ((sock = nextsock(&socks)) >= 0) {
			/* check if this is the accept socket, and if so do an accept */
			if (sock == acc) {
				if ((sock = accept(acc, NULL, 0)) < 0) {
					PERROR("accept in main");
					continue;
				}
				addsockrd(sock, &socks); /* needs to be after accept check */
			}

			/* REALLY THIS SHOULD HAVE A TIMEOUT ASSOCIATED WITH IT... */
			if ((nbytes = brecv(sock, &req, sizeof(req))) < 0) /* get request */ {
				if (errno == EPIPE) {
					LOG1("socket %d closed\n", sock);
					goto socket_cleanup;
				}
				else {
					PERROR("brecv in main");
					goto socket_cleanup;
				}
				continue;
			}
			if (req.majik_nr != MGR_MAJIK_NR) {
				ERR("main: invalid magic number; ignoring request\n");
				goto socket_cleanup;
			}
			if (req.dsize >= MAX_REASONABLE_TRAILER) {
				/* this is ridiculous - throw request out */
				ERR1("main: dsize = %d\n; ignoring request\n", (int) req.dsize);
				goto socket_cleanup;
			}
			if (req.type < 0 || req.type > MAX_MGR_REQ) /* invalid request # */ {
				ERR1("invalid request (%d)", req.type);
				goto socket_cleanup;
			}
			if (req.dsize > 0) /* grab any trailing data */ {
				if ((nbytes = brecv(sock, buf_p, req.dsize)) < 0) {
					PERROR("brecv in main - getting trailing data");
					goto socket_cleanup;
				}
				buf_p[req.dsize] = '\0'; /* terminate this if string */
			}

			/* initialize request */
			ack.majik_nr = MGR_MAJIK_NR;
			ack.type     = req.type; /* everything else is filled in in reqfn() */
			ack.status   = 0;
			ack.dsize    = 0;
			LOG2("req: %s on %d\n", reqtext[req.type], sock);
			ret = (reqfn[req.type])(sock, &req, buf_p, &ack); /* handle request */
			continue;

socket_cleanup:
			close(sock);
			delsock(sock, &socks);
			do_all_implicit_closes(sock);
		}
	} /* end of for(;;) */
	return(0);
} /* end of main() */

int startup(int argc, char **argv)
{
	int fd;

	if (getuid() != 0 && geteuid() != 0) {
		ERR("WARNING: pvfsmgr should be run as root\n");
	}

	if ((fd = new_sock()) < 0) {
		PERROR("pvfsmgr: new_sock");
		return(-1);
	}

	set_sockopt(fd, SO_REUSEADDR, 1);

	if (bind_sock(fd, MGR_REQ_PORT) < 0) {
		PERROR("pvfsmgr: bind_sock");
		return(-1);
	}
	if (listen(fd, MGR_BACKLOG) != 0) {
		PERROR("pvfsmgr: listen");
		return(-1);
	}
	if (is_daemon) {
		char logname[] = "/tmp/mgrlog.XXXXXX";
		int logfd;

		if ((logfd = mkstemp(logname)) == -1) {
			ERR("couldn't create logfile...continuing...\n");
			close(0); close(1); close(2);
		}
		else {
			fchmod(logfd, 0755);
			dup2(logfd, 2);
			dup2(logfd, 1);
			close(0);
		}
		if (fork()) exit(0); /* fork() and kill parent */
		setsid();
	}	

	active_p = fslist_new(); /* initialize filesystem list */

	/* set up SIGUSR1 handler to spit out debugging info */
	signal(SIGUSR1, (void *)do_statusdump);

	/* set up SIGTERM handler to shut things down */
	signal(SIGTERM, (void *)mgr_shutdown);

	/* set up SIGHUP handler to restart the daemon */
	signal(SIGHUP, (void *)restart);
	
	/* catch SIGPIPE and SIGSEGV signals and log them, on SEGV we die */
	signal(SIGPIPE, (void *)do_signal);
	signal(SIGSEGV, (void *)do_signal);

	umask(0);
	chdir("/"); /* to avoid unnecessary busy filesystems */
	LOG1("%s\n", mgr_c_hdr);

	return(fd);
} /* end of startup() */

int cleanup()
{
	fslist_cleanup(active_p);
	close(acc);
	exit(0);
} /* end of cleanup() */

int do_chmod(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	req_p->req.chmod.fname = data_p;

	/* check for reserved name first */
	if (resv_name(data_p) == 0) {
		ack_p->status          = md_chmod(req_p); /* 0 on success */
		ack_p->eno             = errno;
	}
	else {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
	}
	if (!ack_p->status) return(bsend(sock, ack_p, sizeof(mack)));
	return(send_error(ack_p->status, ack_p->eno, sock, ack_p));
}

int do_chown(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	req_p->req.chown.fname = data_p;

	/* check for reserved name first */
	if (resv_name(data_p) == 0) {
		ack_p->status          = md_chown(req_p); /* 0 on success */
		ack_p->eno             = errno;
	}
	else {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
	}
	if (!ack_p->status) return(bsend(sock, ack_p, sizeof(mack)));
	return(send_error(ack_p->status, ack_p->eno, sock, ack_p));
}

/* do_access() - perform the access() operation
 *
 * This call also performs a stat(), so we go ahead and return that
 * data.  The file size included in there is not valid, so we -1 it out.
 */
int do_access(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	/* right now we simply check the file not the path */
	int mode_bits = 0;
	int ret;

	req_p->req.access.fname  = data_p;

	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}
	/* do a stat so we can figure out owner and permissions */
	if ((ret = md_stat(data_p, req_p, &(ack_p->ack.access.meta))) != 0) {
		ack_p->ack.access.meta.fs_ino = -1;
		return(send_error(ret, errno, sock, ack_p));
	}
	ack_p->ack.access.meta.u_stat.st_size = -1; /* not valid anyway */
	ack_p->eno = EACCES;
	ack_p->status = -1;
	if (req_p->req.access.mode & R_OK)
		mode_bits |= 004;
	if (req_p->req.access.mode & W_OK)
		mode_bits |= 002;
	if (req_p->req.access.mode & X_OK)
		mode_bits |= 001;
	/* check o, g, and u in that order */
	if (((ack_p->ack.access.meta.u_stat.st_mode & 007) &
		 mode_bits) == mode_bits)
		ack_p->status = 0;
	else if (ack_p->ack.access.meta.u_stat.st_gid == req_p->gid &&
		(((ack_p->ack.access.meta.u_stat.st_mode >> 3) & 007) &
		 mode_bits) == mode_bits)
		ack_p->status = 0;
	else if (ack_p->ack.access.meta.u_stat.st_uid == req_p->uid &&
		(((ack_p->ack.access.meta.u_stat.st_mode >> 6) & 007) &
		 mode_bits) == mode_bits)
		ack_p->status = 0;
	if (!ack_p->status) return(bsend(sock, ack_p, sizeof(mack)));
	return(send_error(ack_p->status, ack_p->eno, sock, ack_p));
} /* end of do_access */

int do_truncate(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	int fd, ret;
	fsinfo_p fs_p;
	ireq iodreq;
	fmeta meta;

	req_p->req.truncate.fname  = data_p;
	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}

	if ((fs_p = quick_mount(req_p->req.truncate.fname, req_p->uid,
		req_p->gid, ack_p)) == NULL)
	{
		return(send_error(-1, errno, sock, ack_p));
	}

	/* get metadata, check permission to write to file */
	if ((fd = meta_open(req_p->req.truncate.fname, O_RDONLY)) < 0
	|| (meta_read(fd, &meta) < 0)
	|| (meta_close(fd) < 0))
	{
		return(send_error(-1, errno, sock, ack_p));
	}

	/* for the moment permissions aren't checked... */

	/* build request for iods, send it */
	iodreq.majik_nr = IOD_MAJIK_NR;
	iodreq.type = IOD_TRUNCATE;
	iodreq.req.truncate.f_ino = meta.u_stat.st_ino;
	iodreq.req.truncate.length = req_p->req.truncate.length;
	iodreq.req.truncate.p_stat = meta.p_stat;
	iodreq.req.truncate.part_nr = 0; /* set in send_req() */

	if ((ret = send_req(fs_p->iod, fs_p->nr_iods, meta.p_stat.base, 
			meta.p_stat.pcount, &iodreq, NULL, NULL)) < 0)
	{
		return(send_error(ret, errno, sock, ack_p));
	}	
	if (sock >= 0) /* real socket number, send the ack */ {
		ret = bsend(sock, ack_p, sizeof(mack));
		if (ret < 0) return(-1);
	}
	return(0);
}

/* DO_UTIME() - perform utime() operation
 */
int do_utime(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	int fd, i, myerr;
	fmeta meta;

	req_p->req.utime.fname  = data_p;

	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}

	/* open metadata file */
	if ((fd = meta_open(req_p->req.utime.fname, O_RDWR)) < 0)
	{
		if (errno != EISDIR) {
			myerr = errno;
			PERROR("do_utime: meta_open");
			errno = myerr;
			return(send_error(-1, errno, sock, ack_p));
		}
		else fd = 0; /* needed for meta_access() below...Scott was on
						  * crack when he wrote the fn... */
	}

	/* check write access permissions on file */
	if (meta_access(fd, req_p->req.utime.fname,req_p->uid,req_p->gid, W_OK) < 0)
	{
		myerr = errno;
		PERROR("do_utime: meta_access");
		errno = myerr;
		return(send_error(-1, errno, sock, ack_p));
	}

	/* see if its a directory or file */
	if ((i = check_pvfs(req_p->req.utime.fname)) < 1) {
		myerr = errno;
		PERROR("do_utime: check_pvfs");
		errno = myerr;
		return(send_error(-1, errno, sock, ack_p));
	}

	/* read metadata */
	if (i == 1) {
		if (meta_read(fd, &meta) != sizeof(fmeta)) {
			myerr = errno;
			PERROR("do_utime: meta_read");
			errno = myerr;
			return(send_error(-1, errno, sock, ack_p));
		}

		/* modify metadata */
		meta.u_stat.st_atime = req_p->req.utime.buf.actime;
		meta.u_stat.st_mtime = req_p->req.utime.buf.modtime;

		if ((i=meta_write(fd, &meta)) != sizeof(fmeta)) {
			myerr = errno;
			PERROR("do_utime: meta_write");
			errno = myerr;
			return(send_error(-1, errno, sock, ack_p));
		}

		meta_close(fd);
	}
	else { /* it's a directory */
		if (utime(req_p->req.utime.fname,&(req_p->req.utime.buf)) < 0) {
			myerr = errno;
			PERROR("do_utime: utime");
			errno = myerr;
			return(send_error(-1, errno, sock, ack_p));
		}
	}

	/* send ack to client */
	if (sock >= 0) /* real socket number, send the ack */ {
		if (bsend(sock, ack_p, sizeof(mack)) < 0)
			return(-1);
	}

	/* success */
	return(0);
}

/* do_fstat() - perform fstat() operation -- calls do_stat() to do the
 * work
 */
int do_fstat(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	mreq fakereq;
	fsinfo_p fs_p;
	finfo_p f_p;

	/* find filesystem & open file */
	if (!(fs_p = fs_search(active_p, req_p->req.fstat.meta.fs_ino))
		|| !(f_p = f_search(fs_p->fl_p, req_p->req.fstat.meta.u_stat.st_ino)))
	{
		ack_p->status = -1;
		ack_p->eno  = errno;
		bsend(sock, ack_p, sizeof(mack));
		return(-1);
	}

	/* build a fake stat request, call do_stat() */
	fakereq.majik_nr = MGR_MAJIK_NR;
	fakereq.type     = MGR_STAT;
	fakereq.uid      = req_p->uid;
	fakereq.gid      = req_p->gid;
	fakereq.dsize    = strlen(f_p->f_name);

	return(do_stat(sock, &fakereq, f_p->f_name, ack_p));
} /* end of do_fstat() */

int do_stat(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	int ret, i;
	fsinfo_p fs_p;
	ireq iodreq;

	req_p->req.stat.fname  = data_p;

	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}

	if ((fs_p = quick_mount(req_p->req.stat.fname, req_p->uid,
		req_p->gid, ack_p)) == NULL)
	{
		return(send_error(-1, errno, sock, ack_p));
	}	

	if ((ret = md_stat(data_p, req_p, &(ack_p->ack.stat.meta))) != 0) {
		return(send_error(ret, errno, sock, ack_p));
	}

	/* if regular file, get size from iods */
	if (S_ISREG(ack_p->ack.stat.meta.u_stat.st_mode)) {
		iodreq.majik_nr       = IOD_MAJIK_NR;
		iodreq.type 			 = IOD_STAT;
		iodreq.dsize 			 = 0;
		iodreq.req.stat.f_ino = ack_p->ack.stat.meta.u_stat.st_ino;
		if ((ret = send_req(fs_p->iod, fs_p->nr_iods, 
			ack_p->ack.stat.meta.p_stat.base, ack_p->ack.stat.meta.p_stat.pcount, 
			&iodreq, data_p, NULL)) < 0)
		{
			return(send_error(ret, errno, sock, ack_p));
		}	
		ack_p->ack.stat.meta.u_stat.st_size = 0;
		for(i = 0; i < ack_p->ack.stat.meta.p_stat.pcount; i++) {
			ack_p->ack.stat.meta.u_stat.st_size +=
				fs_p->iod[(i + ack_p->ack.stat.meta.p_stat.base) %
				fs_p->nr_iods].ack.ack.stat.fsize;
		}
	}

	if (!ack_p->status) return(bsend(sock, ack_p, sizeof(mack)));
	if (!errno) errno = ack_p->eno; /* return the old error */
	return(-1);
} /* end of do_stat() */

/* do_statfs()
 */
int do_statfs(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	ireq iodreq;
	fsinfo_p fs_p;
	struct statfs sfs;
	int64_t min_free_bytes = 0;
	int64_t total_bytes = 0;
	int i, err;

	req_p->req.statfs.fname = data_p;

   /* clear the statfs buffer */
   memset(&sfs, 0, sizeof(sfs));

	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, ENOENT, sock, ack_p));
	}

	/* make sure we can talk to the iods */
	if ((fs_p = quick_mount(req_p->req.unlink.fname, req_p->uid,
	req_p->gid, ack_p)) == NULL)
	{
		return(send_error(-1, errno, sock, ack_p));
	}

	/* statfs on the file locally to get file-related values */
	err = statfs(data_p, &sfs);
	if (err == -1) return (send_error(-1, errno, sock, ack_p));

	ack_p->ack.statfs.tot_files = sfs.f_files;
	ack_p->ack.statfs.free_files = sfs.f_ffree;
	ack_p->ack.statfs.namelen = sfs.f_namelen;
	
	/* if we didn't get an error by now, then our filename is ok */

	/* send request to all iods for this file system */
	iodreq.majik_nr = IOD_MAJIK_NR;
	iodreq.type = IOD_STATFS;
	iodreq.dsize = 0;
	if ((err = send_req(fs_p->iod, fs_p->nr_iods, 0, 
			fs_p->nr_iods, &iodreq, data_p, NULL)) < 0)
	{
		return(send_error(err, errno, sock, ack_p));
	}	

	/* pull data together
    *
	 * Note: we total up the bytes on all iods for the total value, but
	 * for the free value we're going to calculate using:
	 * 
	 * (free space) = (# of iods) * (minimum amount free on any one)
	 *
	 * Although we're lying a bit, it's the best value for giving an
	 * expectation of what could be written to the system.
	 */
	min_free_bytes = fs_p->iod[0].ack.ack.statfs.free_bytes;
	for (i=0; i < fs_p->nr_iods; i++) {
		if (min_free_bytes > fs_p->iod[i].ack.ack.statfs.free_bytes)
			min_free_bytes = fs_p->iod[i].ack.ack.statfs.free_bytes;
		total_bytes += fs_p->iod[i].ack.ack.statfs.tot_bytes;
	}
	ack_p->ack.statfs.tot_bytes = total_bytes;
	ack_p->ack.statfs.free_bytes = min_free_bytes * ((int64_t) fs_p->nr_iods);

	if (sock >= 0) /* real socket number, send the ack */ {
		err = bsend(sock, ack_p, sizeof(mack));
		if (err < 0) return(-1);
	}
	return(0);
}

int do_unlink(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	int ret;
	fsinfo_p fs_p;
	finfo_p f_p;
	ireq iodreq;
	fmeta meta;

	req_p->req.unlink.fname  = data_p;

	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}

	if ((fs_p = quick_mount(req_p->req.unlink.fname, req_p->uid,
		req_p->gid, ack_p)) == NULL)
	{
		return(send_error(-1, errno, sock, ack_p));
	}
	/* md_unlink returns -1 on failure, or an open fd to metadata file on
	 * success.  This is so we can hold on to the inode if we need to.
	 */
	if ((ret = md_unlink(req_p, &meta)) < 0) {
		return(send_error(ret, errno, sock, ack_p));
	}

	if ((f_p = f_search(fs_p->fl_p, meta.u_stat.st_ino)) != 0) {
		/* don't delete this one yet */
		LOG("do_unlink: file open, delaying unlink\n");
		f_p->unlinked = ret;
	}
	else /* need to do IOD call */ {
		close(ret); /* don't need to keep the metadata file open */
		iodreq.majik_nr = IOD_MAJIK_NR;
		iodreq.type = IOD_UNLINK;
		iodreq.dsize = 0;
		iodreq.req.unlink.f_ino = meta.u_stat.st_ino;
		if ((ret = send_req(fs_p->iod, fs_p->nr_iods, meta.p_stat.base, 
				meta.p_stat.pcount, &iodreq, NULL, NULL)) < 0)
		{
			return(send_error(ret, errno, sock, ack_p));
		}	
	}
	if (sock >= 0) /* real socket number, send the ack */ {
		ret = bsend(sock, ack_p, sizeof(mack));
		if (ret < 0) return(-1);
	}
	return(0);
} /* end of do_unlink() */

int do_close(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	int ret, myerr = 0;
	fsinfo_p fs_p;
	finfo_p f_p;
	ireq iodreq;
	/* find filesystem & open file */
	if (!(fs_p = fs_search(active_p, req_p->req.close.meta.fs_ino))
		|| !(f_p = f_search(fs_p->fl_p, req_p->req.close.meta.u_stat.st_ino)))
	{
		myerr = errno;
		LOG2("do_close: error finding fs (%Ld) or ino (%ld)\n",
			req_p->req.close.meta.fs_ino,
			req_p->req.close.meta.u_stat.st_ino);
		return(send_error(-1, errno, sock, ack_p));
	}
	if (--f_p->cnt == 0) /* last open instance; tell IODs to close */ {
		LOG("last reference; closing file\n");
		/* send request to close to IODs */
		iodreq.majik_nr = IOD_MAJIK_NR;
		iodreq.type     = IOD_CLOSE;
		iodreq.dsize    = 0;
		iodreq.req.close.f_ino = f_p->f_ino;
		/* Tells iod to close all instances of open file */
		iodreq.req.close.cap = -1; 
		
		if ((ret = send_req(fs_p->iod, fs_p->nr_iods, f_p->p_stat.base, 
				f_p->p_stat.pcount, &iodreq, NULL, NULL)) < 0)
		{
			myerr = errno;
		}

		if (f_p->unlinked >= 0) /* tell IODs to remove the file too */ {
			iodreq.majik_nr = IOD_MAJIK_NR;
			iodreq.type     = IOD_UNLINK;
			iodreq.dsize    = 0;
			iodreq.req.unlink.f_ino = f_p->f_ino;
			if ((ret = send_req(fs_p->iod, fs_p->nr_iods, f_p->p_stat.base, 
					f_p->p_stat.pcount, &iodreq, NULL, NULL)) < 0)
			{
				myerr = errno;
			}
		}
		/* md_close() will pull the atime and mtime from the request and
		 * set them in the metadata file.
		 */
		 req_p->req.close.meta.u_stat.st_atime = 
		 req_p->req.close.meta.u_stat.st_mtime = time(NULL);
		md_close(req_p, f_p->f_name);
		f_rem(fs_p->fl_p,f_p->f_ino); /* wax file info */
	}
	else /* just need to update the finfo structure */ {
		dfd_clr(sock, &f_p->socks);
	}
	if (myerr) return(send_error(-1, myerr, sock, ack_p));
	if (sock >= 0 && bsend(sock, ack_p, sizeof(mack)) < 0) return(-1);
	return(0);
}

int do_mount(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	fsinfo_p fs_p;
	int niods, ret=-1;
	static char tmpbuf[256];
	iodtabinfo_p tab_p;
	dmeta dir;

	/* save fs inode # and # of iods */
	req_p->req.mount.fsname = data_p;

	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}

	if (get_dmeta(req_p->req.mount.fsname, &dir) < 0) {
		return(send_error(ret, errno, sock, ack_p));
	}
	/* if mounted, OK */
	if (fs_search(active_p, dir.fs_ino)) { 
		if (sock > -1) if (bsend(sock, ack_p, sizeof(mack)) < 0) return(-1);
		return(0);
	}

	/* set up iodtab file name */
	req_p->req.mount.fsname = data_p;
	strncpy(tmpbuf, data_p, req_p->dsize);
	strcpy(&tmpbuf[req_p->dsize], TABFNAME);

	/* read iodtab file */
	if ((tab_p = parse_iodtab(tmpbuf)) == NULL) {
		return(send_error(ret, errno, sock, ack_p));
	}

	/* allocate and fill in fsinfo structure */
	if ((fs_p = (fsinfo_p) malloc(sizeof(fsinfo)
		+ sizeof(iod_info) * ((tab_p->nodecount)-1))) == NULL)
	{
		return(send_error(ret, errno, sock, ack_p));
	}
	fs_p->nr_iods = tab_p->nodecount;
	fs_p->fs_ino  = dir.fs_ino;
	fs_p->fl_p    = NULL;
	for (niods = 0; niods < tab_p->nodecount; niods++) {
		fs_p->iod[niods].addr = tab_p->iod[niods];
		fs_p->iod[niods].sock = -1;
	}

	if ((ret = fs_add(active_p, fs_p)) < 0) {
		PERROR("fs_add in do_mount");
		return(send_error(ret, errno, sock, ack_p));
	}
	/* send ack if this isn't a "fake" request */
	if (sock > -1) if (bsend(sock, ack_p, sizeof(mack)) < 0) return(-1);
	return(0);
}

#define RQ_PSTAT req_p->req.open.meta.p_stat
#define AK_PSTAT ack_p->ack.open.meta.p_stat

int do_open(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	int ret, new_entry=0, new_file=0, myerr;
	fsinfo_p fs_p;
	finfo_p f_p;
	ireq iodreq;

	/* NOTE:
	 * new_entry is used to indicate that a file needs to be added into
	 * our list of open files (isn't already in there)
	 *
	 * new_file is used to indicate that we're creating this new file.
	 * -- Rob
	 */

	req_p->req.open.fname  = data_p;

	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}

	if ((fs_p = quick_mount(req_p->req.open.fname, req_p->uid,
	req_p->gid, ack_p)) == NULL) {
		LOG1("do_open: quick_mount: NULL returned for %s\n",
			req_p->req.open.fname);
		return(send_error(-1, errno, sock, ack_p));
	}

	/* check for pcount > nr_iods and base >= nr_iods */
	if (RQ_PSTAT.pcount > fs_p->nr_iods || RQ_PSTAT.base >= fs_p->nr_iods) {
		return(send_error(-1, EINVAL, sock, ack_p));
	}

	/* if default values are requested, fill them in */
	if (RQ_PSTAT.pcount == -1) RQ_PSTAT.pcount = fs_p->nr_iods; 
	if (RQ_PSTAT.ssize == -1)  RQ_PSTAT.ssize  = DEFAULT_SSIZE;
	if (RQ_PSTAT.bsize == -1)  RQ_PSTAT.bsize  = DEFAULT_BSIZE;

	if (RQ_PSTAT.base == -1) {
		if (random_base) {
			/* pick a random base node number */
			int randbase;
			randbase = rand() % fs_p->nr_iods;
			RQ_PSTAT.base = randbase;
		}
		else RQ_PSTAT.base = 0;
	}

	/* check to see if the file exists already */
	if ((ret = meta_open(data_p, O_RDONLY)) < 0) new_file = 1;
	else meta_close(ret);

	/* md_open() fills in the st_ino structure used below and 
	 * performs sanity checking on the values passed in
	 *
	 * It also creates the metadata file in the specified directory.
	 */
	if ((ret = md_open(data_p, req_p, &(ack_p->ack.open.meta))) < 0) {
		ERR2("do_open: md_open failed: %s, flags = 0x%x\n",
		strerror(errno), req_p->req.open.flag);
		if (new_file && !(req_p->req.open.flag & O_CREAT)) {
		    ERR("do_open: tried to open new file without O_CREAT\n");
		}
		return(send_error(ret, errno, sock, ack_p));
	}

	ack_p->ack.open.meta.fs_ino = fs_p->fs_ino;

	f_p = f_search(fs_p->fl_p,ack_p->ack.open.meta.u_stat.st_ino);

	if (!f_p) /* not open */ {
		new_entry=1;
		if (!(f_p = f_new())) return(send_error(-1, errno, sock, ack_p));
		f_p->f_ino   = ack_p->ack.open.meta.u_stat.st_ino; 
		f_p->p_stat  = AK_PSTAT;
		f_p->f_name  = (char *)malloc(strlen(req_p->req.open.fname)+1);
		strcpy(f_p->f_name, req_p->req.open.fname);
		dfd_set(sock, &f_p->socks);
	}
	else /* already open */ {
		f_p->cap++;
		f_p->cnt++;
		dfd_set(sock, &f_p->socks);
	}

	f_dump(f_p);

	iodreq.majik_nr        = IOD_MAJIK_NR;
	iodreq.type            = IOD_OPEN;
	iodreq.dsize           = 0;
	iodreq.req.open.f_ino  = f_p->f_ino;
	iodreq.req.open.cap    = f_p->cap;
	iodreq.req.open.flag   = req_p->req.open.flag;
	iodreq.req.open.mode   = S_IRUSR | S_IWUSR; /* permissions on file */
	iodreq.req.open.p_stat = AK_PSTAT;
	iodreq.req.open.pnum   = 0;
	
	if ((ret = send_req(fs_p->iod, fs_p->nr_iods, f_p->p_stat.base, 
			f_p->p_stat.pcount, &iodreq, NULL, NULL)) < 0)
	{
		ERR1("do_open (%s): send_req failed\n", req_p->req.open.fname);
		if (new_file) {
			/* here is a tricky case...we need to remove all the files we
			 * created on the nodes that DID work and we need to remove the
			 * metadata file that md_open() created too...
			 *
			 * We're going to get errors from all this, because something
			 * has happened to one of our IODs.  We're just trying to get
			 * everything back to a sane state as best we can.
			 */
			mreq fakereq;
			mack fakeack;

			myerr = errno;

			ERR1("do_open(%s): cleaning up after failed open\n",
				req_p->req.open.fname);
			fakereq.majik_nr = MGR_MAJIK_NR;
			fakereq.type     = MGR_CLOSE;
			fakereq.uid      = req_p->uid;
			fakereq.gid      = req_p->gid;
			fakereq.dsize    = 0;
			fakereq.req.close.meta.fs_ino        = fs_p->fs_ino;
			fakereq.req.close.meta.u_stat.st_ino = f_p->f_ino;
			f_p->unlinked++;

			/* here we add the file in so that do_close() can find it */
			f_add(fs_p->fl_p, f_p);
			/* we're closing the file so all the IODs will know it is safe
			 * to unlink it; we're telling them to unlink it at the same
			 * time...
			 */
			do_close(-1, &fakereq, NULL, &fakeack);
			/* do_close() gets rid of our copy of the file info at f_p */

			/* now get rid of the metadata file */
			meta_unlink(req_p->req.open.fname);
			errno = myerr;
		}
		return(send_error(ret, errno, sock, ack_p));
	}

	if (send_open_ack(sock, ack_p, fs_p, f_p->cap) < 0) {
		/* handle cleaning up bad send */
		ERR("do_open: send_open_ack failed\n");
		if (new_entry) f_free(f_p);
		return(-1);
	}
	if (new_entry) f_add(fs_p->fl_p, f_p); /* add file info to list if new */
	return(0);
} /* end of do_open() */

/* SEND_OPEN_ACK() - sends ACK and iod addresses through socket 
 * back to an application that has opened a file
 */
int send_open_ack(int sock, mack_p ack_p, fsinfo_p fs_p, int cap)
{
	struct sockaddr saddr;
	int saddrsize, pcnt_remain, nr_infos;

	ack_p->eno = 0;
	ack_p->dsize = AK_PSTAT.pcount * sizeof(iod_info);
	ack_p->ack.open.cap = cap;

	saddrsize = sizeof(struct sockaddr);
	if (getsockname(sock, &saddr, &saddrsize) < 0) {
		PERROR("getsockname");
		return(-1);
	}
	ack_p->ack.open.meta.mgr = saddr;
	if (bsend(sock, ack_p, sizeof(mack)) < 0) return(-1);

	/* handle the simple one-send case first */
	if (AK_PSTAT.base+AK_PSTAT.pcount <= fs_p->nr_iods) {
		if (bsend(sock, &fs_p->iod[AK_PSTAT.base], ack_p->dsize) < 0) return(-1);
		return(0);
	}
	
	/* otherwise it's going to take multiple xfers */
	pcnt_remain = AK_PSTAT.pcount;
	if (AK_PSTAT.base) /* send partial list */ {
		nr_infos = fs_p->nr_iods - AK_PSTAT.base;
		if (bsend(sock, &fs_p->iod[AK_PSTAT.base], nr_infos*sizeof(iod_info))<0)
			return(-1);
		pcnt_remain -= nr_infos;
	}
	while (pcnt_remain >= fs_p->nr_iods) /* send the whole list */ {
		if (bsend(sock, fs_p->iod, fs_p->nr_iods*sizeof(iod_info)) < 0)
			return(-1);
		pcnt_remain -= fs_p->nr_iods;
	}
	if (pcnt_remain > 0) /* send list part, starting w/iod 0 */ {
		if (bsend(sock, fs_p->iod, pcnt_remain * sizeof(iod_info)) < 0)
			return(-1);
	}
	return(0);
}

#undef RQ_PSTAT
#undef AK_PSTAT

int do_umount(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	int ret;
	fsinfo_p fs_p;
	dmeta dir;

	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}

	/* find filesystem & check for mounted */
	if (get_dmeta(data_p, &dir) < 0) {
		errno = ENOENT;
		return(send_error(-1, ENOENT, sock, ack_p));
	}

	if ((fs_p = fs_search(active_p, dir.fs_ino)) == NULL) {
		errno = ENOENT;
		return(send_error(-1, ENOENT, sock, ack_p));
	}

	/* shut down all IODs */
	if ((ret = stop_iods(fs_p)) < 0) {
		return(send_error(ret, errno, sock, ack_p));
	}
	/* close any open files (?) */

	/* remove entry for filesystem */
	fs_rem(active_p, dir.fs_ino);
	/* send ack if this isn't a "fake" request */
	if (sock > -1 ) if (bsend(sock, ack_p, sizeof(mack)) < 0) return(-1);
	return(0);
}

int do_shutdown(int sock, mreq_p req_p, void *data_p, mack_p ack_p)
{
	forall_fs(active_p, stop_iods);
	LOG("Shut down all iods\n");
	bsend(sock, ack_p, sizeof(mack));
	LOG("Sent ack\n");
	cleanup();
	return(0);
}

int do_rename(int sock, mreq_p req_p, void *data_p, mack_p ack_p)
{
	char *c_p = data_p, temp[MAXPATHLEN];
	int fd, length;

	/* set pointers to old and new filenames */
	req_p->req.rename.oname = c_p;
	for (;*c_p;c_p++); /* theres got to be a better way */
	req_p->req.rename.nname = ++c_p;

	/* check for reserved names first */
	if (resv_name(req_p->req.rename.oname) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}

	if (resv_name(req_p->req.rename.nname) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}

	/* check metadat for permissions of old file */
	if ((fd = meta_open(req_p->req.rename.oname,O_RDONLY)) < 0) {
		PERROR("do_rename: meta_open");
		return(send_error(-1, errno, sock, ack_p));
	}
	
	/* check permissions */
	if (meta_access(fd, req_p->req.rename.oname, req_p->uid,
		req_p->gid, R_OK | W_OK) < 0) {
		meta_close(fd);
		PERROR("do_rename: meta_access (old name)");
		return(send_error(-1, errno, sock, ack_p));
	}
	if (meta_close(fd) < 0) {
		PERROR("do_rename: meta_close");
		return(send_error(-1, errno, sock, ack_p));
	}
	/* check for write/execute permissions on parent directory */
	strncpy(temp,req_p->req.rename.nname,MAXPATHLEN);
	length = get_parent(temp);
	/* if length < 0, CWD used, directory permissions not checked */
	if (length >= 0) {
		if (meta_access(0, temp, req_p->uid, req_p->gid, X_OK|W_OK) < 0) { 
			PERROR("do_rename: meta_access (new name)");
			return(send_error(-1, errno, sock, ack_p));
		}
	}

	/* just try to remove the new file and see if it works */
   if (unlink(req_p->req.rename.nname) < 0) {
		if (errno != ENOENT) {
			PERROR("do_rename: unlink");
			return(send_error(-1, errno, sock, ack_p));
		}
	}
	/* move to new name */
   if (rename(req_p->req.rename.oname, req_p->req.rename.nname) < 0) {
      PERROR("do_rename: rename");
      return(send_error(-1, errno, sock, ack_p));
   }
	if (sock >= 0) /* real socket number, send the ack */ {
		if (bsend(sock, ack_p, sizeof(mack)) < 0) return(-1);
	}
	return(0); /* no errors, no ack to send */
} /* end of do_rename() */

/* do_iod_info() - grabs statistics from iods
 */
int do_iod_info(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	fsinfo_p fs_p;
	dmeta dir;

	if (get_dmeta(data_p, &dir) < 0) {
		return(send_error(-1, ENOENT, sock, ack_p));
	}

	if ((fs_p = fs_search(active_p, dir.fs_ino)) == NULL) {
		return(send_error(-1, ENOENT, sock, ack_p));
	}
	ack_p->ack.iod_info.nr_iods = fs_p->nr_iods;
	ack_p->dsize  = fs_p->nr_iods*sizeof(iod_info);

	if (sock > -1 ) if (bsend(sock, ack_p, sizeof(mack)) < 0) return(-1);
	/* also send back iod_info 	*/
	if (bsend(sock, fs_p->iod, ack_p->dsize) < 0)
		return(-1);
	return(0);
}

/* do_mkdir() - makes a new directory in the file system
 */
int do_mkdir(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	char temp[MAXPATHLEN];
	int length;
	dmeta dir;

	/* set request file name */
	req_p->req.mkdir.fname  = data_p;

	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}

	/* check permissions to make this directory */
	strncpy(temp, req_p->req.mkdir.fname, MAXPATHLEN);
	length = get_parent(temp);
	if (length >= 0) {
		/* check for permissions to write to directory */
		if (meta_access(0, temp, req_p->uid, req_p->gid, X_OK | W_OK)
			< 0) {
			PERROR("do_mkdir: meta_access");
			return(send_error(-1, errno, sock, ack_p));
		}
	}

	/* get metadata of the dir this is going into */
	if (get_dmeta(req_p->req.mkdir.fname, &dir) < 0) {
		errno = ENOENT;
		return(send_error(-1, ENOENT, sock, ack_p));
	}
	
	/* make the new directory */
	if ((ack_p->status = mkdir(req_p->req.mkdir.fname, 0775)) < 0) {
		return(send_error(-1, errno, sock, ack_p));
	}
	/* set up the dotfile in the new directory */
	dir.dr_uid = req_p->uid;
	if (!(S_ISGID & dir.dr_mode))
		dir.dr_gid = req_p->gid;
	dir.dr_mode = req_p->req.mkdir.mode | (S_ISGID & dir.dr_mode) | S_IFDIR;

	/* NOTE:
	 * It's important to use md_mkdir() here instead of put_dmeta;
	 * md_mkdir() understands to use the fname portion of the dmeta
	 * structure as part of the name of the directory, so it will get the
	 * sd_path field correct.  put_dmeta() will not.
	 */
	if (md_mkdir(req_p->req.mkdir.fname, &dir) < 0) {
		errno = ENOENT;
		return(send_error(-1, ENOENT, sock, ack_p));
	}
	/* send ack to lib */
	if (sock >= 0) /* real socket number, send the ack */ {
		if (bsend(sock, ack_p, sizeof(mack)) < 0)
			return(-1);
	}
	return(0);
} /* end of do_mkdir() */

/* do_fchown() - chowns based on file descriptor
 */
int do_fchown(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	mreq fakereq;
	fsinfo_p fs_p;
	finfo_p f_p;

	/* find filesystem & open file */
	if (!(fs_p = fs_search(active_p, req_p->req.fchown.fs_ino))
	|| !(f_p = f_search(fs_p->fl_p, req_p->req.fchown.file_ino)))
	{
		ack_p->status = -1;
		ack_p->eno  = errno;
		bsend(sock, ack_p, sizeof(mack));
		return(-1);
	}

	/* build a fake chown request, call do_chown() */
	fakereq.majik_nr = MGR_MAJIK_NR;
	fakereq.type     = MGR_CHOWN;
	fakereq.uid      = req_p->uid;
	fakereq.gid      = req_p->gid;
	fakereq.dsize    = strlen(f_p->f_name);
	fakereq.req.chown.owner = req_p->req.fchown.owner;
	fakereq.req.chown.group = req_p->req.fchown.group;

	return(do_chown(sock, &fakereq, f_p->f_name, ack_p));
} /* end of do_fchown() */

/* do_fchmod() - chmods based on file descriptor
 */
int do_fchmod(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	mreq fakereq;
	fsinfo_p fs_p;
	finfo_p f_p;

	/* find filesystem & open file */
	if (!(fs_p = fs_search(active_p, req_p->req.fchmod.fs_ino))
	|| !(f_p = f_search(fs_p->fl_p, req_p->req.fchmod.file_ino)))
	{
		ack_p->status = -1;
		ack_p->eno  = errno;
		bsend(sock, ack_p, sizeof(mack));
		return(-1);
	}
	/* build a fake chmod request, call do_chmod() */
	fakereq.majik_nr = MGR_MAJIK_NR;
	fakereq.type     = MGR_CHMOD;
	fakereq.uid      = req_p->uid;
	fakereq.gid      = req_p->gid;
	fakereq.dsize    = strlen(f_p->f_name);
	fakereq.req.chmod.mode = req_p->req.chmod.mode;

	return(do_chmod(sock, &fakereq, f_p->f_name, ack_p));
} /* end of do_fchmod() */

/* do_rmdir() - removes a directory in the file system
 */
int do_rmdir(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	req_p->req.rmdir.fname  = data_p;

	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}

	ack_p->status = md_rmdir(req_p);
	ack_p->eno  = errno;
	if (!ack_p->status)
		return(bsend(sock, ack_p, sizeof(mack)));
	bsend(sock, ack_p, sizeof(mack));
	errno = ack_p->eno; /* return the old error */
	return(-1);
} /* end of do_rmdir() */

/* DO_GETDENTS() - opens a directory, seeks to the appropriate location,
 * reads directory data out, and pushes it back through the socket.
 *
 * The common case for this with the kernel module is a request for a
 * single entry.  The common case for this for the library is somewhat
 * larger.  We need to strip out the reserved names from the output
 * here.
 */
int do_getdents(int sock, mreq_p req_p, void *data_p, mack_p ack_p) {
	int i = -1, myerr = 0, ret;
	char *orig = NULL, *last = NULL;
	off_t off;
	size_t size;

	/* should do permission checking */

	req_p->req.getdents.dname  = data_p;

	/* check for reserved name first */
	if (resv_name(data_p) != 0) {
		ack_p->status = -1;
		ack_p->eno = ENOENT;
		return(send_error(-1, errno, sock, ack_p));
	}
	if (!(orig = (char *)malloc((int)req_p->req.getdents.length)))
		goto do_getdents_error;

	LOG3("do_getdents: name = %s, offset = %Ld, size = %Ld\n",
		req_p->req.getdents.dname, req_p->req.getdents.offset, 
		req_p->req.getdents.length);

	if ((i = open(req_p->req.getdents.dname, O_RDONLY)) < 0)
		goto do_getdents_error;

	off = req_p->req.getdents.offset;
	size = req_p->req.getdents.length;
	last = NULL;

	/* seek to our starting position */
	if (lseek(i, off, SEEK_SET) < 0)
		goto do_getdents_error;

	/* read into the buffer until we get at least one unfiltered entry,
	 * hit EOF, or get an error of some kind
	 */
	while (last == NULL) {
		if ((ret = syscall(SYS_getdents, i, (struct dirent *)orig, size)) <= 0)
		{
			myerr = errno;
			break;
		}

		last = filter_dirents(orig, ret);

	}
	/* save our position */
	off = lseek(i, 0, SEEK_CUR);

	/* send back the final position */
	ack_p->status = (ret < 0) ? -1 : 0;
	ack_p->eno = myerr;
	ack_p->ack.getdents.offset = off;
	size = (last == NULL) ? 0 : last - orig + ((struct dirent *)last)->d_reclen;
	ack_p->dsize = size;

	/* everything went ok, send data */
	bsend(sock, ack_p, sizeof(mack));
	bsend(sock, orig, ack_p->dsize);
	free(orig);
	close(i);
	return(0);

do_getdents_error:
	myerr = errno;
	free(orig);
	if (i >= 0) close(i);
	ack_p->dsize = 0;
	errno = myerr;
	return(send_error(-1, errno, sock, ack_p));
} /* end of do_getdents() */

/* DO_ALL_IMPLICIT_CLOSES(),
 * DO_IMPLICIT_CLOSES(),
 * CHECK_FOR_IMPLICIT_CLOSE()
 *
 * These functions are used to clean up after applications when they
 * exit.  All files that are open are checked to see if they were opened
 * by the application holding open the socket, and if so the files are
 * "closed" (reference count is decremented and files are closed if
 * count reaches zero).
 */
int do_all_implicit_closes(int sock) {
	iclose_sock = sock;
	forall_fs(active_p, do_implicit_closes);
	return(0);
}

int do_implicit_closes(void *v_p) {
	fsinfo_p fs_p = (fsinfo_p) v_p;
	iclose_fsp = fs_p;
	forall_finfo(fs_p->fl_p, check_for_implicit_close);
	return(0);
}

int check_for_implicit_close(void *v_p) {
	finfo_p f_p = (finfo_p) v_p;
	ireq iodreq;
	int ret;

	if (dfd_isset(iclose_sock, &f_p->socks)) {
		LOG2("socket %d found in mask for file %ld\n", iclose_sock, f_p->f_ino);
		if (--f_p->cnt > 0) /* not the last reference */ {
			dfd_clr(iclose_sock, &f_p->socks);
		}
		else /* need to close file on IODs */ {
			LOG1("file %ld implicitly closed\n", f_p->f_ino);
			/* send request to close to IODs */
			iodreq.majik_nr = IOD_MAJIK_NR;
			iodreq.type     = IOD_CLOSE;
			iodreq.dsize    = 0;
			iodreq.req.close.f_ino = f_p->f_ino;
			/* Tells iod to close all instances of open file */
			iodreq.req.close.cap = -1; 
			
			if ((ret = (send_req(iclose_fsp->iod, iclose_fsp->nr_iods,
				f_p->p_stat.base, f_p->p_stat.pcount, &iodreq, NULL, NULL)) < 0))
			{
				PERROR("send_req failed during implicit close\n");
				return(-1);
			}

			if (f_p->unlinked >= 0) /* tell IODs to remove the file too */ {
				iodreq.majik_nr = IOD_MAJIK_NR;
				iodreq.type     = IOD_UNLINK;
				iodreq.dsize    = 0;
				iodreq.req.unlink.f_ino = f_p->f_ino;
				if ((ret = (send_req(iclose_fsp->iod, iclose_fsp->nr_iods,
					f_p->p_stat.base, f_p->p_stat.pcount, &iodreq, NULL,
					NULL)) < 0))
				{
					PERROR("send_req failed during implicit close (unlink)\n");
					return(-1);
				}
			}
			close(f_p->unlinked);
			f_rem(iclose_fsp->fl_p,f_p->f_ino); /* wax file info */
		}
	}
	return(0);
} /* end of check_for_implicit_close() */

/* stop_iods() - closes connections to idle iods, lets them know that we
 * don't have any files open.  Name is something of a misnomer left over
 * from a time when the manager did actually start and stop the I/O
 * daemons.
 *
 * Returns 0.
 */
int stop_iods(void *v_p) {
	int i;
	ireq iodreq;
	fsinfo_p fs_p = (fsinfo_p) v_p;

	/* shut down all IODs */
	iodreq.majik_nr = IOD_MAJIK_NR;
	iodreq.type     = IOD_SHUTDOWN;
	iodreq.dsize    = 0;
	if (send_req(fs_p->iod, fs_p->nr_iods, 0, fs_p->nr_iods, &iodreq, 
		NULL, NULL) < 0) return(-1);
	for (i = 0; i < fs_p->nr_iods; i++) {
		close(fs_p->iod[i].sock);
		fs_p->iod[i].sock = -1;
	}

	return(0);
} /* end of stop_iods() */

/* send_req() - sends a request to a number of iods
 * iod[]  - pointer to array of iod_info structures
 * iods   - number of iods in array
 * base   - base number for file
 * pcount - number of iods on which the file resides
 * req_p  - pointer to request to send to each iod
 *        this may be modified by send_req() to update values in request
 *        such as the pnum value in an OPEN request
 * data_p - pointer to any request trailing data to be sent; size is
 *        stored in req_p->dsize; may be NULL
 * ack_p  - pointer to iack structure; NOT USED...
 *
 * Returns 0 on success, non-zero on failure.
 */
int send_req(iod_info iod[], int iods, int base, int pcount, ireq_p req_p, 
				void *data_p, iack_p ack_p)
{
	int ret, i;
	int cnt=base, errs=0, myerr=0;

	if (!iod || !req_p) {
		ERR("NULL pointer passed to send_req()\n");
		errno = EINVAL;
		return(-1);
	}

	if (base >= iods || pcount > iods || iods < 1) {
		ERR("Invalid value passed to send_req (base, pcount, or nr_iods)\n");
		errno = EINVAL;
		return(-1);
	}

	for (i = 0; i < pcount;
		cnt = (cnt + 1)%iods, 
		(req_p->type == IOD_OPEN) ? req_p->req.open.pnum++ : 1, /* ugly!  */
		(req_p->type == IOD_TRUNCATE) ? req_p->req.truncate.part_nr++ : 1,
		i++)
	{
		/* clear errno */
		iod[cnt].ack.eno = 0;
		iod[cnt].ack.status = 0;

		if (iod[cnt].sock < 0) /* open the connection first */ {
			/* get socket, connect */
			if ((iod[cnt].sock = new_sock()) == -1) {
				iod[cnt].ack.eno = errno;
				iod[cnt].ack.status = -1;
				PERROR("new_sock"); 
				errs++;
				continue;
			}
			/* connect */
			if (connect(iod[cnt].sock, (struct sockaddr *)&(iod[cnt].addr),
				sizeof(iod[cnt].addr)) < 0)
			{
				iod[cnt].ack.eno = errno;
				iod[cnt].ack.status = -1;
				ERR3("error connecting to iod %d (%s:%d)\n", cnt,
					inet_ntoa(iod[cnt].addr.sin_addr), 
					ntohs(iod[cnt].addr.sin_port));
				invalidate_conn(&iod[cnt]);
				errs++;
				continue;
			}	
		}
		if ((ret = bsend(iod[cnt].sock, req_p, sizeof(ireq))) < 0) {
			/* error sending request */
			iod[cnt].ack.eno = errno;
			iod[cnt].ack.status = -1;
			ERR3("error sending request to iod %d (%s:%d)\n", cnt,
				inet_ntoa(iod[cnt].addr.sin_addr), 
				ntohs(iod[cnt].addr.sin_port));
			invalidate_conn(&iod[cnt]);
			errs++;
			continue;
		}
		if (req_p->dsize > 0 && data_p
		&& (ret = bsend(iod[cnt].sock, data_p, req_p->dsize)) < 0) {
			/* error sending trailing data */
			iod[cnt].ack.eno = errno;
			iod[cnt].ack.status = -1;
			ERR3("error sending trailing data to iod %d (%s:%d)\n", cnt,
				inet_ntoa(iod[cnt].addr.sin_addr), 
				ntohs(iod[cnt].addr.sin_port));
			invalidate_conn(&iod[cnt]);
			errs++;
			continue;
		}
		if ((ret = brecv(iod[cnt].sock, &(iod[cnt].ack), sizeof(iack))) < 0) {
			/* error receiving ack */
			iod[cnt].ack.eno = errno;
			iod[cnt].ack.status = -1;
			ERR3("error receiving ack from iod %d (%s:%d)\n", cnt,
				inet_ntoa(iod[cnt].addr.sin_addr), 
				ntohs(iod[cnt].addr.sin_port));
			invalidate_conn(&iod[cnt]);
			errs++;
			continue;
		}
	} /* end of forall iods */
	for (i=0, cnt=base; i < iods; i++, cnt=(cnt+1)%iods) {
		if (iod[cnt].ack.status) {
			ERR2("error received from iod %d: %s\n", cnt,
				strerror(iod[cnt].ack.eno));
			errno = iod[cnt].ack.eno;
			return(iod[cnt].ack.status);
		}
	}
	if (errs) {
		errno = EIO;
		return(-1);
	}
	return(0);
} /* end of send_req() */

/* invalidate_conn() - closes an iod socket and removes references to it
 *
 * Returns -1 on obvious error, 0 on success.
 */
int invalidate_conn(iod_info *info_p)
{
	if (!info_p) {
		ERR("NULL iod_info pointer passed to invalidate_conn()\n");
		return(-1);
	}
	close(info_p->sock);
	info_p->sock = -1;
	return(0);
}

/* send_error() - sends an error ack back to an app if a valid
 * socket number (>= 0) is given, and returns -1.
 *
 * sets errno to the value passed in as p_errno
 */
int send_error(int status, int p_errno, int sock, mack_p ack_p) 
{
	if (sock >= 0) {
		LOG("*** SENDING ERROR ACK ***\n");
		ack_p->status = status;
		ack_p->eno  = p_errno;
		ack_p->dsize  = 0;
		bsend(sock, ack_p, sizeof(mack));
	}
	else {
		LOG("*** ERROR - NOT SENDING ACK ***\n");
	}
	errno = p_errno; /* preserve original errno */
	return(-1);
} /* end of send_error() */

/* quick_mount() - checks if file system holding a given file is mounted;
 * if not, it mounts it
 */
fsinfo_p quick_mount(char *fname, int uid, int gid, mack_p ack_p) {
	int ret;
	static mreq fakereq;
	fsinfo_p fs_p;
	dmeta dir;

	/* fill in fake request for do_mount() */
	if (get_dmeta(fname, &dir) < 0) {
		PERROR("quick_mount: get_dmeta");
		return(NULL);
	}

	if ((fs_p = fs_search(active_p, dir.fs_ino)) != 0) return(fs_p);

	LOG1("quick_mount: mounting %s\n", dir.rd_path);
	/* need a / on the end of the root path - hack it - WBL */
	/* this trashes the sd_path, but that shouldn't matter */
	*(dir.sd_path-1) = '/';
	*(dir.sd_path) = '\0';
	fakereq.type  = MGR_MOUNT;
	fakereq.uid   = uid;
	fakereq.gid   = gid;
	fakereq.dsize = strlen(dir.rd_path);
	/* mount fs */
	if ((ret = do_mount(-1, &fakereq, dir.rd_path, ack_p)) < 0) {
		PERROR("quick_mount: do_mount");
		return(NULL);
	}
	/* find filesystem & open file */
	return (fs_search(active_p, dir.fs_ino));
} /* end of quick_mount() */

int timeout_fns()
{
	forall_fs(active_p, stop_idle_iods);
	return(0);
}

int stop_idle_iods(void *v_p)
{
	int ret, ret2;
	fsinfo_p fs_p = (fsinfo_p) v_p;

	if (flist_empty(fs_p->fl_p)) {
		LOG("Closing idle iod connections\n");
		ret = stop_iods(v_p);
		ret2 = fs_rem(active_p, fs_p->fs_ino);
		return(MIN(ret, ret2));
	}
	return(0);
} /* end of stop_idle_iods() */

void *restart(int sig_nr)
{
	LOG("pvfsmgr: received HUP signal\n");

	forall_fs(active_p, stop_iods);
	LOG("Shut down all iods\n");
	fslist_cleanup(active_p);
	active_p = fslist_new(); /* initialize new filesystem list */
	LOG("Cleared filesystem list\n");
	signal(SIGHUP, (void *)restart);

	return(0);
} /* end of restart() */

void *mgr_shutdown(int sig_nr)
{
	forall_fs(active_p, stop_iods);
	LOG("Closed all iod connections\n");
	cleanup();
	return(0);
}

/* do_signal() - catch signals, reset handler, die on SIGSEGV */
static void *do_signal(int sig_nr)
{
	ERR1("do_signal: got signal %d\n", sig_nr);
	if (sig_nr == SIGPIPE) {
		signal(sig_nr, (void *)do_signal);
		return(0);
	}

	ERR("\nOPEN FILES:\n");
	fslist_dump(active_p);
	cleanup();
	return(0);
}

/* do_statusdump() - spit out good info, reset signal handler */
static void *do_statusdump(int sig_nr)
{
	ERR("\nopen files:\n");
	fslist_dump(active_p);

	printf("read set:\n");
	dfd_dump(&socks.read);
	printf("write set:\n");
	dfd_dump(&socks.write);
	printf("tmp_read set:\n");
	dfd_dump(&socks.tmp_read);
	printf("tmp_write set:\n");
	dfd_dump(&socks.tmp_write);

	fflush(stdout);
	fflush(stderr);

	signal(sig_nr, (void *)do_statusdump);
	
	return (void *)0;
}
