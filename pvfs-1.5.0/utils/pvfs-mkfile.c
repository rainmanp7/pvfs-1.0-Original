/*
 * pvfs-unlink.c copyright (c) 1999 Clemson University, all rights reserved.
 *
 * Written by Rob Ross.
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
 * Contact:  Rob Ross    rbross@parl.clemson.edu
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <pvfs.h>
#include <pvfs_proto.h>

#define BUFSZ (128 * 1024)

int usage(int argc, char **argv);

extern char *optarg;
extern int optind, opterr;

/* NOTES:
 *
 * This is a quick and silly little tool to allow for creation of PVFS
 * files without the preloaded library or kernel module or whatever.  It
 * is here to make testing easier.  Enjoy.
 */
int main(int argc, char **argv)
{
	int fd, err, i, opt;
	int opt_mbytes = 0, opt_kbytes = 0, opt_gbytes = 0;
	int64_t size = -1, remain;
	char buf[BUFSZ];
	char filename[256];

	while ((opt = getopt(argc, argv,"MGKs:f:")) != EOF) {
		switch (opt) {
			case 'M' : opt_mbytes = 1; break;
			case 'G' : opt_gbytes = 1; break;
			case 'K' : opt_kbytes = 1; break;
			case 's' : size = atoi(optarg); break;
			case 'f' : strncpy(filename, optarg, 255); break;
			default : usage(argc, argv);
		}
	}


	if (size <= 0) {
		usage(argc, argv);
		return -1;
	}

	if (opt_mbytes) size *= 1048576LL;
	else if (opt_gbytes) size *= 1073741824LL;
	else if (opt_kbytes) size *= 1024LL;

	printf("# %s: attempting to create %s with %Ld bytes.\n", argv[0],
	filename, size);

	/* put some data into the buffer */
	for (i=0; i < BUFSZ; i++) buf[i] = (char) i;

	fd = pvfs_open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777, 0, 0);
	if (fd < 0) {
		perror("pvfs_open");
		usage(argc, argv);
		return -1;
	}

	/* write out whole buffers until we're almost done */
	remain = size;
	while (remain - BUFSZ > 0) {
		err = pvfs_write(fd, buf, BUFSZ);
		if (err < 0) {
			perror("pvfs_write (1)");
			fprintf(stderr,
			"error occurred writing extent %Ld - %Ld\n",
			size - remain, size - remain + BUFSZ - 1);
			return -1;
		}
		remain -= BUFSZ;
	}

	/* write out any remaining data */
	if (remain > 0) {
		err = pvfs_write(fd, buf, remain);
		if (err < 0) {
			perror("pvfs_write (2)");
			return -1;
		}
		remain -= err;
	}
	size -= remain;

	printf("# %s: created %s with %Ld bytes.\n", argv[0], filename, size);

	pvfs_close(fd);
	return 0;
}

int usage(int argc, char **argv)
{
	fprintf(stderr, "usage: %s [-MGK] -f <filename> -s <size>\n\t-M"
	"\tsize in Mbytes\n\t-G\tsize in Gbytes\n\t-K\tsize in Kbytes\n", argv[0]);
	return 0;
}
