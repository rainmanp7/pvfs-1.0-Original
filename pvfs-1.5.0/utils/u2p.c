/*
 * u2p.c copyright (c) 1997 Clemson University, all rights reserved.
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

/* u2p - copies a file on a unix filesystem to a parallel filesystem
 */

/*
 * $Log: u2p.c,v $
 * Revision 1.4  2000/10/30 14:51:59  rbross
 * Took out all the glibc files, since we're not using them any more.
 * Removed instances of syscall() all over the place.
 *
 * Revision 1.3  2000/10/03 21:33:13  rbross
 * These files implement 64-bit access from the client using the 64-bit
 * calls pvfs_lseek64, pvfs_read, and pvfs_open, pvfs_write, and
 * pvfs_close.  I don't know if anything else works.
 *
 * Revision 1.2  2000/09/15 15:37:32  rbross
 * Cleaned up the utilities!  Wow!  Turned on warnings and strict
 * prototypes for a bit, cleaned up most everything.
 *
 * Revision 1.1.1.1  1999/08/10 17:11:31  rbross
 * Original 1.3.3 sources
 */

/* UNIX INCLUDES */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>

/* PVFS INCLUDES */
#include <pvfs.h>
#include <pvfs_proto.h>
#include <debug.h>

/* DEFINES */
#define MAX_BUF_SIZE 1024*1024*16
#define GET_TIMING

/* PROTOTYPES */
void usage();

/* GLOBALS */
extern int optind;
extern char *optarg;
extern int debug_on;

/* FUNCTIONS */
int main(int argc, char **argv)
{
	int c, ssize=-1, base=-1, pcount=-1;
	pvfs_filestat metadata={0,0,0,0,0};
	int src, dest;
	char *buf;
	int bufsize, readsize;
	struct stat filestat;

#ifdef GET_TIMING
	int total_size = 0;
	struct timeval start, end;
	struct timezone zone;
	long runtime = 0;
#endif

	debug_on = 0;

	while ((c = getopt(argc, argv, "s:n:b:")) != EOF) {
		switch(c) {
		case 's':
			ssize = atoi(optarg);
			break;
		case 'b':
			base = atoi(optarg);
			break;
		case 'n':
			pcount = atoi(optarg);
			break;
		case '?':
			usage(argc, argv);
			exit(0);
		}
	} /* end of options */
	if (optind >= (argc - 1)) {
		usage(argc, argv);
		exit(0);
	}

	/* open files */
	if ((src = pvfs_open(argv[argc-2], O_RDONLY, 0, 0, 0)) == -1) {
		PERROR("pvfs_open");
		exit(-3);
	}

	metadata.base = base;
	metadata.pcount = pcount;
	metadata.ssize = ssize;
	metadata.bsize = 0;
	if ((dest = pvfs_open(argv[argc-1], O_TRUNC|O_WRONLY|O_CREAT|O_META,
		0777, &metadata, 0)) == -1)
	{
		fprintf(stderr, "pvfs_open: unable to open destfile %s\n",
			argv[argc-1]);
		exit(-2);
	}
	pvfs_ioctl(dest, GETMETA, &metadata);

	/* get a buffer of some (reasonably) intelligent size */
	pvfs_fstat(src, &filestat);
	bufsize = ((MAX_BUF_SIZE) / (metadata.ssize * metadata.pcount)) *
		(metadata.ssize * metadata.pcount);

	if (filestat.st_size < bufsize) {
		bufsize = filestat.st_size;
	}
	buf = (char *)malloc(bufsize);

	/* read & write until done */
	while ((readsize = pvfs_read(src, buf, bufsize)) > 0) {
#ifdef GET_TIMING
		/* start timing */
		gettimeofday(&start, &zone);
#endif
		if (pvfs_write(dest, buf, readsize) < readsize) {
			fprintf(stderr, "pvfs_write: short write\n");
		}
#ifdef GET_TIMING
		/* stop timing */
		gettimeofday(&end, &zone);
		runtime += (end.tv_usec-start.tv_usec) + 
			1000000*(end.tv_sec-start.tv_sec);
		total_size += readsize;
#endif
	} /* end of while loop */
#ifdef GET_TIMING
	printf("%d node(s); ssize = %d; buffer = %d; %3.3fMBps (%d bytes total)\n",
		metadata.pcount, metadata.ssize, bufsize, ((double)total_size / 1048576.0)
		/ runtime * 1000000.0, total_size);
#endif
	/* close files and free memory */
	pvfs_close(src);
	pvfs_close(dest);
	free(buf);
	exit(0);
} /* end of main() */

void usage(int argc, char **argv)
{
	fprintf(stderr, "usage: %s [-s stripe] [-b base] [-n (# of nodes)]" 
		" srcfile destfile\n", argv[0]);
} /* end of usage() */
