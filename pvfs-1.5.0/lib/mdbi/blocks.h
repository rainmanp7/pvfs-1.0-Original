
#ifndef BLOCKS_H
#define BLOCKS_H

/*
  blocks.h: header file for buffered multi-dimensional blocks interface
  author: W. B. Ligon III
  date: 5/27/97
  org: Clemson University
  copyright (c) 1997 Clemson University, all rights reserved.
*/

/* this stuff allows blocks code to compile for unix fs or pvfs */

#ifdef NOPVFS

/* PVFS IS NOT INCLUDED */

typedef struct fdim fdim, *fdim_p;

struct fdim {
	int dims;	/* # of dimensions */
	int esize;	/* size of element */
	int dirty;	/* flags if buffer is dirty */
	char *buffer;	/* points to block buffer */
	int *buftag;	/* points to array of ints with buffer tag */
	int *bsize;	/* size of a block */
	int *bcount;	/* number of blocks */
	int *bbuff;	/* buffering factor */
};

extern fdim_p open_files[];
#define OPEN_FILES(fd) open_files[fd]
#define VALID(fd) (lseek(fd,0,SEEK_SET)>=0)

#define READ read
#define WRITE write
#define OPEN open
#define CLOSE close
#define FSYNC fsync
#define LSEEK lseek

#define PDIM 1
#define PART(f,o,g,s) do { } while (0)

#else

/* PVFS IS INCLUDED */

#include <pvfs.h>
extern fdesc_p pfds[];
#define OPEN_FILES(fd) pfds[fd]->dim_p
#define VALID(fd) pfds[fd]

#define READ pvfs_read
#define WRITE pvfs_write
#define OPEN pvfs_open
#define CLOSE pvfs_close
#define FSYNC(fd) 0
#define LSEEK pvfs_lseek

#define PDIM 2
#define PART(f,o,g,s)				\
	do {					\
		fpart p;			\
		p.offset = o;			\
		p.gsize = g;			\
		p.stride = s;			\
		p.gstride = 0;			\
		p.ngroups = 0;			\
		pvfs_ioctl(f, SETPART, &p);	\
		} while (0)

#endif

/* this stuff is needed for all versions of the blocks code */

#define DIM_SIZE(dim) ((dim)*4*sizeof(int))

/* values for the dirty flag */
#define DIRTY 1
#define CLEAN 0


/* mode definitions */
#define MFILE 0
#define MBUFF 1

union fpu {
        int i;
        char *p;
};

#endif
