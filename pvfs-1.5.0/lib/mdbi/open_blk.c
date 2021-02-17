
/*
  open_blk.h: open file for buffered multi-dimensional blocks interface
  author: W. B. Ligon III
  date: 5/27/97
  org: Clemson University
  copyright (c) 1997 Clemson University, all rights reserved.
*/

#include "blocks.h"

open_blk(char *path, int flags, int mode)
{
	return OPEN(path, flags, mode);
	}
