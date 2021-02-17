/*
 * fslist.h copyright (c) 1997 Clemson University, all rights reserved.
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

#ifndef FSLIST_H
#define FSLIST_H

#include <llist.h>
#include <desc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <flist.h>

typedef llist_p fslist_p;

typedef struct fsinfo fsinfo, *fsinfo_p;

struct fsinfo {
	ino_t fs_ino;    /* inode # of root directory for this filesystem */
	int nr_iods;     /* # of iods for this filesystem */
	flist_p fl_p;    /* list of open files for this filesystem */
	iod_info iod[1]; /* list of iod addresses */
};

fslist_p fslist_new(void);
int fs_add(fslist_p, fsinfo_p);
fsinfo_p fs_search(fslist_p, ino_t);
int fs_rem(fslist_p, ino_t);
void fslist_cleanup(fslist_p);
int forall_fs(fslist_p, int (*fn)(void *));
int fs_dump(void *);
int fslist_dump(fslist_p fsl_p);

#endif
