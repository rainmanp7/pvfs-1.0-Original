/*
 * jlist.h copyright (c) 1997 Clemson University, all rights reserved.
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

#ifndef JLIST_H
#define JLIST_H

#include <llist.h>
#include <alist.h>

/* includes needed for using fd_sets */
#include <dfd_set.h>

typedef llist_p jlist_p;

typedef struct jinfo jinfo, *jinfo_p;

struct jinfo {
	int jobnum;			/* unique identifier for jobs			*/
	int8_t type;
	int8_t nospc;		/* no space left in file -- throw away any write data */
	dyn_fdset socks;		/* list of sockets that are (or have been) in job */
	int max_nr;			/* max socket # + 1 (for use w/socks) */
	int64_t size;			/* amount of data to be moved by this job */
	finfo_p file_p;	/* pointer to open file info */
	int64_t lastb;			/* offset of last byte in file to be accessed by job */
	alist_p al_p;		/* list of accesses */
};

jlist_p jlist_new(void);
int j_add(jlist_p, jinfo_p);
jinfo_p j_search(jlist_p, int sock);
int j_sock_rem(jlist_p, int sock);
int j_ptr_rem(jlist_p, void *j_p);
void jlist_cleanup(jlist_p);
int jlist_dump(jlist_p);
int j_dump(void *);
void j_free(void *j_p);
jinfo_p j_new(void);
int jlist_purge_sock(jlist_p jl_p, int s);
int jlist_count(void *);


#define j_rem(__l_p, __s) j_sock_rem((__l_p), (__s))

#endif
