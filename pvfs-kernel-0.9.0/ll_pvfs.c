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

/* Low-level PVFS VFS operations, called by our VFS functions.
 *
 * CALLS IMPLEMENTED IN THIS FILE:
 *   WORKING:
 *     ll_pvfs_file_read()
 *     ll_pvfs_readdir()
 *     ll_pvfs_getmeta()
 *     ll_pvfs_create()
 *     ll_pvfs_lookup()
 *     ll_pvfs_mkdir()
 *     ll_pvfs_rmdir()
 *     ll_pvfs_unlink()
 *     ll_pvfs_file_write()
 *     ll_pvfs_fsync()
 *     ll_pvfs_statfs() [returns dummy data]
 *     ll_pvfs_truncate()
 *     ll_pvfs_setmeta()
 *   NOT IMPLEMENTED:
 *     ll_pvfs_rename()
 *
 * NOTES:
 *
 * These calls are responsible for:
 * - handling communication with the PVFS device
 * - checking error conditions on the returned downcall
 *
 */

#include <config.h>

#ifdef __KERNEL__
#define __NO_VERSION__
#include <pvfs_kernel_config.h>
#endif

#include <ll_pvfs.h>
#include <pvfs_linux.h>
#include <pvfsdev.h>

static void init_structs(struct pvfs_upcall *up, struct pvfs_downcall *dp,
	pvfs_type_t type);

/* ll_pvfs_lookup(sb, name, len, meta)
 */
int ll_pvfs_lookup(struct pvfs_super *sb, const pvfs_char_t *name,
pvfs_size_t len, struct pvfs_meta *meta)
{
	int error;
	struct pvfs_upcall up;
	struct pvfs_downcall down;

	PDEBUG(D_LLEV, "ll_pvfs_lookup called for %s\n", name);
	init_structs(&up, &down, LOOKUP_OP);
	strncpy(up.u.lookup.name, name, PVFSNAMELEN);
	strncpy(up.v1.fhname, name, PVFSNAMELEN);

	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		PERROR("ll_pvfs_lookup failed on enqueue for %s\n", name);
		return error;
	}

	if (down.error < 0) {
		PDEBUG(D_DOWNCALL, "ll_pvfs_lookup failed on downcall for %s\n", name);
		return down.error;
	}

	*meta = down.u.lookup.meta;
	/* NOTE: for some reason the "blocks" value in the linux stat
	 * structure seems to need to be in terms of 512-byte blocks.  Dunno
	 * why, but du and others rely on it.  There are three places in this
	 * file where we account for this: ll_pvfs_setmeta(),
	 * ll_pvfs_lookup() and ll_pvfs_getmeta().
	 */
	meta->blocks = (meta->size / 512) + ((meta->size % 512) ? 1 : 0);
	return 0;
}


/* ll_pvfs_hint(inode, hint)
 *
 */
int ll_pvfs_hint(struct pvfs_inode *inode, pvfs_type_t hint)
{
	int error;
	struct pvfs_upcall up;
	struct pvfs_downcall down;

	PDEBUG(D_LLEV, "ll_pvfs_hint called for %s\n", inode->name);

	init_structs(&up, &down, HINT_OP);
	up.u.hint.handle = inode->handle;
	up.u.hint.hint = hint;
	strncpy(up.v1.fhname, inode->name, PVFSNAMELEN);

	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		PERROR("ll_pvfs_hint failed on enqueue for %s\n", inode->name);
		return error;
	}

	if (down.error < 0) {
		PERROR("ll_pvfs_hint failed on downcall for %s\n", inode->name);
		return down.error;
	}

	return 0;
}

/* ll_pvfs_rmdir(inode)
 */
int ll_pvfs_rmdir(struct pvfs_inode *inode)
{
	int error;
	struct pvfs_upcall up;
	struct pvfs_downcall down;

	PDEBUG(D_LLEV, "ll_pvfs_rmdir called for %s\n", inode->name);

	init_structs(&up, &down, RMDIR_OP);
	strncpy(up.u.rmdir.name, inode->name, PVFSNAMELEN);
	strncpy(up.v1.fhname, inode->name, PVFSNAMELEN);

	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		PERROR("ll_pvfs_rmdir failed on enqueue for %s\n", inode->name);
		return error;
	}

	if (down.error < 0) {
		PERROR("ll_pvfs_rmdir failed on downcall for %s\n", inode->name);
		return down.error;
	}

	return 0;
}

/* ll_pvfs_mkdir(sb, name, len, meta)
 */
int ll_pvfs_mkdir(struct pvfs_super *sb, const pvfs_char_t *name,
pvfs_size_t len, struct pvfs_meta *meta)
{
	int error;
	struct pvfs_upcall up;
	struct pvfs_downcall down;

	PDEBUG(D_LLEV, "ll_pvfs_mkdir called for %s\n", name);

	init_structs(&up, &down, MKDIR_OP);
	strncpy(up.v1.fhname, name, PVFSNAMELEN);
	strncpy(up.u.mkdir.name, name, PVFSNAMELEN);
	up.u.mkdir.meta = *meta;

	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		PERROR("ll_pvfs_mkdir failed on enqueue for %s\n", name);
		return error;
	}

	if (down.error < 0) {
		PERROR("ll_pvfs_mkdir failed on downcall for %s\n", name);
		return down.error;
	}

	return 0;
}

/* ll_pvfs_create(sb, name, len, meta, phys)
 *
 * NOTE:
 * This function will silently ignore the situation where a file exists
 * prior to this call -- specifically if another node has created a file
 * between the time when this node has identified a file as not
 * existing, then this would normally cause an error (EEXIST).  Since
 * this can happen all the time with parallel applications, we will
 * ignore the error.
 *
 * PARAMETERS:
 * sb - pointer to PVFS superblock information
 * name - pointer to full path name
 * len - length of file name (not used)
 * meta - pointer to desired metadata (input only)
 * phys - pointer to desired physical distribution (input only)
 *
 * Returns 0 on success, -errno on failure.
 */
int ll_pvfs_create(struct pvfs_super *sb, const pvfs_char_t *name,
pvfs_size_t len, struct pvfs_meta *meta, struct pvfs_phys *phys)
{
	int error;
	struct pvfs_upcall up;
	struct pvfs_downcall down;

	PDEBUG(D_LLEV, "ll_pvfs_create called for %s\n", name);

	init_structs(&up, &down, CREATE_OP);
	strncpy(up.v1.fhname, name, PVFSNAMELEN);
	strncpy(up.u.create.name, name, PVFSNAMELEN);
	up.u.create.meta = *meta;
	up.u.create.phys = *phys;

	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		PERROR("ll_pvfs_create failed on enqueue for %s\n", name);
		return error;
	}
	if (down.error < 0) {
		if (down.error == -EEXIST) {
			/* this is a common occurrence; file is created by one task
			 * between the time that one stat's for existence and then
			 * tries to create the file.  we will just act like everything
			 * is ok.
			 */
			return 0;
		}
		PERROR("ll_pvfs_create failed on downcall for %s\n", name);
		return down.error;
	}

	return 0;
}

/* ll_pvfs_rename(old_inode, new_inode)
 *
 */
int ll_pvfs_rename(struct pvfs_inode *old_inode, struct pvfs_inode *new_inode)
{
	int error;
	struct pvfs_upcall up;
	struct pvfs_downcall down;

	PDEBUG(D_LLEV, "ll_pvfs_rename called for %ld\n",
	(long) old_inode->handle);
	init_structs(&up, &down, RENAME_OP);
	up.u.rename.handle = old_inode->handle;
	strncpy(up.v1.fhname, old_inode->name, PVFSNAMELEN);
	strncpy(up.u.rename.new_name, new_inode->name, PVFSNAMELEN);
	strncpy(up.v1.fhname, old_inode->name, PVFSNAMELEN);

	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		PERROR("ll_pvfs_rename failed on enqueue for %s\n", old_inode->name);
		return error;
	}

	if (down.error < 0) {
		PERROR("ll_pvfs_rename failed on downcall for %s\n", old_inode->name);
		return down.error;
	}

	return 0;
}

/* ll_pvfs_unlink(inode)
 *
 */
int ll_pvfs_unlink(struct pvfs_inode *inode)
{
	int error;
	struct pvfs_upcall up;
	struct pvfs_downcall down;
	
	PDEBUG(D_LLEV, "ll_pvfs_unlink called for %ld\n", (long) inode->handle);
	init_structs(&up, &down, REMOVE_OP);
	up.u.remove.handle = inode->handle;
	strncpy(up.v1.fhname, inode->name, PVFSNAMELEN);

	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		PERROR("ll_pvfs_unlink failed on enqueue for %s\n", inode->name);
		return error;
	}

	if (down.error < 0) {
		PERROR("ll_pvfs_unlink failed on downcall for %s\n", inode->name);
		return down.error;
	}

	return 0;
}

/* ll_pvfs_setmeta(inode, meta)
 *
 */
int ll_pvfs_setmeta(struct pvfs_inode *inode, struct pvfs_meta *meta)
{
	int error;
	struct pvfs_upcall up;
	struct pvfs_downcall down;
	
	PDEBUG(D_LLEV, "ll_pvfs_setmeta called for %ld\n", (long) inode->handle);
	init_structs(&up, &down, SETMETA_OP);
	up.u.setmeta.meta = *meta;
	strncpy(up.v1.fhname, inode->name, PVFSNAMELEN);

	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		PERROR("ll_pvfs_setmeta failed on enqueue for %s\n", inode->name);
		return error;
	}

	if (down.error < 0) {
		PERROR("ll_pvfs_setmeta failed on downcall for %s\n", inode->name);
		return down.error;
	}

	*meta = down.u.setmeta.meta;
	/* NOTE: for some reason the "blocks" value in the linux stat
	 * structure seems to need to be in terms of 512-byte blocks.  Dunno
	 * why, but du and others rely on it.  There are three places in this
	 * file where we account for this: ll_pvfs_setmeta(),
	 * ll_pvfs_lookup() and ll_pvfs_getmeta().
	 */
	meta->blocks = (meta->size / 512) + ((meta->size % 512) ? 1 : 0);
	/* not doing anything with the phys info in the downcall for now */
	return 0;
}

/* ll_pvfs_getmeta(inode, meta, phys)
 *
 */
int ll_pvfs_getmeta(struct pvfs_inode *inode, struct pvfs_meta *meta,
struct pvfs_phys *phys)
{
	int error;
	struct pvfs_upcall up;
	struct pvfs_downcall down;
	
	PDEBUG(D_LLEV, "ll_pvfs_getmeta called for %ld\n", (long) inode->handle);
	init_structs(&up, &down, GETMETA_OP);
	strncpy(up.v1.fhname, inode->name, PVFSNAMELEN);

	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		PERROR("ll_pvfs_getmeta failed on enqueue for %s\n", inode->name);
		return error;
	}

	if (down.error < 0) {
		PERROR("ll_pvfs_getmeta failed on downcall for %s\n", inode->name);
		return down.error;
	}

	*meta = down.u.getmeta.meta;
	/* NOTE: for some reason the "blocks" value in the linux stat
	 * structure seems to need to be in terms of 512-byte blocks.  Dunno
	 * why, but du and others rely on it.  There are three places in this
	 * file where we account for this: ll_pvfs_setmeta(),
	 * ll_pvfs_lookup() and ll_pvfs_getmeta().
	 */
	meta->blocks = (meta->size / 512) + ((meta->size % 512) ? 1 : 0);

	if (phys != NULL) *phys = down.u.getmeta.phys;
	return 0;
}

/* ll_pvfs_statfs(sb, statfsbuf)
 */
int ll_pvfs_statfs(struct pvfs_super *sb, struct pvfs_statfs *sbuf)
{
	int error;
	struct pvfs_upcall up;
	struct pvfs_downcall down;
	char namebuf[PVFSHOSTLEN + PVFSNAMELEN + 8];

	sprintf(namebuf, "%s:%d%s", sb->mgr, sb->port, sb->dir);
	
	PDEBUG(D_LLEV, "ll_pvfs_statfs called\n");
	init_structs(&up, &down, STATFS_OP);
	up.u.statfs.handle = 0;

	/* NOTE: THERE IS POTENTIAL FOR TRUNCATION HERE!!! */
	strncpy(up.v1.fhname, namebuf, PVFSNAMELEN);

	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		PERROR("ll_pvfs_statfs failed on enqueue for %s\n", namebuf);
		return error;
	}

	if (down.error < 0) {
		PERROR("ll_pvfs_statfs failed on downcall for %s\n", namebuf);
		return down.error;
	}

	*sbuf = down.u.statfs.statfs;
	return 0;
}

/* ll_pvfs_readdir(dir, off, dirent)
 *
 * Returns 0 on success, error on failure.  In the case of EOF, length
 * field of returned pvfs_dirent structure is set to 0.
 */
int ll_pvfs_readdir(struct pvfs_inode *inode, struct pvfs_dirent *dirent,
pvfs_off_t *offp)
{
	int error;
	pvfs_off_t off;
	struct pvfs_upcall up;
	struct pvfs_downcall down;

	off = *offp;

	PDEBUG(D_LLEV, "ll_pvfs_readdir called for %ld, offset %ld\n",
	(long) inode->handle, (long) off);
	init_structs(&up, &down, GETDENTS_OP);
	up.u.getdents.handle = inode->handle;
	up.u.getdents.off = off;
	up.u.getdents.count = 1;

	/* describe buffer */
	up.xfer.to_kmem = 1;
	up.xfer.ptr = dirent; /* note: this is a kernel space buffer */
	up.xfer.size = sizeof(*dirent);
	strncpy(up.v1.fhname, inode->name, PVFSNAMELEN);

	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		PERROR("ll_pvfs_readdir failed on enqueue for %s\n", inode->name);
		return error;
	}

	if (down.error < 0) {
		PERROR("ll_pvfs_readdir failed on downcall for %s\n", inode->name);
		return down.error;
	}

	if ((down.u.getdents.eof != 0) || (down.u.getdents.count == 0)) {
		memset(dirent, 0, sizeof(struct pvfs_dirent));
	}

	PDEBUG(D_LLEV, "ll_pvfs_readdir: name = %s, len = %d, eof = %d\n",
	dirent->name, dirent->len, down.u.getdents.eof);

	/* pass back the new offset, as reported in the downcall */
	*offp = down.u.getdents.off;

	return 0;
}

/* ll_pvfs_file_read()
 *
 * Returns size of data read on success.  Returns error (< 0) on
 * failure.  Sets new offset if and only if one or more bytes are
 * successfully read (is this a good idea?).
 *
 * The to_kmem flag indicates that the buffer is in kernel space, not in
 * user space.
 */
int ll_pvfs_file_read(struct pvfs_inode *inode, pvfs_char_t *buf,
pvfs_size_t count, pvfs_off_t *offp, pvfs_boolean_t to_kmem)
{
	int error = 0;
	struct pvfs_upcall up;
	struct pvfs_downcall down;

	PDEBUG(D_LLEV, "ll_pvfs_file_read called for %s (%ld), size %ld, loc %ld\n",
	inode->name, (long) inode->handle, (long) count, (long) (*offp));

	init_structs(&up, &down, READ_OP);
	up.u.rw.handle = inode->handle;
	up.u.rw.io.type = IO_CONTIG;
	up.u.rw.io.u.contig.off = *offp;
	up.u.rw.io.u.contig.size = count;

	/* buffer */
	up.xfer.to_kmem = to_kmem;
	up.xfer.ptr = buf;
	up.xfer.size = count;
	strncpy(up.v1.fhname, inode->name, PVFSNAMELEN);


	/* send off the request */
	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		/* error in servicing upcall */
		PERROR("ll_pvfs_file_read failed on %ld\n", (long) inode->handle);
		return error;
	}

	if (down.error < 0) {
		/* error performing operation for process */
		PERROR("ll_pvfs_file_read got error in downcall\n");
		return down.error;
	}

	if (down.u.rw.eof != 0) return 0;

	*offp += down.u.rw.size;
	return down.u.rw.size;
}

/* ll_pvfs_file_write()
 *
 * Basically a carbon copy of ll_pvfs_read_file()
 *
 * The to_kmem flag indicates that the buffer is in kernel space, not in
 * user space.
 */
int ll_pvfs_file_write(struct pvfs_inode *inode, pvfs_char_t *buf,
pvfs_size_t count, pvfs_off_t *offp, pvfs_boolean_t to_kmem)
{
	int error = 0;
	struct pvfs_upcall up;
	struct pvfs_downcall down;

	PDEBUG(D_LLEV, "ll_pvfs_file_write called for %s (%ld), size %ld, loc %ld\n",
	inode->name, (long) inode->handle, (long) count, (long) (*offp));

	init_structs(&up, &down, WRITE_OP);
	up.u.rw.handle = inode->handle;
	up.u.rw.io.type = IO_CONTIG;
	up.u.rw.io.u.contig.off = *offp;
	up.u.rw.io.u.contig.size = count;

	/* buffer */
	up.xfer.to_kmem = to_kmem;
	up.xfer.ptr = buf;
	up.xfer.size = count;
	strncpy(up.v1.fhname, inode->name, PVFSNAMELEN);

	/* send off the request */
	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		/* error in servicing upcall */
		PERROR("ll_pvfs_file_write failed on %ld\n", (long) inode->handle);
		return error;
	}

	if (down.error < 0) {
		/* error performing operation for process */
		PERROR("ll_pvfs_file_write got error in downcall\n");
		return down.error;
	}

	if (down.u.rw.eof != 0) return 0;

	*offp += down.u.rw.size;
	return down.u.rw.size;
}

/* ll_pvfs_file_fsync()
 *
 */
int ll_pvfs_fsync(struct pvfs_inode *inode)
{
	int error = 0;
	struct pvfs_upcall up;
	struct pvfs_downcall down;

	PDEBUG(D_LLEV, "ll_pvfs_fsync called for %ld\n", (long) inode->handle);
	init_structs(&up, &down, GETMETA_OP);
	up.u.getmeta.handle = inode->handle;
	strncpy(up.v1.fhname, inode->name, PVFSNAMELEN);

	/* send off the request */
	if ((error = pvfsdev_enqueue(&up, &down, 1)) < 0) {
		/* error in servicing upcall */
		PERROR("ll_pvfs_fsync failed on %ld\n", (long) inode->handle);
		return error;
	}

	if (down.error < 0) {
		/* error performing operation for process */
		PERROR("ll_pvfs_fsync got error in downcall\n");
		return down.error;
	}
	
	return 0;
}


/* init_structs()
 *
 * Initialize the downcall and upcall structures.
 */
static void init_structs(struct pvfs_upcall *up,
struct pvfs_downcall *dp, pvfs_type_t type)
{
	memset(up, 0, sizeof(*up));
	memset(dp, 0, sizeof(*dp));
	up->magic = PVFS_UPCALL_MAGIC;
	up->type = type;
}

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */
