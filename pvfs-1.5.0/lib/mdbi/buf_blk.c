
/*
  buf_blk.h: set buffering paramters for buffered multi-dimensional blocks interface
  author: W. B. Ligon III
  date: 5/27/97
  org: Clemson University
  copyright (c) 1997 Clemson University, all rights reserved.
*/

#include <unistd.h>
#include <stdio.h>
#include <varargs.h>
#include "blocks.h"

/* buf_blk(int fd, int v_bbuff, ... ) */

buf_blk(va_alist)
va_dcl
{
	va_list ap;
	int fd;
	int buf_size;
	char *malloc();
	int d;
	fdim_p fp;
	int valid = 1;
	int dobuf = 0;

	va_start(ap);
	fd = va_arg(ap,int);
	if (!VALID(fd))
		return(-1);
	if (!(fp = OPEN_FILES(fd)))
		return(-1);
	if (fp->buffer)
	{
		if (fp->dirty)
		{
			/* need to write buffer */
			int *bsize, *bcount;
			union fpu source;
			bsize = (int *)malloc(fp->dims * sizeof(int));
			bcount = (int *)malloc(fp->dims * sizeof(int));
			for (d = 0; d < fp->dims; d++)
			{
				if (fp->bbuff[d] < 1 || fp->buftag[d] < 0)
					valid = 0;
				bsize[d] = fp->bsize[d] * fp->bbuff[d];
				bcount[d] = fp->bcount[d] / fp->bbuff[d];
				}
			source.i = fd;
			if (valid)
				do_write_blk(source, fp->buffer, fp->dims,
					fp->esize, fp->buftag, bsize, bcount,
					PDIM, MFILE);
			free(bsize);
			free(bcount);
			}
		free(fp->buffer);
		fp->buffer = NULL;
		}
	buf_size = fp->esize;
	for (d = 0; d < fp->dims; d++)
	{
		fp->bbuff[d] = va_arg(ap,int);
		/* this is minimal validation */
		if (fp->bbuff[d] < 1)
			fp->bbuff[d] = 1;
		if (fp->bbuff[d] > fp->bcount[d])
			fp->bbuff[d] = fp->bcount[d];
		if (fp->bbuff[d] > 1)
			dobuf = 1;
		buf_size *= fp->bbuff[d] * fp->bsize[d];
		fp->buftag[d] = -1;
		}
	fp->dirty = CLEAN;
	va_end(ap);
	if (dobuf)
		if ((fp->buffer = malloc(buf_size)) == NULL)
		{
			for (d = 0; d < fp->dims; d++)
				fp->bbuff[d] = 1;
			return(-1);
			}
	return(0);
	}
