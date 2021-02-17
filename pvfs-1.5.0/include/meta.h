/*
 * meta.h copyright (c) 1996 Clemson University, all rights reserved.
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
 * Contact:  Rob Ross  	rbross@parl.eng.clemson.edu
 * Matt Cettei mcettei@parl.eng.clemson.edu
 * 
 */


#ifndef META_H
#define META_H

/* includes for struct stat */
#include <sys/stat.h>
#include <unistd.h>

/* includes for sockaddr_in */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* includes for umode_t */
#include <asm/types.h>

#ifdef NEED_UMODE_T_DEF
typedef unsigned short umode_t;
#endif

typedef struct dmeta dmeta, *dmeta_p;

struct dmeta {
	int64_t fs_ino; /* file system root directory inode # */
	uid_t dr_uid; /* directory owner uid # */
	gid_t dr_gid; /* directory owner gid # */
	umode_t dr_mode; /* directory mode # */
	u_int16_t port;
	char *host;
	char *rd_path;
	char *sd_path;
	char *fname;
};

typedef struct pvfs_filestat pvfs_filestat, *pvfs_filestat_p;
	
struct pvfs_filestat {
	int32_t base;
	int32_t pcount;
	int32_t ssize;
	int32_t soff;
	int32_t bsize;
};

typedef struct fmeta fmeta, *fmeta_p;

struct fmeta {
	int64_t fs_ino; /* file system root directory inode # */
	struct stat u_stat;
	struct pvfs_filestat p_stat;
	struct sockaddr mgr;
};

#endif

