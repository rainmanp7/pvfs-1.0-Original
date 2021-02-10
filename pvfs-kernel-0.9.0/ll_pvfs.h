/* Hey Emacs! This file is -*- nroff -*- source.
 * 
 * Copyright (c) 1996 Clemson University, All Rights Reserved.
 * 2000 Argonne National Laboratory.
 * Originally Written by Rob Ross.
 *
 * Parallel Architecture Research Laboratory, Clemson University
 * Mathematics and Computer Science Division, Argonne National Laboratory
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **Code** This Protected Resource Provided By = rainmanp7
 * Contact:  
 * Rob Ross			    rross@mcs.anl.gov
 * Matt Cettei			mcettei@parl.eng.clemson.edu
 * Chris Brown			muslimsoap@gmail.com
 * Walt Ligon			walt@parl.clemson.edu
 * Robert Latham		robl@mcs.anl.gov
 * Neill Miller 		neillm@mcs.anl.gov
 * Frank Shorter		fshorte@parl.clemson.edu
 * Harish Ramachandran	rharish@parl.clemson.edu
 * Dale Whitchurch		dalew@parl.clemson.edu
 * Mike Speth			mspeth@parl.clemson.edu
 * Brad Settlemyer		bradles@CLEMSON.EDU
 */

#ifndef _LL_PVFS_H
#define _LL_PVFS_H
#ifdef __KERNEL__
#include <linux/types.h>
#endif

#ifndef __KERNEL__
#ifdef LINUX
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#endif
#endif

/* NOTE: if a type doesn't necessarily have a fixed size, it is defined
 * to have one here...
 */

/* miscellaneous defines */
typedef uint32_t pvfs_magic_t;
typedef int32_t pvfs_error_t;
typedef int32_t pvfs_seq_t; /* sequence number for identifying requests */
typedef uint8_t pvfs_boolean_t;
enum {
	PVFS_UPCALL_MAGIC = 0x12345678,
	PVFS_DOWNCALL_MAGIC = 0x9abcdef0
};

enum {
	NO_SEQUENCE_NR = -1
};

/* metadata associated defines */
typedef uint64_t pvfs_handle_t;
typedef uint64_t pvfs_off_t;
typedef uint64_t pvfs_size_t;
typedef time_t pvfs_time_t;
typedef uint32_t pvfs_uid_t;
typedef uint32_t pvfs_gid_t;
typedef uint32_t pvfs_mode_t;
typedef uint32_t bitfield_t;
enum {
	V_MODE = 1, /* mode is valid */
	V_UID = 2, /* UID is valid */
	V_GID = 4, /* GID is valid */
	V_SIZE = 8, /* file size is valid */
	V_TIMES = 16, /* time values are valid */
	V_BLKSIZE = 32, /* blocksize is valid */
	V_BLOCKS = 64 /* number of blocks is valid */
};

/* dirent associated defines */
typedef uint8_t pvfs_char_t;
enum {
	PVFSNAMELEN = 1023
};

/* physical distribution associated defines */
typedef uint32_t pvfs_count_t;
typedef int16_t pvfs_type_t;
enum {
	DIST_RROBIN = 1
};

/* superblock associated defines */
typedef uint32_t pvfs_flags_t;
enum {
	PVFSHOSTLEN = 64,
	PVFSDIRLEN = 1023
};

/* io description associated defines */
enum {
	IO_CONTIG = 1
};

/* data transfer protocol associated defines */
typedef uint8_t pvfs_xferproto_t;
enum {
	PVFS_PROTO_TCP = 1
};

/* hint associated defines */
enum {
	HINT_OPEN = 1,
	HINT_CLOSE = 2
};

/* all the major structures */
struct pvfs_statfs {
	pvfs_size_t bsize; /* block size (in bytes) */
	pvfs_size_t blocks; /* total blocks */
	pvfs_size_t bfree; /* free blocks */
	pvfs_size_t bavail; /* free blocks available to non-superuser */
	pvfs_size_t files; /* total files */
	pvfs_size_t ffree; /* free files */
	uint16_t namelen; /* maximum name length */
};

struct pvfs_dirent {
	pvfs_handle_t handle;
	pvfs_off_t off; /* offset to THIS entry */
	uint16_t len; /* length of this entry */
	pvfs_char_t name[PVFSNAMELEN+1]; /* null terminated */
};

struct pvfs_meta {
	pvfs_handle_t handle; /* similar to inode number, always valid */
	bitfield_t valid; /* bitfield specifying what fields are valid */
	pvfs_mode_t mode; /* protection */
	pvfs_uid_t uid; /* user ID of owner */
	pvfs_gid_t gid; /* group ID of owner */
	pvfs_size_t size; /* total size of file in bytes */
	pvfs_time_t atime; /* time of last access (all times under one bit) */
	pvfs_time_t mtime; /* time of last modification */
	pvfs_time_t ctime; /* time of last change */
	pvfs_size_t blksize; /* ``suggested'' block size (usually strip size) */
	pvfs_size_t blocks; /* number of ``suggested'' blocks */
};

struct pvfs_procinfo {
	pvfs_uid_t uid;
	pvfs_gid_t gid;
};

enum {
	DEFAULT_BLKSIZE = 65536,
	DEFAULT_NODECT = -1,
	DEFAULT_DIST = DIST_RROBIN
};

struct pvfs_phys {
	pvfs_size_t blksize; /* unit of distribution (strip), in bytes */
	pvfs_count_t nodect; /* number of nodes in distribution */
	pvfs_type_t dist; /* distribution function indicator */
};

struct pvfs_super {
	pvfs_flags_t flags;
	uint16_t port;
	pvfs_char_t mgr[PVFSHOSTLEN+1]; /* null terminated */
	pvfs_char_t dir[PVFSDIRLEN+1]; /* null terminated */
};

struct pvfs_inode {
	pvfs_handle_t handle;
	pvfs_char_t *name;
	struct pvfs_super *super;
};

struct pvfs_io {
	pvfs_type_t type; /* type of I/O description */
	union {
		struct {
			pvfs_off_t off; /* offset relative to entire logical file */
			pvfs_size_t size; /* size of contiguous region to access */
		} contig;
	} u;
};

/* low level interface prototypes */
int ll_pvfs_rename(struct pvfs_inode *old_inode, struct pvfs_inode
	*new_inode);
int ll_pvfs_lookup(struct pvfs_super *sb, const pvfs_char_t *name,
	pvfs_size_t len, struct pvfs_meta *meta);
int ll_pvfs_mkdir(struct pvfs_super *sb, const pvfs_char_t *name,
	pvfs_size_t len, struct pvfs_meta *meta);
int ll_pvfs_create(struct pvfs_super *sb, const pvfs_char_t *name,
	pvfs_size_t len, struct pvfs_meta *meta, struct pvfs_phys *phys);
int ll_pvfs_setmeta(struct pvfs_inode *inode, struct pvfs_meta *meta);
int ll_pvfs_getmeta(struct pvfs_inode *inode, struct pvfs_meta *meta,
	struct pvfs_phys *phys);
int ll_pvfs_statfs(struct pvfs_super *sb, struct pvfs_statfs *sbuf);
int ll_pvfs_readdir(struct pvfs_inode *inode, struct pvfs_dirent *dirent,
	pvfs_off_t *offp);
int ll_pvfs_rmdir(struct pvfs_inode *inode);
int ll_pvfs_unlink(struct pvfs_inode *inode);
int ll_pvfs_file_read(struct pvfs_inode *inode, pvfs_char_t *buf,
	pvfs_size_t count, pvfs_off_t *offp, pvfs_boolean_t to_kmem);
int ll_pvfs_file_write(struct pvfs_inode *inode, pvfs_char_t *buf,
	pvfs_size_t count, pvfs_off_t *offp, pvfs_boolean_t to_kmem);
int ll_pvfs_fsync(struct pvfs_inode *inode);
int ll_pvfs_hint(struct pvfs_inode *inode, pvfs_type_t hint);


/* low level protocol structures
 *
 * Here we're doing something similar to the Coda upcall/downcall
 * concept.  This will probably be continued into the user-space
 * implementation at some point...
 *
 * Or maybe we'll decide we don't like it and scrap the whole thing?
 */

enum {
	NULL_OP     = -1, /* not implemented for v1.x */
	GETMETA_OP  = 2,
	SETMETA_OP  = 3,
	LOOKUP_OP   = 4,
	RLOOKUP_OP  = -1, /* not implemented for v1.x */
	READLINK_OP = -1, /* not implemented for v1.x */
	CREATE_OP   = 7,
	REMOVE_OP   = 8,
	RENAME_OP   = 9,
	SYMLINK_OP  = -1, /* not implemented for v1.x */
	MKDIR_OP    = 11,
	RMDIR_OP    = 12,
	GETDENTS_OP = 13,
	STATFS_OP   = 14, /* not implemented until 1.4.8-pre-xxx */
	EXPORT_OP   = -1, /* not implemented for v1.x */
	MKDIRENT_OP = -1, /* not implemented for v1.x */
	RMDIRENT_OP = -1, /* not implemented for v1.x */
	FHDUMP_OP   = -1, /* not implemented for v1.x */
	HINT_OP     = 19,
	READ_OP     = 20,
	WRITE_OP    = 21
	/* SHOULD THERE BE SOME SORT OF SYNC CALL?  DATASYNC? */
};

struct pvfs_upcall {
	pvfs_magic_t magic;
	pvfs_seq_t seq; /* only positive sequence numbers should be used */
	pvfs_type_t type;
	struct pvfs_procinfo proc; /* info on process for determining priveleges */
	union {
		struct {
			/* empty */
		} null;
		struct {
			pvfs_handle_t handle;
		} getmeta;
		struct {
			struct pvfs_meta meta;
		} setmeta;
		struct {
			pvfs_char_t name[PVFSNAMELEN+1];
		} lookup;
		struct {
			/* not implemented in v1.x */
		} rlookup;
		struct {
			/* not implemented in v1.x */
		} readlink;
		struct {
			pvfs_char_t name[PVFSNAMELEN+1];
			struct pvfs_meta meta;
			struct pvfs_phys phys;
		} create;
		struct {
			pvfs_handle_t handle;
		} remove;
		struct {
			pvfs_handle_t handle;
			pvfs_char_t new_name[PVFSNAMELEN+1];
		} rename;
		struct {
			/* not implemented in v1.x */
		} symlink;
		struct {
			pvfs_char_t name[PVFSNAMELEN+1];
			struct pvfs_meta meta;
		} mkdir;
		struct {
			/* why do we pass this name but not one for remove? */
			pvfs_char_t name[PVFSNAMELEN+1];
		} rmdir;
		struct {
			pvfs_handle_t handle;
			pvfs_off_t off;
			pvfs_count_t count;
		} getdents;
		struct {
			pvfs_handle_t handle;
		} statfs;
		struct {
			/* not implemented in v1.x */
		} export;
		struct {
			/* not implemented in v1.x */
		} mkdirent;
		struct {
			/* not implemented in v1.x */
		} rmdirent;
		struct {
			/* not implemented in v1.x */
		} fhdump;
		struct {
			pvfs_handle_t handle;
			pvfs_type_t hint;
		} hint;
		struct {
			pvfs_handle_t handle;
			struct pvfs_io io;
		} rw; /* read and write both handled here */
	} u;
	struct {
		/* v1.xx needs names, in terms of the manager's file system, for
		 * all calls.  newer code will not have this problem, so we're
		 * going to stick the name down here out of the way so it doesn't
		 * pollute our otherwise clean v2.xx operation list
		 *
		 * this structure will always hold the file name.
		 */
		pvfs_char_t fhname[PVFSNAMELEN+1];
	} v1;
	struct {
		void *ptr;
		pvfs_size_t size;
		/* used in kernel to differentiate between
		 * user-space and kernel-space addresses
		 */
		pvfs_boolean_t to_kmem;
	} xfer; /* implementation-specific data transfer elements */
};

struct pvfs_downcall {
	pvfs_magic_t magic;
	pvfs_seq_t seq; /* matching sequence number, or NO_SEQUENCE_NR */
	pvfs_type_t type;
	pvfs_error_t error; /* 0 on success, negative for error performing op */
	union {
		struct {
			/* empty */
		} null;
		struct {
			struct pvfs_meta meta;
			struct pvfs_phys phys;
		} getmeta;
		struct {
			struct pvfs_meta meta;
			struct pvfs_phys phys;
		} setmeta;
		struct {
			struct pvfs_meta meta; /* includes handle */
		} lookup;
		struct {
			/* not implemented in v1.x */
		} rlookup;
		struct {
			/* not implemented in v1.x */
		} readlink;
		struct {
			/* empty */
		} create;
		struct {
			/* empty */
		} remove;
		struct {
			/* empty */
		} rename;
		struct {
			/* not implemented in v1.x */
		} symlink;
		struct {
			/* empty */
		} mkdir;
		struct {
			/* empty */
		} rmdir;
		struct {
			pvfs_boolean_t eof;
			pvfs_off_t off; /* new offset, after operation */
			pvfs_count_t count;
		} getdents;
		struct {
			struct pvfs_statfs statfs;
		} statfs;
		struct {
			/* not implemented in v1.x */
		} export;
		struct {
			/* not implemented in v1.x */
		} mkdirent;
		struct {
			/* not implemented in v1.x */
		} rmdirent;
		struct {
			/* not implemented in v1.x */
		} fhdump;
		struct {
		} hint;
		struct {
			pvfs_boolean_t eof; /* non-zero when trying to read past eof */
			pvfs_size_t size; /* size of data read, in bytes */
		} rw; /* read and write both handled here */
	} u;
	struct {
		void *ptr;
		pvfs_size_t size;
	} xfer; /* implementation-specific data transfer elements */
};

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */

#endif
