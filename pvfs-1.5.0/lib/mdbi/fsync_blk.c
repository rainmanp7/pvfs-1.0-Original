
/*
  fsync_blk.h: fsync file for buffered multi-dimensional blocks interface
  author: W. B. Ligon III
  date: 5/27/97
  org: Clemson University
  copyright (c) 1997 Clemson University, all rights reserved.
*/

#include "blocks.h"

fsync_blk(int fd)
{
	return FSYNC(fd);
	}
