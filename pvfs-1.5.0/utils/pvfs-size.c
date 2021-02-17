/*
 * pvfs-size.c copyright (c) 1999 Clemson University, all rights reserved.
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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


#include <pvfs.h>
#include <pvfs_proto.h>

/* NOTES:
 *
 * This is a quick and silly little tool to show the size of PVFS
 * files without the preloaded library or kernel module or whatever.  It
 * is here to make testing easier.  Enjoy.
 */
int main(int argc, char **argv)
{
	int fd;
	int64_t size;

	/* wow, I actually check to see that there is a parameter! */
	if (argc != 2) {
		fprintf(stderr, "%s: <filename>\n", argv[0]);
		return 0;
	}

	fd = pvfs_open(argv[1], O_RDONLY, 0,0,0);
	if (fd < 0) {
		perror("pvfs_open");
		return 0;
	}
	size = pvfs_lseek64(fd, 0L, SEEK_END);
	if (size < 0) {
		perror("pvfs_lseek64");
		return 0;
	}
	printf("%s: %Ld bytes\n", argv[1], size);
	return 0;
}
