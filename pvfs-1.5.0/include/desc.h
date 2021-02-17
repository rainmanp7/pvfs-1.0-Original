/*
 * desc.h copyright (c) 1996 Clemson University, all rights reserved.
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * any later version.
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
 * Contact:  Rob Ross  	rbross@parl.clemson.edu
 *           Walt Ligon walt@parl.clemson.edu
 * 
 */

#ifndef DESC_H
#define DESC_H

#include <meta.h>
#include <req.h>
#include <pvfs_config.h>

#define STRICT_DESC_CHECK

#ifdef STRICT_DESC_CHECK
#define IOD_INFO_CHECKVAL 123456789
#define FDESC_CHECKVAL    234567890
#endif

#define O_PASS  O_GADD /* passive connection */
#define O_SCHED O_GSTART /* scheduling connection */

/* FS TYPE DEFINES -- USED IN FDESC STRUCTUES */
#define FS_UNIX 0 /* UNIX file/directory */
#define FS_PVFS 1 /* PVFS file */
#define FS_RESV 2 /* reserved (socket connected to PVFS daemon) */
#define FS_PDIR 3 /* PVFS directory */

typedef struct fpart fpart, *fpart_p;

struct fpart {
	int64_t offset;
	int64_t gsize;
	int64_t stride;
	int64_t gstride;
	int64_t ngroups;
};


typedef struct iod_info iod_info, *iod_info_p;

struct iod_info {
#ifdef STRICT_DESC_CHECK
	int32_t checkval;
#endif
	struct sockaddr_in addr;
	iack ack;
	int sock;
};

typedef struct fdim fdim, *fdim_p;

struct fdim {
	int32_t dims;	/* # of dimensions */
	int32_t rcc, bc;
	int32_t esize;	/* size of element */
	int8_t dirty;	/* flags if buffer is dirty */
	char *buffer;	/* points to block buffer */
	int *buftag;	/* points to current buffer tag */
	int *bsize;	/* points to block size */
	int *bcount;	/* points to block count */
	int *bbuff;	/* points to buffering factor */
};

typedef struct fdesc fdesc, *fdesc_p;

struct fdesc {
#ifdef STRICT_DESC_CHECK
	int checkval;
#endif
	int8_t fs; /* no longer boolean -- SEE DEFINES ABOVE! */
	fpart_p part_p;
	fdim_p dim_p;
	char *fn_p; /* pointer to optionally available file name -- */
	            /* in terms of manager's view! */
	struct {
		int64_t off;
		fmeta meta;
		int32_t cap; /* capability */
		int32_t ref; /* number of pfds referencing this structure */
		int flag; /* flags used to open file */
		int32_t grp_nr; /* # in group (if using scheduled I/O) */
		iod_info iod[1];
	} fd;
};

#endif
