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

/* NOTES:
 *
 * This is a quick and silly little tool to allow for removal of PVFS
 * files without the preloaded library or kernel module or whatever.  It
 * is here to make testing easier.  Enjoy.
 */

#include <pvfs.h>
#include <pvfs_proto.h>

int main(int argc, char **argv)
{
	if (argc > 1) {
		pvfs_unlink(argv[1]);
	}
	return 0;
}
