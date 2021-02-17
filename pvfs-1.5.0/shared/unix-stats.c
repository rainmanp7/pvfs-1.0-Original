/*
 * unix-stats.c copyright (c) 1997 Clemson University, all rights reserved.
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

/*
 * 10/27/2000 - THIS IS ALL MOOT NOW; JUST CALLING STAT CALLS
 *
 */

#include <sys/stat.h>


int unix_lstat(const char *fn, struct stat *s_p)
{
	return lstat(fn, s_p);
}

int unix_stat(const char *fn, struct stat *s_p)
{
	return stat(fn, s_p);
}

int unix_fstat(int fd, struct stat *s_p)
{
	return fstat(fd, s_p);
}
