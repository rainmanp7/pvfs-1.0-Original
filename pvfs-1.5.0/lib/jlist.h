/*
 * jlist.c copyright (c) 1996 Rob Ross, all rights reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * any later version.
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
 * Contact:  Rob Ross  rbross@parl.eng.clemson.edu  or
 */

/*
 * JLIST.C - LIBRARY VERSION - header file for jlist.c
 *
 */

#ifndef JLIST_H
#define JLIST_H

#include <llist.h>
#include <alist.h>

#include <dfd_set.h>

typedef llist_p jlist_p;

typedef struct jinfo jinfo, *jinfo_p;

#define J_READ  0
#define J_WRITE 1
#define J_OTHER 2

struct jinfo {
	int8_t type;			/* IOD_RW_READ, IOD_RW_WRITE, etc. */
	dyn_fdset socks;     /* list of sockets that are (or have been) in job */
	int max_nr;       /* max socket # + 1 (for use w/socks) */
	int64_t size;	/* amount of data to be moved by this job */
	alist_p al_p;		/* list of accesses */
	int64_t psize[1];	/* array of partial sizes from iods - variable size! */
};

jlist_p jlist_new(void);
int j_add(jlist_p, jinfo_p);
jinfo_p j_search(jlist_p, int sock);
int j_rem(jlist_p, int sock);
void j_cleanup(jlist_p);
int jlist_dump(jlist_p);
int j_dump(void *);
jinfo_p j_new(int pcount);

#endif
