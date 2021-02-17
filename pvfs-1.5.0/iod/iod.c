/*
 * iod.c copyright (c) 1998-2000 Clemson University, all rights reserved.
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

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <req.h>
#include <flist.h>
#include <jlist.h>
#include <alist.h>
#include <signal.h>
#include <sys/vfs.h>
#include <sockset.h>
#include <sockio.h>
#include <iod.h>
#include <debug.h>
#include <values.h>
#include <config.h>
#include <dirent.h>
#include <jobs.h>

void *do_restart(int s);
int verify_dirs(void);
int move_files(void);
int startup(int argc, char **argv);
int new_request(int s);

/* GLOBALS */
flist_p open_p;   			/* list of open files */
jlist_p jobs_p; 				/* list of active jobs */
sockset socks;

int debug_on = 0;          /* toggles debugging output */
iodstat jobstat[STATNUM];  /* pointers to iodstats  */
int stat_ind;              /* index into iodstats	 */
int timing_on = 0;         /* toggles timing record */
int acc;                   /* accept socket, global so restart can work */

/* NOTE: THESE MUST BE IN SAME ORDER AS REQUESTS IN REQ.H!!! */
static int (*reqfn[])(int, ireq_p, iack_p) = {
	do_open_req,
	do_close_req,
	do_stat_req,
	do_unlink_req,
	do_rw_req,
	do_shutdown_req,
	do_noop_req,
	do_noop_req,
	do_instr_req,
	do_ftruncate_req,
	do_truncate_req,
	do_fdatasync_req,
	do_noop_req,
	do_statfs_req
};

static char *reqtext[] = {
	"open",
	"close",
	"stat",
	"unlink",
	"rw",
	"shutdown",
	"gstart - removed",
	"gadd - removed",
	"instr",
	"ftruncate",
	"truncate",
	"fdatasync",
	"noop",
	"statfs"
};

static char iod_c_hdr[] = "$Header: /projects/cvsroot/pvfs/iod/iod.c,v 1.23 2001/01/16 00:19:13 rbross Exp $";

/* main()
 *
 */
int main(int argc, char **argv)
{
	int s, ret, cnt;
	jinfo_p j_p;

	/* call startup to get things rolling */
	if ((acc = startup(argc, argv)) == -1) {
		ERR("startup failed; exiting.\n");
		exit(-1);
	}
	fprintf(stderr, "%s\n", iod_c_hdr);

	for (;;) {
		/* wait for something to happen */
		if ((cnt = check_socks(&socks, -1)) < 0) {
			PERROR("main: check_socks");
			return -1;
		}
		/* note: nextsock() removes returned sock # from ready set */
		while ((s = nextsock(&socks)) >= 0) /* still some sockets ready */ {
			if (!(j_p = j_search(jobs_p, s))) /* no job for socket */ {
				new_request(s);
			}
			else /* there's a job - work on it */ {
				if ((ret = do_job(s, j_p)) < 0) /* socket crapped out! */ {
					LOG1("main: socket %d appears to have died\n", s);
					delsock(s, &socks);
					clr_active(s);
					close(s);
					jlist_purge_sock(jobs_p, s); /* remove all job refs to sock */
					flist_purge_sock(open_p, s); /* remove all file refs to sock */
				}	
			}
		}
	}
	return -1; /* should never get here */
}

/* new_request()
 *
 * Handles receiving new requests and calling the appropriate handler
 * function.
 *
 * Returns 0 on success, -errno on failure.
 */
int new_request(int s)
{
	int myerr, ret;
	ireq req;
	iack ack;

	if (s == acc) {
		if ((s = accept(acc, NULL, 0)) == -1) {
			myerr = errno;
			PERROR("new_request: accept");
			return -myerr;
		}
		/* kill Nagle */
		if (set_tcpopt(s, TCP_NODELAY, 1) < 0) {
			myerr = errno;
			PERROR("set_tcpopt");
			close(s);
			return -myerr;
		}
		/* set socket buffer sizes (send and receive) */
		if ((set_sockopt(s, SO_SNDBUF, IOD_SOCKET_BUFFER_SIZE) < 0)
		|| (set_sockopt(s, SO_RCVBUF, IOD_SOCKET_BUFFER_SIZE) < 0)) {
			myerr = errno;
			PERROR("set_tcpopt");
			close(s);
			return -myerr;
		}

		LOG1("new conn. on %d\n", s);
	}
	if ((ret = brecv(s, &req, sizeof(req))) < (int) sizeof(req)) {
		myerr = errno;
		if (errno != EPIPE) {
			PERROR("new_request: brecv");
		}
		delsock(s, &socks);
		clr_active(s);
		close(s);
		LOG1("socket %d has been closed\n", s);
		return -myerr;
	} 

	/* check to make sure the request is valid */
	if (req.majik_nr != IOD_MAJIK_NR) {
		ERR("new_request: invalid magic number; ignoring request\n");
		send_error(-1, -1, s, &ack);
		delsock(s, &socks);
		clr_active(s);
		close(s);
		return -EINVAL;
	}
	if (req.type < 0 || req.type > MAX_IOD_REQ) {
		ERR2("new_request: invalid req type (%d) on socket %d\n",
			req.type, s);
		send_error(-1, -1, s, &ack);
		delsock(s, &socks);
		clr_active(s);
		close(s);
		return -EINVAL;
	}

	/* call the appropriate function */
	ack.majik_nr = IOD_MAJIK_NR;
	ack.type = req.type;
	ack.status = ack.eno = ack.dsize  = 0;

	LOG2("req: %s (sock %d)\n", reqtext[req.type], s);

	if ((ret = (reqfn[req.type])(s, &req, &ack)) < 0) {
		ERR1("new_request: %s failed\n", reqtext[req.type]);
	}

	/* if request is done, watch for next one */
	if (ret >= 0) {
		if (req.type != IOD_RW && req.type != IOD_GADD
		&& req.type != IOD_SHUTDOWN)
		{
			/* check socket for reading */
			addsockrd(s, &socks);
		}

		/* set active sockets as active */
		if (req.type != IOD_GADD && req.type != IOD_SHUTDOWN)
			set_active(s);
	}
	
	return 0;
}

/* startup()
 *
 * Returns open fd on success, -1 on failure.
 */
int startup(int argc, char **argv)
{
	int fd, port = IOD_BASE_PORT, i;
	char logname[] = "iolog.XXXXXX";
	char logfile[1024];

	if (argc > 1) /* grab config file name off cmd line */ {
		if (parse_config(argv[1]) < 0) {
			ERR2("error parsing config file %s: %s\n", argv[1],
				strerror(errno));
			return(-1);
		}
	}
	else if (parse_config("/etc/iod.conf") < 0) {
		ERR2("error parsing config file %s: %s\n", "/etc/iod.conf",
			strerror(errno));
		return(-1);
	}

	/* open up log file and redirect stderr to it */
#ifndef TESTING
	snprintf(logfile, 1024, "%s/%s", get_config_logdir(), logname);
	debug_on = get_config_debug();
	if ((fd = mkstemp(logfile)) == -1) return(-1);
	fchmod(fd, 0755);
	dup2(fd, 2);
	dup2(fd, 1);
#endif

	port = get_config_port();

	/* grab a socket for receiving requests */
	if ((fd = new_sock()) == -1) return(-1);

	/* set up for fast restart */
	set_sockopt(fd, SO_REUSEADDR, 1);

	if (bind_sock(fd, port) == -1) {
		ERR("error binding to port\n");
		return(-1);
	}

	if (listen(fd, IOD_BACKLOG) == -1) return(-1);

	/* be rude about scheduling */
	if (!geteuid()) nice(-20);

	/* set up environment */
	if (set_config() < 0) {
		FILE *fp;
		ERR("error setting up environment from config file\n");
		fp = fdopen(1, "w");
		dump_config(fp);
		return(-1);
	}

	/* become a daemon */
#ifndef TESTING
	close(0);
	if (fork()) exit(0);
	setsid();
#endif

	/* initialize any lists, etc. */
	open_p   = flist_new();
	jobs_p = jlist_new();
	stat_ind = 0;
	for (i = 0; i < STATNUM; i++) {
		jobstat[i].numaccess  		= -1;		
		jobstat[i].st_serv.tv_sec	= -1;
		jobstat[i].st_serv.tv_usec = -1;
		jobstat[i].offset				= -1;
	}	
	initset(&socks);
	addsockrd(fd, &socks);
	set_active(fd);


	/* catch SIGHUP signals */
#ifdef USE_SIGACTION
	act.sa_handler = (void *)do_restart;
	act.sa_mask    = 0;
	act.sa_flags   = 0;
	if (sigaction(SIGHUP, &act, NULL) < 0) {
		perror("pvfsmgr: sigaction");
		return(-1);
	}
#else
	signal(SIGHUP, (void *)do_restart);
#endif

	/* catch SIGSEGV and SIGPIPE signals */
#ifdef USE_SIGACTION
	act.sa_handler = (void *)do_signal;
	act.sa_mask    = 0;
	act.sa_flags   = 0;
	if (sigaction(SIGSEGV, &act, NULL) < 0) {
		perror("pvfsmgr: sigaction");
		return(-1);
	}
	if (sigaction(SIGPIPE, &act, NULL) < 0) {
		perror("pvfsmgr: sigaction");
		return(-1);
	}
#else
	signal(SIGSEGV, (void *)do_signal);
	signal(SIGPIPE, (void *)do_signal);
#endif

	/* see if we can write into our working directory */
	if ((i = open("testopen", O_RDWR | O_CREAT, 0777)) < 0) {
		PERROR("cannot open files in working directory");
		return(-1);
	}
	close(i);
	unlink("testopen");

	/* ensure all data subdirectories are present and accessable */
	if ((i = verify_dirs()) < 0) {
		errno = -i;
		PERROR("error verifying data subdirectories");
		return -1;
	}

	/* move any data files into the subdirectories if they are in root */
	if ((i = move_files()) < 0) {
		errno = -i;
		PERROR("error moving files into subdirectories");
		return -1;
	}

	/* return socket for accepting connections */
	return(fd);
}

/* NOTE: do_job() and do_rw_req() both in jobs.c now */

int do_unlink_req(int sock, ireq_p r_p, iack_p a_p) {
	int ret;

	if ((ret = unlink(fname(r_p->req.unlink.f_ino))) < 0) {
		return(send_error(ret, errno, sock, a_p));
	}

	if (bsend(sock, a_p, sizeof(iack)) < 0) return(-1);
	return(0);
}

int do_fdatasync_req(int sock, ireq_p r_p, iack_p a_p) {
	finfo_p f_p;
	int err;

	if (!(f_p = f_search(open_p, r_p->req.ftruncate.f_ino,
		r_p->req.ftruncate.cap)))
	{
		return(send_error(-1, ENOENT, sock, a_p));
	}
	if ((err = fsync(f_p->fd)) < 0) {
		return(send_error(err, errno, sock, a_p));
	}
	if (bsend(sock, a_p, sizeof(iack)) < 0) return(-1);
	return(0);
}

int do_truncate_req(int sock, ireq_p r_p, iack_p a_p) {
	off_t len = r_p->req.truncate.length;
	int nr_sfrags, leftovers, my_size, err;
	int part_nr = r_p->req.truncate.part_nr;

	/* calculate our new file location... for comments see ftruncate */
	len -= r_p->req.truncate.p_stat.soff;
	nr_sfrags = len / r_p->req.truncate.p_stat.ssize;
	leftovers = len % r_p->req.truncate.p_stat.ssize;
	my_size = nr_sfrags / r_p->req.truncate.p_stat.pcount;

	if (part_nr < nr_sfrags % r_p->req.truncate.p_stat.pcount) my_size++;
	my_size *= r_p->req.truncate.p_stat.ssize; /* now in bytes */

	if (part_nr == nr_sfrags % r_p->req.truncate.p_stat.pcount)
		my_size += leftovers;
	
	LOG2("truncating %s to %d\n", fname(r_p->req.truncate.f_ino), my_size);
	if ((err = truncate(fname(r_p->req.truncate.f_ino), my_size)) < 0) {
		return(send_error(err, errno, sock, a_p));
	}

	if (bsend(sock, a_p, sizeof(iack)) < 0) return(-1);
	return(0);
}

int do_ftruncate_req(int sock, ireq_p r_p, iack_p a_p) {
	finfo_p f_p;
	off_t len = r_p->req.ftruncate.length;
	int nr_sfrags, leftovers, my_size, err;

	if (!(f_p = f_search(open_p, r_p->req.ftruncate.f_ino,
		r_p->req.ftruncate.cap)))
	{
		return(send_error(-1, ENOENT, sock, a_p));
	}

	/* calculate our new file location... */
	len -= f_p->p_stat.soff;
	nr_sfrags = len / f_p->p_stat.ssize; /* number of whole stripe frags */
	leftovers = len % f_p->p_stat.ssize; /* partial stripe frag size */
	my_size = nr_sfrags / f_p->p_stat.pcount; /* have at least this much */
	if (f_p->pnum < nr_sfrags % f_p->p_stat.pcount) my_size++;
	my_size *= f_p->p_stat.ssize; /* now in bytes */
	if (f_p->pnum == nr_sfrags % f_p->p_stat.pcount) my_size += leftovers;
	
	LOG2("ftruncating %s to %d\n", fname(r_p->req.ftruncate.f_ino), my_size);
	if ((err = ftruncate(f_p->fd, my_size)) < 0) {
		return(send_error(err, errno, sock, a_p));
	}

	if (bsend(sock, a_p, sizeof(iack)) < 0) return(-1);
	return(0);
}

int do_open_req(int sock, ireq_p r_p, iack_p a_p) {
	int fd, ret;
	finfo_p f_p;
	/* find & open file */
do_open_req_open_file:
	if ((fd = open(fname(r_p->req.open.f_ino), r_p->req.open.flag,
	r_p->req.open.mode)) < 0)
	{
		/* NOTE:
		 * We were getting an error here when the file existed already and
		 * touch was used by a client to try to create the file (passes
		 * O_CREAT|O_TRUNC|O_WRONLY|O_LARGEFILE).
		 *
		 * For now I'm going to report the problem and do a little magic
		 * to let things keep going.
		 *
		 * This might break O_EXCL, but then it probably wasn't working
		 * right before...  -- Rob
		 */
		if (errno == EEXIST) {
			char savename[512];

			ERR2("open: %s exists (flags = %x); saving\n",
			fname(r_p->req.open.f_ino), r_p->req.open.flag);

			/* rename the file, unlinking any previously saved file.
			 * if the rename fails, error out (avoid infinite loop).
			 */
			strncpy(savename, fname(r_p->req.open.f_ino), 501);
			strcat(savename, ".saveme");
			unlink(savename);
			if (rename(fname(r_p->req.open.f_ino), savename) == 0)
				goto do_open_req_open_file;
		}
		return(send_error(fd, errno, sock, a_p));
	}
	LOG1("File: %s\n", fname(r_p->req.open.f_ino));

	/* add file to list of open files */
	if (!(f_p = f_new())) {
		return(send_error(-1, errno, sock, a_p));
	}
	f_p->f_ino       = r_p->req.open.f_ino;
	f_p->cap         = r_p->req.open.cap;
	f_p->p_stat      = r_p->req.open.p_stat;
	f_p->pnum        = r_p->req.open.pnum;
	f_p->fd          = fd;

	if ((ret = f_add(open_p, f_p)) < 0) {
		return(send_error(ret, errno, sock, a_p));
	}
	/* send ack */
	if (bsend(sock, a_p, sizeof(iack)) < 0) return(-1);
	return(0);
}

int do_close_req(int sock, ireq_p r_p, iack_p a_p) {
	finfo_p f_p;
	int i=0;

	/* find file (and cap) in list; the cap for mgr requests will
	 * be -1, which indicates that ALL instances of the file (inode)
	 * should be closed.  f_search() correctly handles the cap == -1 by 
	 * returning a pointer to an open instance if there is one with a 
	 * matching inode number.
	 */
	while ((f_p = f_search(open_p, r_p->req.close.f_ino, r_p->req.close.cap)))
	{
		i++;
		/* unmap file if mapped */
		if (f_p->mm.mloc) munmap(f_p->mm.mloc, f_p->mm.msize);

		/* close file */
		if (f_p->fd) close(f_p->fd);

		/* remove from list (also frees item) */
		f_rem(open_p, f_p->f_ino, f_p->cap);
	}
	if (!i && r_p->req.close.cap != -1) {
		ERR3("do_close_req: failed (i=%d, ino=%lu, cap=%d)\n", i,
			(unsigned long) r_p->req.close.f_ino, r_p->req.close.cap);
		return(send_error(-1, ENOENT, sock, a_p));
	}

	/* send ack */
	if (bsend(sock, a_p, sizeof(iack)) < 0) return(-1);
	return(0);
}

/* do_statfs_req()
 *
 * Determines total and free bytes on file system used to store data.
 */
int do_statfs_req(int sock, ireq_p r_p, iack_p a_p) {
	struct statfs sfs;

	a_p->type = IOD_STATFS;
	a_p->status = statfs(".", &sfs);
	a_p->ack.statfs.tot_bytes = ((int64_t)sfs.f_blocks)*((int64_t)sfs.f_bsize);
	a_p->ack.statfs.free_bytes = ((int64_t)sfs.f_bavail)*((int64_t)sfs.f_bsize);

	/* send ack */
	if (bsend(sock, a_p, sizeof(iack)) < 0) return(-1);
	return(0);
}

int do_stat_req(int sock, ireq_p r_p, iack_p a_p) {
#ifndef LARGE_FILE_SUPPORT
	struct stat filestat;
	/* find the file and get the length */
	a_p->status = stat(fname(r_p->req.stat.f_ino), &filestat);	
	a_p->ack.stat.fsize = filestat.st_size;
#else
#endif
	a_p->type = IOD_STAT;
	/* send ack */
	if (bsend(sock, a_p, sizeof(iack)) < 0) return(-1);
	return(0);
}

/*  Type of data: Start Wait = 10  		*/
/*	 					Start Serv.= 11		*/
/*						End Serv	  = 12    	*/
/*  Flags are declared in req.h			*/
int do_instr_req(int sock, ireq_p r_p, iack_p a_p) {
	float avg = 0;
	int  i = 0, total = 0, nr_acc = 0;
	long wait = 0;

	/* handle instrumentation requests  		*/	
	if (r_p->req.instr.flags & INSTR_OFF) {
		timing_on = 0;
	}
	if (r_p->req.instr.flags & INSTR_PRNT) {
		for (i = 0; i < STATNUM; i++) {
			if(jobstat[i].numaccess > 0) {
				fprintf(stderr, "%d\t%d\t10\t%d\t%d\t%d\n", i, jobstat[i].fd,
						(int) jobstat[i].st_wait.tv_sec,
						(int) jobstat[i].st_wait.tv_usec,
						jobstat[i].type); 
				fprintf(stderr, "%d\t%d\t11\t%d\t%d\t%d\n", i, jobstat[i].fd,
						(int) jobstat[i].st_serv.tv_sec,
						(int) jobstat[i].st_serv.tv_usec, 
						jobstat[i].offset); 
				fprintf(stderr, "%d\t%d\t12\t%d\t%d\t%d\n", i, jobstat[i].fd,
						(int) jobstat[i].end_serv.tv_sec,
						(int) jobstat[i].end_serv.tv_usec,
						jobstat[i].numaccess); 
				fflush(stderr);
				nr_acc  += jobstat[i].numaccess;
				total++;
			}
		}	
		if (total > 0) {
			a_p->ack.instr.avg = (float)avg;
			a_p->ack.instr.wait = (float)wait;
			a_p->ack.instr.accesses = nr_acc;
		}
	}	
	if (r_p->req.instr.flags & INSTR_RST){
		/* reset structures */
		for (i = 0; i < STATNUM; i++) {
			jobstat[i].numaccess  		= -1;		
			jobstat[i].st_serv.tv_sec	= -1;
			jobstat[i].st_serv.tv_usec = -1;
			jobstat[i].offset				= -1;
		}	
		stat_ind = 0;
	}	
	if (r_p->req.instr.flags & INSTR_ON) {
		timing_on = 1;
	}
	a_p->status = 0;
	a_p->type  = IOD_INSTR;
	if (bsend(sock, a_p, sizeof(iack)) < 0) return(-1);
	return(0);
}

int do_shutdown_req(int sock, ireq_p r_p, iack_p a_p) {
	bsend(sock, a_p, sizeof(iack));
	do_restart(-1);
	return(0);
}


int do_noop_req(int sock, ireq_p r_p, iack_p a_p) {
	if (bsend(sock, a_p, sizeof(iack)) < 0) return(-1);
	return(0);
}


/* verify_dirs()
 *
 * Ensures that all subdirectories for file data storage are present,
 * creating any which are not.
 *
 * If this function is changed, the fname() function should also be
 * updated.
 *
 * Returns 0 on success, -errno on failure.
 */
int verify_dirs(void)
{
	int i, fd, error;
	char buf[64], dir[32];

	for (i=0; i < IOD_DIR_HASH_SZ; i++) {
		sprintf(buf, "%03d/testopen", i);
		/* try to touch a file in the directory */
		/* see if we can write into our working directory */
		if ((fd = open(buf, O_RDWR | O_CREAT, 0777)) < 0) {
			error = -errno;
			if (error != -ENOENT) return error;
			else {
				/* create the directory if possible */
				sprintf(dir, "%03d", i);
				if (mkdir(dir, 0700) < 0) {
					error = -errno;
					return error;
				}
			}
		}
		close(fd);
		unlink(buf);
	}
	return 0;
}


/* move_files()
 *
 * Takes care of any data files which were created in the root
 * directory by moving them into the appropriate subdirectory.  This
 * function is mainly here to transparently handle migration from the
 * flat directory scheme to the hashed one, but it could also be used if
 * the hash size is changed (simply move all the files in the hash
 * directories into the root and restart the iod).
 *
 * This function assumes that we have already changed to the appropriate
 * directory.  It also assumes that the file format is still the
 * "fXXXXXX.Y" format, where the X's represent the inode number.
 *
 * The do...while loop is there because it's not clear what will happen
 * when we're moving files out of the directory while reading it.  We'll
 * just reopen and try again every time we've moved out a file until
 * such time as we can pass through the entire directory list without
 * making a change.
 *
 * Returns 0 on success, -errno on failure.
 */
int move_files(void)
{
	int count, i;
	ino_t inode;
	DIR *dp;
	struct dirent *dent;

	do {
		if ((dp = opendir(".")) == NULL) return -errno;

		count = 0;
		while((dent = readdir(dp)) != NULL) {
			if ((i = sscanf(dent->d_name, "f%ld.%*d", &inode)) == 1) {
				/* match found (sscanf doesn't seem to count the %*d as a
				 * match, which is why we are looking for one conversion.
				 */
				count++;
				rename(dent->d_name, fname(inode));
			}
		}
		closedir(dp);
	} while (count > 0);

	return 0;
}


/* fname() - quick little function to build the filename
 */
char *fname(ino_t f_ino) {
	static char buf[1024];
	int dir;

	dir = f_ino % IOD_DIR_HASH_SZ;
	sprintf(buf, "%03d/f%ld.%d", dir, f_ino, 0);
	return(buf);
}

/* send_error() - sends an error ack back to an app and returns -1.
 */
int send_error(int status, int eno, int sock, iack_p ack_p)
{
	ERR("*** SENDING ERROR ***\n");
	ack_p->status = status;
	ack_p->eno    = eno;
	ack_p->dsize  = 0;
	bsend(sock, ack_p, sizeof(iack));
	errno = ack_p->eno; /* preserve error number */
	return(-1);
}


/* MMAP_PAGE() - mmaps a "page" of a file, unmapping any other regions
 *               of the same file that have been mapped
 *
 * - File MUST be correct length, or this call might fail.  Any necessary
 *   file truncation should be done in build_job().
 *
 *   Also, a "page" is defined to be of IOD_ACCESS_PAGE_SIZE in iod.h.  This
 *   is independent of the actual pagesize for the system, which is 
 *   stored in pagesize.  However, it should be a multiple of the system
 *   page size, so we're not going to do a lot of extra checking for
 *   page alignment...
 *
 * - Returns 0 on success, -1 on error
 */
int mmap_page(finfo_p f_p, int64_t offset, int64_t size)
{
	int mmap_size, mmap_offset;

#ifndef LARGE_FILE_SUPPORT
	if (offset + size > MAXINT) {
		ERR("mmap_page: off + size too big!\n");
		errno = EINVAL;
		return(-1);
	}
#endif

	LOG("      Entered mmap_page()\n");
	if ((f_p->mm.mloc != (char *) -1)                 /* already mapped */
	&&	(offset >= f_p->mm.moff)                       /* offset in bounds */
	&&	offset + size <= f_p->mm.moff + f_p->mm.msize) /* size in bounds */ 
	{
		return(0);
	}
	if ((f_p->mm.mloc != (char *) -1)                 /* mapped in wrong spot */
	&& (munmap(f_p->mm.mloc, f_p->mm.msize) == -1))   /* munmap failed */
	{
		f_p->mm.mloc  = (char *) -1;
		f_p->mm.msize = 0;
		PERROR("mmap_page: munmap");
		/* this is a bad thing, but it doesn't keep us from running... */
	}

	if (f_p->fsize < offset + size) /* trying to go past EOF */ {
		LOG2("mmap_page: fsize = %ld; offset + size = %ld\n",
			(long)f_p->fsize, (long)(offset + size));
		return(-1);
	}
	else {
		mmap_offset = PAGENO(offset);
		mmap_size = MAX(size + PAGEOFF(offset), IOD_ACCESS_PAGE_SIZE);
		mmap_size = MIN(mmap_size, f_p->fsize - mmap_offset);
	}

	if ((f_p->mm.mloc = mmap(0, mmap_size, IOD_MMAP_PROT, IOD_MMAP_FLAGS,
		f_p->fd, mmap_offset)) == (char *) -1)
	{
		f_p->mm.msize = 0;
		PERROR("mmap_page: mmap");
		return(-1);
	}
	f_p->mm.moff  = mmap_offset;
	f_p->mm.msize = mmap_size;
	return(0);
} /* end of mmap_page() */


/* NOTE: do_write() and ack_a_new() now in jobs.c */

/* UPDATE_FSIZE()
 */
int update_fsize(finfo_p f_p)
{
	struct stat filestat;
	if (fstat(f_p->fd, &filestat) == -1) return(-1);

	return(f_p->fsize = filestat.st_size);
}


/* COUNT_ACCESS(jlist_p) - returns the number of accesses in a job
 * pointed to by the jinfo_p.
 * This function could be put somewhere else.
 */
int count_access(jinfo_p job_p, int *off_p) {
	int cnt = 0;
	ainfo_p a_p;
	alist_p tmpal_p = job_p->al_p;

	if ((a_p = a_type_search(tmpal_p, 2)) != NULL) {
		*off_p = a_p->u.rw.off;
	} else if ((a_p = a_type_search(tmpal_p, 3)) !=  NULL) {
		*off_p = a_p->u.rw.off;
	}	else *off_p = -2;	
	a_p = a_get_start(tmpal_p);

	while((tmpal_p = tmpal_p->next) != NULL) {
		cnt++;
	}	
	return(cnt);
}

/* DO_SIGNAL()
 */
void *do_signal(int s) {
	ERR1("do_signal: got signal %d\n", s);
	ERR("\nOPEN FILES:\n");
	flist_dump(open_p);

	ERR("\nACTIVE JOBS (INCLUDING ACCESSES):\n");
	jlist_dump(jobs_p);

	if (s == SIGSEGV) exit(-1);
#ifndef USE_SIGACTION
	signal(s, (void *)do_signal);
#endif
	if (s == SIGPIPE) return(0);

	exit(-1);
}

/* DO_RESTART()
 */
void *do_restart(int s) {
	int i;

	LOG("caught SIGHUP or got shutdown request\n");

	/* clear out flist */
	flist_cleanup(open_p);
	open_p = flist_new();

	/* clear out jlist */
	jlist_cleanup(jobs_p);
	jobs_p = jlist_new();
	stat_ind = 0;
	for (i = 0; i < STATNUM; i++) {
		jobstat[i].numaccess  		= -1;		
		jobstat[i].st_serv.tv_sec	= -1;
		jobstat[i].st_serv.tv_usec = -1;
		jobstat[i].offset				= -1;
	}	
	initset(&socks);
	addsockrd(acc, &socks);
	set_active(acc);

	/* close all sockets that we can */
	for (i=0; i < sysconf(_SC_OPEN_MAX); i++) {
		if (i != acc && i != 2) close(i);
	}
#ifndef USE_SIGACTION
	signal(SIGHUP, (void *)do_restart);
#endif
	return(0);
}









