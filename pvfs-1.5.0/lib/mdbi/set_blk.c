
/*
  set_blk.h: set block parameters for buffered multi-dimensional blocks interface
  author: W. B. Ligon III
  date: 5/27/97
  org: Clemson University
  copyright (c) 1997 Clemson University, all rights reserved.
*/

#include <unistd.h>
#include <stdio.h>
#include <varargs.h>
#include "blocks.h"

#ifdef NOPVFS
fdim_p open_files[4096];
#endif

/* set_blk - sets up a pvfs file to use with multi-dimentional block interface
/* 	parameters:
/*		fd - file descriptor
/*		dims - number of dimensions
/*		esize - size of an element (dimension zero)
/*		v_dsize - variable list of ints - each dimension has
/*			two values:
/*				group - number of elements in a block
/*				count - number of blocks in the dimension
/*/

/* set_blk(int fd, int dims, int esize, int v_dsize, ... ) */

set_blk(va_alist)
va_dcl
{
	va_list ap;
	int fd;
	int dims;
	int esize;
	char *malloc();
	int d;
	fdim_p fp;

	va_start(ap);
	fd = va_arg(ap,int);
	dims = va_arg(ap,int);
	esize = va_arg(ap,int);
	if (!VALID(fd))
		return(-1);
	if (!(fp = OPEN_FILES(fd)))
	{
		fp = OPEN_FILES(fd) = (fdim_p)malloc(sizeof(fdim));
		fp->bsize = NULL;
		}
	else if (fp->dims != dims)
	{
		free(fp->bsize);
		fp->bsize = NULL;
		}
	if (!fp->bsize)
	{
		fp->bsize = (int *)malloc(DIM_SIZE(dims));
		fp->bcount = &(fp->bsize[dims]);
		fp->bbuff = &(fp->bcount[dims]);
		fp->buftag = &(fp->bbuff[dims]);
		}
	fp->dims = dims;
	fp->esize = esize;
	fp->dirty = CLEAN;
	fp->buffer = NULL;
	for (d = 0; d < dims; d++)
	{
		fp->bsize[d] = va_arg(ap,int);
		fp->bcount[d] = va_arg(ap,int);
		fp->bbuff[d] = 1;
		fp->buftag[d] = -1;
		}
	va_end(ap);
	return(0);
	}
