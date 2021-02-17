/*
 * flist.h copyright (c) 1996,1997 Clemson University, all rights reserved.
 * 
 * Written by Rob Ross, Matt Cettei, and Walt Ligon.
 * 
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
 * Contact:  Rob Ross  	rbross@parl.eng.clemson.edu
 *           Matt Cettei mcettei@parl.eng.clemson.edu
 * 
 */

/*
 * FLIST.H - header for flist.c, the manager file list manipulation
 * routines
 */

#ifndef FLIST_H
#define FLIST_H

#include <llist.h>
#include <meta.h>
#include <dfd_set.h>

typedef llist_p flist_p;

typedef struct finfo finfo, *finfo_p;

struct finfo {
	int unlinked;     /* fd of metadata file or -1 */
	ino_t f_ino;      /* inode # of metadata file */
	pvfs_filestat p_stat; /* PVFS metadata for file */
	int cap;          /* max. capability assigned thusfar? */
	int cnt;          /* count of # of times file is open */
	int grp_cap;      /* capability # for group; this should be expanded
	                   * sooner or later so that multiple groups can
	                   * exist for a single file */
	char *f_name;     /* file name - for performing operations on metadata */
	dyn_fdset socks;  /* used to keep track of what FDs have opened the file */
};

flist_p flist_new(void);
int flist_empty(flist_p);
int f_add(flist_p, finfo_p);
finfo_p f_search(flist_p, ino_t);
finfo_p f_new(void);
int f_rem(flist_p, ino_t);
void flist_cleanup(flist_p);
int f_dump(void *);
int forall_finfo(flist_p, int (*fn)(void *));
void f_free(void *f_p);


#endif
