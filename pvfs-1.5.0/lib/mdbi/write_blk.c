
/*
  write_blk.h: write file for buffered multi-dimensional blocks interface
  author: W. B. Ligon III
  date: 5/27/97
  org: Clemson University
  copyright (c) 1997 Clemson University, all rights reserved.
*/

#include "blocks.h"

#define WRITE_BLK
#undef  READ
#ifdef  NOPVFS
#define READ write
#else
#define READ pvfs_write
#endif
#define BCOPY(p1,p2,sz) bcopy(p2,p1,sz);
#define READ_BLK write_blk
#define DO_READ_BLK do_write_blk

#include "read_blk.c"
