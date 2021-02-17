/*
 * pvstat.c copyright (c) 1997-2000 Clemson University, all rights reserved.
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

/*
 * PVSTAT.C -	prints out information from metadata file
 *					Usage:  pvstat <filename>
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <pvfs.h>

int main (int argc, char *argv[])
{
	int i, fd, size;
	pvfs_filestat fstat;
	
	if (argc < 2) {
		fprintf(stderr, "usage: %s <filename>\n", argv[0]);
		return 1;
	}

	if ((fd = pvfs_open(argv[1], O_RDONLY)) < 0) {
		perror("pvfs_open");
		return 1;
	}

	if (pvfs_ioctl(fd, GETMETA, &fstat) < 0) {
		perror("pvfs_ioctl");
		return 1;
	}
	printf("%s: base = %d, pcount = %d, ssize = %d\n", argv[1], fstat.base,
	fstat.pcount, fstat.ssize);
	

	return 0;
}
