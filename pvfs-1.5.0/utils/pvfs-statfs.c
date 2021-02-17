/*
 * pvfs-statfs.c copyright (c) 2000 Clemson University, all rights reserved.
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

/* NOTES:
 *
 * This is a quick and silly little tool to allow for statfs of PVFS
 * without the preloaded library or kernel module or whatever.  It
 * is here to make testing easier.  Enjoy.
 */

#include <stdio.h>
#include <sys/vfs.h>
#include <pvfs.h>
#include <pvfs_proto.h>

int pvfs_statfs(char *path, struct statfs *buf);

int main(int argc, char **argv)
{
	struct statfs sfs;
	int64_t bsize;

	if (argc <= 1 || pvfs_statfs(argv[1], &sfs) < 0) exit(1);

	bsize = sfs.f_bsize;

	printf("blksz = %Ld, total (bytes) = %Ld, free (bytes) = %Ld\n",
	bsize, (int64_t) sfs.f_blocks * bsize, (int64_t) sfs.f_bfree * bsize);
	exit(0);
}
