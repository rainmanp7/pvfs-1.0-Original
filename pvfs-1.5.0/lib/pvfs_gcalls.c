/*
 * pvfs_gcalls.c copyright (c) 1996 R. Ross and M. Cettei, all rights reserved.
	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as published
	by the Free Software Foundation; either version 2 of the License, or
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of 
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
	Contact:  Rob Ross  	rbross@parl.eng.clemson.edu
				 Matt Cettei mcettei@parl.eng.clemson.edu

 */

/* THIS IS ALL USELESS AND IS GETTING CUT OUT */

static char header[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_gcalls.c,v 1.2 1999/08/13 23:26:21 rbross Exp $";

#include <lib.h>

/* EXTERNS */
extern jlist_p active_p;
extern fdesc_p pfds[];
extern sockset socks;

/* FUNCTIONS */
#define PCNT pfds[fd]->fd.meta.p_stat.pcount
#define FINO pfds[fd]->fd.meta.u_stat.st_ino

int pvfs_gstart(int fd, int grp_sz)
{
	errno = ENOSYS;
	return(-1);
}

int pvfs_gadd(int fd, int grp_nr)
{
	errno = ENOSYS;
	return(-1);
}

int sched_read(int fd, size_t count)
{
	errno = ENOSYS;
	return(-1);
}

int sched_write(int fd, size_t count)
{
	errno = ENOSYS;
	return(-1);
}

int sched_rw(int fd, size_t count, int jtype)
{
	errno = ENOSYS;
	return(-1);
}

