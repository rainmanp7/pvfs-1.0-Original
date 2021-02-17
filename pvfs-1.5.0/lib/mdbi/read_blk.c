
/*
  read_blk.h: read file for buffered multi-dimensional blocks interface
  author: W. B. Ligon III
  date: 5/27/97
  org: Clemson University
  copyright (c) 1997 Clemson University, all rights reserved.
*/

#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <varargs.h>
#include "blocks.h"

/* these defines are needed so this source can be used to define write_blk */

#ifndef BCOPY
#define BCOPY bcopy
#endif

#ifndef READ_BLK
#define READ_BLK read_blk
#endif

#ifndef DO_READ_BLK
#define DO_READ_BLK do_read_blk
#endif

READ_BLK(va_alist)
va_dcl
{
	va_list ap;
	int fd;
	fdim_p fp;
	char *ptr;
	int dims;
	int esize;
	char *malloc();
	int d;
	static int bsize_store[10], bcount_store[10];
	static int  block_store[10], mb_store[10];
	int *bsize = bsize_store;
	int *bcount = bcount_store;
	int *block = block_store;
	int *mb = mb_store;
	union fpu source;
	int match = 1;
	int nobuff = 1;

	va_start(ap);
	fd = va_arg(ap, int);
	ptr = va_arg(ap, caddr_t);
	if (!VALID(fd))
		return(-1);
	if (!(fp = OPEN_FILES(fd)))
		return(-1);
	dims = fp->dims;
	esize = fp->esize;
	if (dims > 10)
	{
		block = (int *)malloc(sizeof(int)*dims);
		bsize = (int *)malloc(sizeof(int)*dims);
		bcount = (int *)malloc(sizeof(int)*dims);
		mb = (int *)malloc(sizeof(int)*dims);
		}
	for (d = 0; d < dims; d++)
	{
		/* get requested block indexes */
		block[d] = va_arg(ap, int);
		/* determine macro-block requested */
		mb[d] = block[d] / fp->bbuff[d];
		if (fp->bbuff[d] != 1)
			nobuff = 0;
		/* compute macro-block bsize and bcount */
		bsize[d] = fp->bsize[d] * fp->bbuff[d];
		bcount[d] = fp->bcount[d] / fp->bbuff[d];
		}
	if (nobuff)
	{
		/* no buffering, read directly into user's buffer */
#ifdef DEBUG
#ifdef WRITE_BLK
fprintf(stderr,"writing directly\n");
#else
fprintf(stderr,"reading directly\n");
#endif
#endif
		source.i = fd;
		DO_READ_BLK(source,ptr,dims,esize,mb,bsize,bcount,PDIM,MFILE);
		}
	else
	{
		int valid = 1;
		/* buffering, transfer from macro-block buffer */
		if (!fp->buffer)
			return(-1);
		for (d = 0; d < dims; d++)
		{
			/* check for match with buffer */
			if (mb[d] != fp->buftag[d])
				match = 0;
			if (fp->bbuff[d] < 1 || fp->buftag[d] < 0)
				valid = 0;
			}
		if (!match)
		{
			source.i = fd;
			/* wrong macro-block, read the right macro-block */
			if (fp->dirty && valid)
			{
				/* write the current buffer back */
#ifdef DEBUG
fprintf(stderr,"writing old buffer\n");
#endif
				do_write_blk(source, fp->buffer, dims, esize,
					fp->buftag, bsize, bcount, PDIM, MFILE);
				}
#ifdef DEBUG
fprintf(stderr,"reading new buffer\n");
#endif
			do_read_blk(source, fp->buffer, dims, esize,
				mb, bsize, bcount, PDIM, MFILE);
			/* buffer is now clean */
			fp->dirty = CLEAN;
			bcopy(mb, fp->buftag, dims * sizeof(int));
			}
		/* we now have the right macro-block */
		/* copy the right block to the user's buffer */
		for (d = 0; d < dims; d++)
		{
			/* compute bsize and bcount for buffer */
			bsize[d] = fp->bsize[d];
			bcount[d] = fp->bbuff[d];
			/* compute requested block relative to buffer */
			mb[d] = block[d] % fp->bbuff[d];
			}
#ifdef DEBUG
fprintf(stderr,"buffer transfer\n");
#endif
		source.p = fp->buffer;
		DO_READ_BLK(source,ptr,dims,esize,mb,bsize,bcount,1,MBUFF);
#ifdef WRITE_BLK
		fp->dirty = DIRTY;
#endif		
		/* if this is a write, set dirty flag */
		}
	/* all done */
	if (dims > 10)
	{
		free(bsize);
		free(bcount);
		free(block);
		free(mb);
		}
	va_end(ap);
	return(0);
	}

/* mode indicates whether this is actually a file read or a buffer copy */

DO_READ_BLK(union fpu source, char *ptr, int dims, int esize,
	int block[], int bsize[], int bcount[], int pdim, int mode)
{
	int d;
	int read_size = 0;
	int seg_loc = 0;
	int part_offset = 0;
	int part_group = 0;
	int part_stride = 0;
	int index_store[10], size_store[10];
	int *index = index_store;
	int *size = size_store;
	char *malloc();
	if (dims > 10)
	{
		index = (int *)malloc(sizeof(int)*dims);
		size = (int *)malloc(sizeof(int)*dims);
		}
	for (d = 0; d < dims; d++)
	{
		int sksz; /* running size of  d-1 element */
		/* compute dim size into size array */
		if (!d) /* d is zero */
		{
			size[d] = esize;
			read_size = esize * bsize[d];
			sksz = esize * bsize[d];
			}
		else if (d < pdim)
		{
			size[d] = bsize[d-1] * size[d-1];
			read_size *= bsize[d];
			sksz *= bsize[d];
			}
		else /* d is not zero and not less than pdim */
		{
			size[d] = bcount[d-1] * bsize[d-1] * size[d-1];
			sksz *= bcount[d-1] * bsize[d];
			}
		if (d+1 < pdim)
		{
			/* compute partition offset */
			part_offset += sksz * block[d];
			part_group = esize * bsize[d];
			part_stride = part_group * bcount[d];
			}
		}
	/* perform partitioning */
#ifdef DEBUG
fprintf(stderr,"partitioning o=%d g=%d s=%d\n",part_offset,part_group,
part_stride);
#endif
	if (pdim > 1)
	{
		PART(source.i, part_offset, part_group, part_stride);
		}
	/* this implements a dims-level nested for loop, counting */
	/* from zero to bsize[d] 0<=d<dims */
	/* initialize indexes */
	for (d = 0; d < dims; d++)
	{
		/* if any loop bound is zero we will never execute */
		/* the loop body */
		if (bsize[d] == 0)
			return(-1); /* an error */
		index[d] = 0;
		}
	/* loop until largest index is not less than bsize */
	while(index[dims-1] < bsize[dims-1])
	{
		/* this is a little ass-backwards in that we first do
		the loop body, then check the loop bounds.  This works
		because we check to make sure we satisfy the conditions
		for the first iteration, and then we check for the next
		iteration at the bottom of the loop (after incrementing
		the index) */
		/* LOOP BODY BEGINS HERE */
		/* compute location of next segment */
		seg_loc = 0;
		for (d = pdim-1; d < dims; d++)
		{
			seg_loc += (index[d]+(block[d]*bsize[d]))*size[d];
			}
		switch (mode)
		{
		case MFILE :
#ifdef DEBUG
fprintf(stderr,"seeking to %d\n",seg_loc);
#endif
			LSEEK(source.i, seg_loc, SEEK_SET);
#ifdef DEBUG
#ifdef WRITE_BLK
fprintf(stderr,"writing %d bytes\n",read_size);
#else
fprintf(stderr,"reading %d bytes\n",read_size);
#endif
#endif
			READ(source.i, ptr, read_size);
			break;
		case MBUFF :
#ifdef DEBUG
#ifdef WRITE_BLK
fprintf(stderr,"copying %d bytes to %d\n",read_size,seg_loc);
#else
fprintf(stderr,"copying %d bytes from %d\n",read_size,seg_loc);
#endif
#endif
			BCOPY(&source.p[seg_loc],ptr,read_size);
			break;
			}
		ptr += read_size;
		/* LOOP BODY ENDS HERE */
		/* this is a quick hack to fix this exceptional condition */
		if (dims <= pdim)
		{
			break;
			}
		/* adjust indexes */
		for (d = pdim; d < dims && ++index[d] >= bsize[d]; d++)
		{
			/* do not re-zero the largest index */
			if (d != dims-1)
				index[d] = 0;
			}
		}
	if (dims > 10)
	{
		free(index);
		free(size);
		}
	}
