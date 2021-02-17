/*
 * req.h copyright (c) 1996 Clemson University, all rights reserved.
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
 * Contact:  Rob Ross  	rbross@parl.clemson.edu
 *           Walt Ligon walt@parl.clemson.edu
 * 
 */

#ifndef REQ_H
#define REQ_H

#include <meta.h>
#include <utime.h>

/* defines for requests to manager */
#define MGR_MAJIK_NR 0x4a87c9fe

#define MGR_CHMOD    0
#define MGR_CHOWN    1
#define MGR_CLOSE    2
#define MGR_STAT     3
#define MGR_MOUNT    4
#define MGR_OPEN     5
#define MGR_UNLINK   6
#define MGR_SHUTDOWN 7
#define MGR_UMOUNT   8
#define MGR_FSTAT    9
#define MGR_RENAME   10
#define MGR_IOD_INFO 11
#define MGR_MKDIR    12
#define MGR_FCHOWN   13
#define MGR_FCHMOD   14
#define MGR_RMDIR    15
#define MGR_ACCESS   16
#define MGR_TRUNCATE 17
#define MGR_UTIME	   18
#define MGR_GETDENTS 19
#define MGR_STATFS   20

#define MAX_MGR_REQ  20

/* structure for request to manager */
typedef struct mreq mreq, *mreq_p;

struct mreq {
	int32_t majik_nr;
	int8_t type; /* type of request */
	int uid;   /* uid of program making request */
	int gid;   /* gid of program making request */
	int64_t dsize; /* size of data following request */
	union {
		struct {
			char *fname; /* filename */
			fmeta meta; /* metadata for file (when creating?) */
			int32_t   flag;
			int32_t   mode; /* permissions when creating new file */
		} open;
		struct {
			char *fname;
			int32_t owner;
			int32_t group;
		} chown;
		struct {
			char *fname;
			mode_t mode;
		} chmod;	
		struct {
			fmeta meta;
		} close;
		struct {
			char *fname;
		} stat;	
		struct {
			char *fsname;
		} mount;
		struct {
			char *fsname;
		} umount;
		struct {
			char *fname;
		} unlink;	
		struct {
			fmeta meta;
		} fstat;	
		struct {
			char *oname;
			char *nname;
		} rename;
		struct {
			char* fsname;
		} iod_info;	
		struct {
			char* fname;
			mode_t mode;
		} mkdir;	
		struct {
			int64_t fs_ino; /* using fs_ino and file_ino instead of meta struct */
			int64_t file_ino;
			int32_t owner;
			int32_t group;
		} fchown;
		struct {
			int64_t fs_ino; /* using fs_ino and file_ino instead of meta struct */
			int64_t file_ino;
			mode_t mode;
		} fchmod;	
		struct {
			char* fname;
		} rmdir;	
		struct {
			char* fname;
			mode_t mode;
		} access;	
		struct {
			char *fname;
			int64_t length;
		} truncate;
		struct {
			char *fname;
			struct utimbuf buf;
		} utime;
		struct {
			char *dname;
			int64_t offset;
			int64_t length;
		} getdents;
		struct {
			char *fname;
		} statfs;
	} req;
};

/* structure for ack from manager */
typedef struct mack mack, *mack_p;

struct mack {
	int32_t majik_nr;
	int8_t type;
	int status;
	int eno;
	int64_t dsize;
	union {
		struct {
			fmeta meta; /* metadata of opened file */
			int32_t cap;
		} open;
		struct {
			fmeta meta; /* metadata of checked file */
		} access;
		struct {
			fmeta meta; /* metadata of stat'd file */
		} stat;
		struct {
			fmeta meta; /* metadata requested */
		} fstat;	
		struct {
			int32_t nr_iods;
		} iod_info;
		struct {
			int64_t offset;
		} getdents;
		struct {
			int64_t tot_bytes;
			int64_t free_bytes;
			int32_t tot_files;
			int32_t free_files;
			int32_t namelen;
		} statfs;
	} ack;
};

/* defines for requests to iod */
#define IOD_MAJIK_NR 0x49e3ac9f
#define IOD_OPEN     0
#define IOD_CLOSE    1
#define IOD_STAT     2
#define IOD_UNLINK   3
#define IOD_RW       4
#define IOD_SHUTDOWN 5

/* defines for group requests */
#define IOD_GSTART   6
#define IOD_GADD     7

/* defines for instrumentation */
#define IOD_INSTR		8

/* more defines for iod requests */
#define IOD_FTRUNCATE 9
#define IOD_TRUNCATE  10
#define IOD_FDATASYNC 11 /* also used for fsync() */

/* a no-op request, for opening a connection w/out a request */
#define IOD_NOOP     12
#define IOD_STATFS   13

/* subtypes for RW requests */
#define IOD_RW_READ  0
#define IOD_RW_WRITE 1

#define MAX_IOD_REQ  13

/* flags for instr request */
#define INSTR_ON		1
#define INSTR_OFF		2
#define INSTR_PRNT 	4
#define INSTR_RST		8

/* structure for req to iod */
typedef struct ireq ireq, *ireq_p;

struct ireq {
	int32_t majik_nr;
	int8_t type;
	int64_t dsize;
	union {
		struct {
			int64_t fs_ino;
			int64_t f_ino; /* inode # of metadata file */
			int32_t cap;
			int flag; /* flags for opening */
			int mode; /* permissions when creating new file */
			pvfs_filestat p_stat; /* info on striping of file */
			int pnum; /* partition # for this file */
		} open;
		struct {
			int64_t fs_ino;
			int64_t f_ino;
			int cap;
		} close;
		struct {
			int64_t fs_ino;
			int64_t f_ino;
		} unlink;
		struct {
			int64_t fs_ino;
			int64_t f_ino;
		} stat;
		struct {
			char rw_type;  /* subtype - what kind of access */
			int64_t fs_ino;
			int64_t f_ino;   /* file (metadata file) inode # */
			int32_t cap;
			int64_t off;       /* initial offset */
			int64_t fsize;     /* first size */
			int64_t gsize;     /* group size */
			int64_t stride;    /* stride */
			int64_t gcount;    /* group count */
			int64_t lsize;     /* last size */
			int64_t gstride;   /* group stride */
			int64_t ngroups;   /* # of groups */
			int32_t c_nr;      /* connection # (for indirect I/O) */
		} rw;
		struct {
			int64_t fs_ino;
			int64_t f_ino;   /* inode # of metadata file */
			int32_t cap;       /* capability of scheduler */
			int32_t grp_nr;    /* suggested group # */
		} gadd;
		struct {
			int64_t fs_ino;
			int64_t f_ino;
			int32_t cap;
			int32_t grp_sz;    /* size (# of members) of group */
		} gstart;
		struct {
			int flags;		/* flags for information to return */
		} instr;
		struct {
			int64_t fs_ino;
			int64_t f_ino;
			int32_t cap;
			int64_t length;
		} ftruncate;
		struct {
			int64_t fs_ino;
			int64_t f_ino;
			pvfs_filestat p_stat;
			int64_t length;
			int32_t part_nr;
		} truncate;
		struct {
			int64_t fs_ino;
			int64_t f_ino;
			int32_t cap;
		} fdatasync;
	} req;
};

typedef struct iack iack, *iack_p;

struct iack {
	int32_t majik_nr;
	int8_t type;
	int status;
	int eno;
	int64_t dsize;
	union {
		struct {
			int64_t fsize;
		} stat;
		struct {
			int32_t grp_nr;   /* actual group # */
			int32_t grp_sz;   /* size of group */
		} gadd;
		struct {
			float avg;
			float wait;
			int32_t accesses;
			float dev;
		} instr;
		struct {
			int64_t tot_bytes;
			int64_t free_bytes;
		} statfs;
	} ack;
};

#endif
