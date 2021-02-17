/*
 * flist.h copyright (c) 1996 Clemson University, all rights reserved.
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
 * FLIST.H - header file for iod file management functions
 *
 */

#ifndef FLIST_H
#define FLIST_H

#include <llist.h>
#include <meta.h>
#include <sys/stat.h>
#include <unistd.h>

typedef llist_p flist_p;

typedef struct finfo finfo, *finfo_p;
typedef struct ginfo ginfo, *ginfo_p;
typedef struct minfo minfo, *minfo_p;

/* mmapped file info */
struct minfo {
	void *mloc;			/* address of mmapped region */
	int64_t msize;			/* size of region */
	int64_t moff;			/* offset into file */
	int mprot;			/* protection of mmapped region */
};

/* group connection info */
struct ginfo {
	int sched;			/* socket # of scheduler connection */
	int nr_conn;		/* # of data connections (0 for normal access) */
	int *conn;			/* array of sockets corresponding to data conn's */
};

struct finfo {
	int64_t f_ino;      /* inode # of metadata file */
	int32_t cap;          /* capability */
	int fd;				/* file desc. # */
	int sock;			/* socket # for non-group opens, -1 for group opens */
	pvfs_filestat p_stat; /* PVFS metadata for file */
	int64_t fsize;			/* file size */
	int pnum;			/* partition # */
	minfo mm;			/* mmap specs */
	ginfo grp;			/* group specs */
};

flist_p flist_new(void);
int f_add(flist_p, finfo_p);
int f_sock_rem(flist_p fl_p, int s);
finfo_p f_ino_cap_search(flist_p, int64_t f_ino, int32_t cap);
int f_ino_cap_rem(flist_p, int64_t f_ino, int32_t cap);
void flist_cleanup(flist_p);
int f_dump(void *);
finfo_p f_new(void);
int flist_purge_sock(flist_p fl_p, int s);
int flist_dump(flist_p fl_p);

#define f_search(_x,_y,_z) f_ino_cap_search((_x), (_y), (_z))
#define f_rem(_x,_y,_z) f_ino_cap_rem((_x), (_y), (_z))

#endif
