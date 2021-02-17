
/*
  close_blk.c: closes files used with buffered multidimensional block interface
  author: W. B. Ligon III
  date: 5/27/97
  org: Clemson University
  copyright (c) 1997 Clemson University, all rights reserved.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "blocks.h"

close_blk(int fd)
{
	fdim_p fp;
	if (!VALID(fd))
		return(-1);
	if (!(fp = OPEN_FILES(fd)))
		return CLOSE(fd);
	if (fp->buffer && fp->dirty)
	{
		int d;
		union fpu source;
		int *bsize, *bcount;
		bsize = (int *)malloc(fp->dims * sizeof(int));
		bcount = (int *)malloc(fp->dims * sizeof(int));
		for (d = 0; d < fp->dims; d++)
		{
			bsize[d] = fp->bsize[d] * fp->bbuff[d];
			bcount[d] = fp->bcount[d] / fp->bbuff[d];
			}
		source.i = fd;
		do_write_blk(source, fp->buffer, fp->dims, fp->esize,
			fp->buftag, bsize, bcount, PDIM, MFILE);
		free(bsize);
		free(bcount);
		}
	return CLOSE(fd);
	}
