/* Hey Emacs! This file is -*- nroff -*- source.
 * 
 * Copyright (c) 1996 Clemson University, All Rights Reserved.
 * 2000 Argonne National Laboratory.
 * Originally Written by Rob Ross.
 *
 * Parallel Architecture Research Laboratory, Clemson University
 * Mathematics and Computer Science Division, Argonne National Laboratory
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
 **Code** This Protected Resource Provided By = rainmanp7
 * Contact:  
 * Rob Ross			    rross@mcs.anl.gov
 * Matt Cettei			mcettei@parl.eng.clemson.edu
 * Chris Brown			muslimsoap@gmail.com
 * Walt Ligon			walt@parl.clemson.edu
 * Robert Latham		robl@mcs.anl.gov
 * Neill Miller 		neillm@mcs.anl.gov
 * Frank Shorter		fshorte@parl.clemson.edu
 * Harish Ramachandran	rharish@parl.clemson.edu
 * Dale Whitchurch		dalew@parl.clemson.edu
 * Mike Speth			mspeth@parl.clemson.edu
 * Brad Settlemyer		bradles@CLEMSON.EDU
 */

static char pvfsd_c_hdr[] = "$Header: /projects/cvsroot/pvfs-kernel/pvfsd.c,v 1.29 2001/01/12 21:27:37 rbross Exp $";

#include <config.h>

/* UNIX INCLUDES */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syscall.h>
#include <utime.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/resource.h>

/* PVFS INCLUDES */
#include <pvfs_kernel_config.h>
#include <pvfsd.h>
#include <ll_pvfs.h>

#include <pvfs_v1_xfer.h>

/* PROTOTYPES */
static int do_signal(int signr);
static void exiterror(char *string);
static int startup(int argc, char **argv);
static void cleanup(void);
static int open_pvfsdev(void);
static int read_pvfsdev(int fd, struct pvfs_upcall *up, int timeout);
static int write_pvfsdev(int fd, struct pvfs_downcall *down, int timeout);
static void close_pvfsdev(int fd);
static void init_downcall(struct pvfs_downcall *down, struct pvfs_upcall *up);
static int read_op(struct pvfs_upcall *up, struct pvfs_downcall *down);
static int write_op(struct pvfs_upcall *up, struct pvfs_downcall *down);
static void usage(void);

#ifdef MORE_FDS
static int get_filemax(void);
static int set_filemax(int max);
#endif

/* GLOBALS */
static int is_daemon = 1;
static int dev_fd;
char *iobuf = NULL, *orig_iobuf = NULL, *big_iobuf = NULL;
int pvfs_debug = PVFS_DEFAULT_DEBUG_MASK;

/* FUNCTIONS */
int main(int argc, char **argv)
{
	int err;
	struct pvfs_upcall up;
	struct pvfs_downcall down;
	struct pvfs_dirent dent;
	int opt = 0;

	while((opt = getopt(argc, argv, "hd:")) != EOF) {
		switch(opt){
			case 'd':
				err = sscanf(optarg, "%x", &pvfs_debug);
				if(err != 1){
					usage();
					exiterror("bad arguments");
				}
				break;
			case 'h':
				usage();
				return(0);
				break;
			case '?':
				usage();
				exiterror("bad arguments");
				break;
			default: break;
		}
	}
		
	/* do this before we start the log file */
	if ((dev_fd = open_pvfsdev()) < 0)
		exiterror("open_pvfsdev() failed");
	
	startup(argc, argv);

	pvfs_comm_init();

	/* allocate a 64K, page-aligned buffer for small operations */
	if ((orig_iobuf = (char *) malloc(PVFS_OPT_IO_SIZE + 4096)) == NULL)
		exiterror("malloc failed");
	
	iobuf = orig_iobuf + 4096 - ((unsigned long) orig_iobuf % 4096);
	
	/* loop forever, doing:
	 * - read from device
	 * - service request
	 * - write back response
	 */
	for (;;) {
		err = read_pvfsdev(dev_fd, &up, 30);
		if (err < 0) {
			PDEBUG(D_PSDEV, "read_pvfsdev returned 0; idling.\n");
			pvfs_comm_shutdown();
			close_pvfsdev(dev_fd);
			exiterror("read failed\n");
		}
		if (err == 0) {
			/* timed out */
			pvfs_comm_idle();
			continue;
		}

		/* the do_pvfs_op() call does this already; can probably remove */
		init_downcall(&down, &up);

		err = 0;
		switch (up.type) {
			/* all the easy operations */
			case GETMETA_OP:
			case SETMETA_OP:
			case LOOKUP_OP:
			case CREATE_OP:
			case REMOVE_OP:
			case RENAME_OP:
			case MKDIR_OP:
			case RMDIR_OP:
			case HINT_OP:
			case STATFS_OP:
				PDEBUG(D_UPCALL, "read upcall; type = %d, name = %s\n", up.type,
				up.v1.fhname);
				err = do_pvfs_op(&up, &down);
				if (err < 0) {
					PDEBUG(D_LIB, "do_pvfs_op failed for type %d\n", up.type);
				}
				break;
			/* the more interesting ones */
			case GETDENTS_OP:
				/* need to pass location and size of buffer to do_pvfs_op() */
				up.xfer.ptr = &dent;
				up.xfer.size = sizeof(dent);
				err = do_pvfs_op(&up, &down);
				if (err < 0) {
					PDEBUG(D_LIB, "do_pvfs_op failed for getdents\n");
				}
				break;
			case READ_OP:
				err = read_op(&up, &down);
				if (err < 0) {
					PDEBUG(D_LIB, "read_op failed\n");
				}
				break;
			case WRITE_OP:
				err = write_op(&up, &down);
				if (err < 0) {
					PDEBUG(D_LIB, "do_pvfs_op failed\n");
				}
				break;
			/* things that aren't done yet */
			default:
				err = -ENOSYS;
				break;
		}

		down.error = err;

		err = write_pvfsdev(dev_fd, &down, -1);
		if (err < 0) {
			pvfs_comm_shutdown();
			close_pvfsdev(dev_fd);
			exiterror("write failed");
		}

		/* If we used a big I/O buffer, free it after we have successfully
		 * returned the downcall.
		 */
		if (big_iobuf != NULL) {
			free(big_iobuf);
			big_iobuf = NULL;
		}

	}

	return -1;
} /* end of main() */

/* read_op()
 *
 * Returns 0 on success, -errno on failure.
 */
static int read_op(struct pvfs_upcall *up, struct pvfs_downcall *down)
{
	int err;

	if (up->u.rw.io.type != IO_CONTIG) return -EINVAL;

	if (up->u.rw.io.u.contig.size <= PVFS_OPT_IO_SIZE) {
		/* use our standard little buffer */
		up->xfer.ptr = iobuf;
		up->xfer.size = up->u.rw.io.u.contig.size;
	}
	else {
		/* need a big buffer; this is freed in main */
		if ((big_iobuf = (char *) malloc(up->u.rw.io.u.contig.size)) == NULL)
			return -errno;

		up->xfer.ptr = big_iobuf;
		up->xfer.size = up->u.rw.io.u.contig.size;
	}

	err = do_pvfs_op(up, down);
	if (err < 0) {
		PDEBUG(D_LIB, "do_pvfs_op failed\n");
	}
	
	return err;
}

/* write_op()
 *
 * Returns 0 on success, -errno on failure.
 */
static int write_op(struct pvfs_upcall *up, struct pvfs_downcall *down)
{

	int err;
	struct pvfs_downcall write_down;

	if (up->u.rw.io.type != IO_CONTIG) return -EINVAL;

	if (up->u.rw.io.u.contig.size <= PVFS_OPT_IO_SIZE) {
		/* use our standard little buffer */
		up->xfer.ptr = iobuf;
		up->xfer.size = up->u.rw.io.u.contig.size;
	}
	else {
		/* need a big buffer; this is freed in main */
		if ((big_iobuf = (char *) malloc(up->u.rw.io.u.contig.size)) == NULL)
			return -errno;

		up->xfer.ptr = big_iobuf;
		up->xfer.size = up->u.rw.io.u.contig.size;
	}

	/* NOTE:
	 * The write process requires an extra downcall, sent here, to tell
	 * the kernel where to put the data (in our user-space) so we can
	 * write it to the file system.  Here we setup the downcall to indicate
	 * the location of the pvfsd write buffer, then send it away.
	 */
	init_downcall(&write_down, up);
	write_down.xfer.ptr = up->xfer.ptr;
	write_down.xfer.size = up->xfer.size;
	
	err = write_pvfsdev(dev_fd, &write_down, -1);
	if (err < 0) {
		PDEBUG(D_DOWNCALL, "write_pvfsdev for write buffer failed\n");
		return err;
	}

	err = do_pvfs_op(up, down);
	if (err < 0) {
		PDEBUG(D_LIB, "do_pvfs_op failed\n");
	}

	return err;
}

/* startup()
 *
 * Handles mundane tasks of setting up logging, becoming a daemon, and
 * initializing signal handlers.
 */
static int startup(int argc, char **argv)
{
#ifdef MORE_FDS
	int filemax;
	struct rlimit lim;
#endif

	if (getuid() != 0 && geteuid() != 0)
		exiterror("must be run as root");

	if (is_daemon) {
		char logname[] = "/tmp/pvfsdlog.XXXXXX";
		int logfd;

		if ((logfd = mkstemp(logname)) == -1) {
			PDEBUG(D_SPECIAL, "couldn't create logfile...continuing...\n");
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

#ifdef MORE_FDS
	/* Try to increase number of open FDs.
	 *
	 * NOTE:
	 * The system maximum must be increased in order for us to be able to
	 * take advantage of an increase for this process.  This value is
	 * stored in /proc/sys/fs/file-max and is manipulated here with the
	 * get_filemax() and set_filemax() functions.
	 *
	 * NONE OF THIS CODE IS ANY GOOD UNTIL THE UNDERLYING TRANSPORT IS
	 * BETTER.  Specifically the sockset code needs to utilize larger
	 * numbers of FDs, as well as the code that associates sockets with
	 * files in the job code.  I'm going to leave this code here, but
	 * it's useless for the moment.
	 */
	if ((filemax = get_filemax()) < 0) {
		PERROR("warning: get_filemax failed\n");
	}
	/* let's make sure there are plenty of FDs to go around */
	else if (filemax < 2*PVFSD_NOFILE) {
		if ((filemax = set_filemax(2*PVFSD_NOFILE)) < 0) {
			PERROR("warning: set_filemax failed\n");
		}
	}
	/* now we take care of the per-process limits */
	if (getrlimit(RLIMIT_NOFILE, &lim) < 0) {
		PERROR("warning: getrlimit failed\n");
	}
	else {
		lim.rlim_cur=(lim.rlim_cur<PVFSD_NOFILE) ? PVFSD_NOFILE : lim.rlim_cur;
		lim.rlim_max=(lim.rlim_max<PVFSD_NOFILE) ? PVFSD_NOFILE : lim.rlim_max;
		if (setrlimit(RLIMIT_NOFILE, &lim) < 0) {
			PERROR("warning: setrlimit failed\n");
		}
	}
#endif

	/* change working dir to avoid unnecessary busy file systems */
	if (chdir("/") != 0)
		exiterror("could not change working directory to /\n");

	/* set up SIGTERM handler to shut things down */
	if (signal(SIGTERM, (void *)do_signal) == SIG_ERR)
		exiterror("could not set up SIGTERM handler");

	/* set up SIGHUP handler to restart the daemon */
	if (signal(SIGHUP, (void *)do_signal) == SIG_ERR)
		exiterror("could not set up SIGHUP handler");
	
	/* catch SIGPIPE and SIGSEGV signals and log them, on SEGV we die */
	if (signal(SIGPIPE, (void *)do_signal) == SIG_ERR)
		exiterror("could not set up SIGPIPE handler");
	if (signal(SIGSEGV, (void *)do_signal) == SIG_ERR)
		exiterror("could not set up SIGSEGV handler");

	PERROR(pvfsd_c_hdr); /* not really an error, but we want it printed */

	return 0;
} /* end of startup() */

static int do_signal(int signr)
{
	switch (signr) {
		case SIGTERM:
			cleanup();
		case SIGHUP:
			/* TODO: call restart code! */
			pvfs_comm_shutdown();
			close_pvfsdev(dev_fd);
			exiterror("caught SIGHUP, restart not yet implemented\n");
		case SIGPIPE:
			PERROR("caught SIGPIPE; continuing\n");
			if (signal(SIGPIPE, (void *)do_signal) == SIG_ERR)
				exiterror("could not set up SIGPIPE handler");
			break;
		case SIGSEGV:
			pvfs_comm_shutdown();
			close_pvfsdev(dev_fd);
			exiterror("caught SIGSEGV\n");
		default:
			pvfs_comm_shutdown();
			close_pvfsdev(dev_fd);
			exiterror("caught unexpected signal\n");
	}
	return 0;
}

static void exiterror(char *string)
{
	PERROR("pvfsd: %s\n", string);
	exit(-1);
}

static void cleanup(void)
{
	/* two calls to pvfs_comm_idle() will close everything */
	pvfs_comm_idle();
	pvfs_comm_idle();
	exit(0);
} /* end of cleanup() */


#ifdef MORE_FDS
/* SYSTEM-WIDE OPEN FILE MAXIMUM MANIPULATION CODE */

/* get_filemax(void)
 *
 * Returns the maximum number of files open for the system
 */
static int get_filemax(void)
{
	FILE *fp;
	int max;
	char lnbuf[1024];

	if ((fp = fopen("/proc/sys/fs/file-max", "r")) < 0)
		return -errno;

	fgets(lnbuf, 1023, fp);
	fclose(fp);
	max = atoi(lnbuf);
	return max;
}


/* set_filemax(int max)
 *
 * Returns new maximum on success.
 */
static int set_filemax(int max)
{
	FILE *fp;

	if ((fp = fopen("/proc/sys/fs/file-max", "w")) < 0)
		return -errno;

	if (max > 1048575) max = 1048575; /* keep things a little sane */
	fprintf(fp, "%d\n", max);
	fclose(fp);
	return get_filemax();
}
#endif



/* DEVICE MANIPULATION CODE */

/* open_pvfsdev() - opens the PVFS device file
 *
 * Returns FD on success, -1 on failure.
 */
static int open_pvfsdev(void)
{
	int fd;
	char fn[] = "/dev/pvfsd";

	if ((fd = open(fn, O_RDWR)) < 0) {
		if (errno == ENOENT) {
			PERROR("need to create /dev/pvfsd with \"mknod /dev/pvfsd c 60 0\"\n");
		}
		else if (errno == ENODEV) {
			PERROR("need to load pvfs module\n");
		}
		else if (errno == EBUSY) {
			PERROR("pvfsd already running?\n");
		}
		else {
			PDEBUG(D_PSDEV, "failed to open device file\n");
		}
		return -1;
	}

	return fd;
}

/* read_pvfsdev(fd, up, timeout) - reads a pvfs_upcall from the PVFS device
 *
 * timeout - number of seconds to wait for ready to read.  If timeout is
 *   less than zero, then the call will block until data is available.
 *
 * Returns sizeof(*up) on success, 0 on timeout, -1 on failure.
 *
 * NOTES:
 * This implementatio uses a select() first to block until the device is
 * ready for reading.  Currently the read() function implementation for
 * the device is nonblocking, so we must do this.
 */
static int read_pvfsdev(int fd, struct pvfs_upcall *up, int timeout)
{
	fd_set readfds;
	int ret;
	struct timeval tv;

read_pvfsdev_select_restart:
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	if (timeout >= 0) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		ret = select(fd+1, &readfds, NULL, NULL, &tv);
	}
	else ret = select(fd+1, &readfds, NULL, NULL, NULL);

	if (ret < 0) {
		if (errno == EINTR) goto read_pvfsdev_select_restart;

		PERROR("fatal error occurred selecting on device\n");
		return -1;
	}
	
	if (ret == 0) return 0; /* timed out */

	PDEBUG(D_UPCALL, "reading from device\n");
	
read_pvfsdev_read_restart:
	if ((ret = read(fd, up, sizeof(*up))) < 0) {
		if (errno == EINTR) goto read_pvfsdev_read_restart;

		PERROR("fatal error occurred reading from device\n");
		return -1;
	}

	if (up->magic != PVFS_UPCALL_MAGIC) {
		PDEBUG(D_UPCALL, "magic number not valid in upcall\n");
		return -1;
	}

	return sizeof(*up);
}

/* write_pvfsdev() - writes a pvfs_downcall to the PVFS device
 *
 * Returns sizeof(*down) on success, 0 on timeout, -1 on failure.
 *
 * TODO: IMPLEMENT TIMEOUT FEATURE IF IT IS EVER NEEDED.
 * 
 * NOTES:
 * Timeout isn't implemented at this time.
 */
static int write_pvfsdev(int fd, struct pvfs_downcall *down, int timeout)
{
	int ret;


	if (down->magic != PVFS_DOWNCALL_MAGIC) {
		PDEBUG(D_DOWNCALL, "magic number not valid in downcall\n");
		return -1;
	}

	PDEBUG(D_PSDEV, "writing to device\n");

write_pvfsdev_restart:
	if ((ret = write(fd, down, sizeof(*down))) < 0) {
		if (errno == EINTR) goto write_pvfsdev_restart;

		PERROR("fatal error occurred writing to device\n");
		return -1;
	}

	return sizeof(*down);
}

/* close_pvfsdev() - closes the PVFS device file
 *
 */
static void close_pvfsdev(int fd)
{
	close(fd);

	return;
}


/* init_downcall() - initialize a downcall structure
 */
static void init_downcall(struct pvfs_downcall *down, struct pvfs_upcall *up)
{
	memset(down, 0, sizeof(*down));
	down->magic = PVFS_DOWNCALL_MAGIC;
	down->seq = up->seq;
	down->type = up->type;
}

static void usage(void){
	printf("\n");
	printf("Usage: pvfsd [OPTIONS]\n");
	printf("Starts PVFS client side daemon.\n");
	printf("\n");
	printf("\t-d <debugging level in hex>   (increases amount of pvfsd logging)\n");
	printf("\t-h                            (show this help screen)\n");
	printf("\n");
	return;
}

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */
