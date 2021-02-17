/*
 * check_pvfs.c copyright (c) 1997 Clemson University, all rights reserved.
 *
 * Written by Matt Cettei and Walt Ligon.
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
 * Contact:  Rob Ross	 rbross@parl.eng.clemson.edu
 *			  Matt Cettei mcettei@parl.eng.clemson.edu
 */

/*
 * CHECK_PVFS.C - determine if a file or directory is a PVFS file/dir.
 *
 */

static char check_pvfs_c[] = "$Header: /projects/cvsroot/pvfs/shared/check_pvfs.c,v 1.6 2000/11/22 16:06:06 rbross Exp $";

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/timeb.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <meta.h>
#include <req.h>

extern int errno;

/* check_pvfs() - checks to see if the filename pointed to by "pathname"
 * is or would be a PVFS file.  This is determined as follows.  If the
 * name refers to a file that exists or a name that does not exist, the
 * directory is checked for the existance of a ".pvfsdir" file.  If this
 * file is there, the file is (or would be) a PVFS file and 1 is
 * returned.
 *
 * If there is no ".pvfsdir" file, 0 is returned.
 * If there is a ".pvfsdir" file and the pathname refers to a directory,
 * 2 is returned.
 */

#define MAXTMP 512
int check_pvfs(const char* pathname)
{
	int length, isdir=0;
	char tmp[MAXTMP];
	static char pvfsdir[]=".pvfsdir", iodtab[]=".iodtab";
	struct stat foo;

	if (!pathname) {
		errno = EFAULT;
		return(-1);
	}

	strncpy(tmp, pathname, MAXTMP);
	length = strlen(tmp);
	if (length == 0) {
		errno = ENOENT;
		return(-1);
	}
	if (unix_stat(tmp, &foo) < 0) {
		/* file doesn't exist.  We need to fool the next check so that we
		 * can see if this would be a PVFS file were it to be created...
		 */
		foo.st_mode = 0;
	}
	if (!S_ISDIR(foo.st_mode)) {
		/* find division between file name and directory */
		for (; length >= 0 && tmp[length] != '/'; length--);

		/* NEW CODE AND BEHAVIOR: RETURN -1 FOR RESERVED NAMES, USE NEW FN */
		if (resv_name(pathname)) {
			errno = ENOENT;
			return -1;
		}
		/* END OF NEW CODE */

		if (length < 0) /* no directory here! */ {
			tmp[0] = '.';
			tmp[1] = 0;
		}
		else  /* cut off the file name */ {
			tmp[length] = 0;
		}
	}
	else {
		isdir=1;
	}
	strcat(tmp, "/.pvfsdir");
	if (unix_stat(tmp, &foo) < 0) {
		return(0);
	}
	else {
		/* Otherwise must be pvfs file */
		return(1+isdir);
	}
}
