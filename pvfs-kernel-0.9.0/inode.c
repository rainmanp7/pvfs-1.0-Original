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


#define __NO_VERSION__

#include <config.h>
#include <pvfs_kernel_config.h>

#include <linux/locks.h>
#include <asm/uaccess.h>

#include <pvfs_linux.h>
#include <pvfs_mount.h>
#include <ll_pvfs.h>

/* VFS super_block ops */
static struct inode *pvfs_super_iget(struct super_block *sb, char *name);
static struct super_block *pvfs_read_super(struct super_block *, void *, int);
static void pvfs_read_inode(struct inode *);
static void pvfs_put_inode(struct inode *);
static void pvfs_delete_inode(struct inode *);
static void pvfs_put_super(struct super_block *);
#ifdef HAVE_LINUX_3_PARAM_SUPER_STATFS
/* Linux 2.2 has an additional parameter to the statfs member fn */
static int pvfs_statfs(struct super_block *sb, struct statfs *buf, 
		       int bufsize);
#else
static int pvfs_statfs(struct super_block *sb, struct statfs *buf);
#endif

/* exported operations */
#ifdef HAVE_LINUX_3_PARAM_SUPER_STATFS
/* Assuming new super_operations from our test of the statfs member
 * function prototype
 */
/* 2.2 version */
struct super_operations pvfs_super_operations =
{
	pvfs_read_inode,        /* read_inode */
	NULL,                   /* write_inode */
	pvfs_put_inode,	        /* put_inode */
	pvfs_delete_inode,      /* delete_inode */
	pvfs_notify_change,	/* notify_change */
	pvfs_put_super,	        /* put_super */
	NULL,			/* write_super */
	pvfs_statfs,   		/* statfs */
	NULL			/* remount_fs */
};
#else
/* 2.4 version */
struct super_operations pvfs_super_operations =
{
	read_inode:    pvfs_read_inode,
	put_inode:     pvfs_put_inode,
	delete_inode:  pvfs_delete_inode,
	/* moved notify_change to inode_operations setattr in 2.4 */
	put_super:     pvfs_put_super,
	statfs:        pvfs_statfs,
};
#endif

/* iattr_to_pvfs_meta()
 *
 * Doesn't work on size field right now.
 */
int iattr_to_pvfs_meta(struct iattr *attr, struct pvfs_meta *meta)
{
	PDEBUG(D_INODE, "iattr_to_pvfs_meta: valid = %x, flags = %x\n",
	attr->ia_valid, attr->ia_attr_flags);

	memset(meta, 0, sizeof(*meta));
	meta->mode = attr->ia_mode;
	meta->uid = attr->ia_uid;
	meta->gid = attr->ia_gid;
	meta->atime = attr->ia_atime;
	meta->mtime = attr->ia_mtime;
	meta->ctime = attr->ia_ctime;
	meta->valid = 0;

	/* map valid fields in attr to ones in meta */
	if (attr->ia_valid & ATTR_MODE) meta->valid |= V_MODE;
	if (attr->ia_valid & ATTR_UID) meta->valid |= V_UID;
	if (attr->ia_valid & ATTR_GID) meta->valid |= V_GID;

	/* TODO: MAKE SURE THIS WORKS OUT OK! 
	 * Might need to pull the non-valid values in the iattr out from the
	 * inode?  That would be a bit troublesome...
	 */
	if (attr->ia_valid & (ATTR_ATIME | ATTR_MTIME | ATTR_CTIME))
		meta->valid |= V_TIMES;

	return 0;
}

/* pvfs_notify_change()
 *
 * Calls ll_pvfs_setmeta() to set PVFS attributes, then calls
 * inode_setattr() to set the attributes in the local cache.
 *
 * NOTE: we don't ever request a size change here; we leave that to
 * truncate().
 */
int pvfs_notify_change(struct dentry *entry, struct iattr *iattr)
{
	int error = 0;
	struct pvfs_inode *pinode;
	struct pvfs_meta meta;
	unsigned int saved_valid;

	saved_valid = iattr->ia_valid;
	iattr->ia_valid &= ~ATTR_SIZE;

	if (entry->d_inode == NULL) {
		PDEBUG(D_INODE, "pvfs_notify_change called for %s (null inode)\n",
		entry->d_name.name);
		return -EINVAL;
	}
	pinode = pvfs_inop(entry->d_inode);

	PDEBUG(D_INODE, "pvfs_notify_change called for %s\n", pinode->name);

	/* fill in our metadata structure, call ll_pvfs_setmeta() */
	iattr_to_pvfs_meta(iattr, &meta);
	meta.handle = pinode->handle;
	if ((error = ll_pvfs_setmeta(pinode, &meta)) != 0) {
		PERROR("pvfs_notify_change failed\n");
		return error;
	}

	iattr->ia_valid = saved_valid;
	inode_setattr(entry->d_inode, iattr);

	return error;
}

static struct super_block * pvfs_read_super(struct super_block *sb, 
					    void *data, int silent)
{
	struct pvfs_super *pvfs_sbp = NULL;
	struct inode *root;
	struct pvfs_mount *mnt = (struct pvfs_mount *)data;

	PDEBUG(D_SUPER, "pvfs_read_super called.\n");

	MOD_INC_USE_COUNT;

	if (data == NULL) {
		if (!silent) PERROR("pvfs_read_super: no data parameter!\n");
		goto pvfs_read_super_abort;
	}

	if ((pvfs_sbp = (struct pvfs_super *) kmalloc(sizeof(struct
		pvfs_super), GFP_KERNEL)) == NULL)
	{
		if (!silent) PERROR("pvfs_read_super: kmalloc failed!\n");
		goto pvfs_read_super_abort;
	}

	/* data has already been copied in... */
	pvfs_sbp->flags = mnt->flags;
	pvfs_sbp->port = mnt->port;
	strncpy(pvfs_sbp->mgr, mnt->mgr, PVFSHOSTLEN);
	strncpy(pvfs_sbp->dir, mnt->dir, PVFSDIRLEN);

	/* modify the superblock */
#ifndef HAVE_LINUX_STRUCT_FILE_OPERATIONS_OWNER
	/* Locking in 2.4 for this function is handled at a higher level, so
	 * we only need it here for 2.2
	 *
	 * We don't have a good test for this necessity, so we are making an
	 * assumption based on another test...
	 */
	lock_super(sb);
#endif
	sb->s_magic = PVFS_SUPER_MAGIC;
	sb->s_blocksize = 4096;
	sb->s_blocksize_bits = 12;
	sb->s_op = &pvfs_super_operations;
	sb->u.generic_sbp = pvfs_sbp;

	/* set up root inode info */
	root = pvfs_super_iget(sb, pvfs_sbp->dir);
	if (root == NULL) goto pvfs_read_super_abort;

#ifdef HAVE_LINUX_STRUCT_FILE_OPERATIONS_OWNER
	sb->s_root = d_alloc_root(root);
#else
	sb->s_root = d_alloc_root(root, NULL);
#endif


#ifndef HAVE_LINUX_STRUCT_FILE_OPERATIONS_OWNER
	unlock_super(sb); /* NO UNLOCK IN 2.4 EITHER */
#endif

	PDEBUG(D_SUPER, "super addr = %lx, mode = %o, name = %s\n", (long) sb,
	root->i_mode, pvfs_inop(root)->name);
	return sb;

pvfs_read_super_abort:
	PERROR("pvfs_read_super aborting!\n");
	sb->s_dev = 0;
	if (pvfs_sbp != NULL) kfree(pvfs_sbp);
#ifndef HAVE_LINUX_STRUCT_FILE_OPERATIONS_OWNER
	unlock_super(sb); /* not in 2.4 */
#endif
	MOD_DEC_USE_COUNT;
	return NULL;
}

/* pvfs_put_super()
 *
 * Frees memory we allocated to store superblock information.
 */
static void pvfs_put_super(struct super_block *sb)
{
	PDEBUG(D_SUPER, "pvfs_put_super called.\n");

	kfree(sb->u.generic_sbp);

	MOD_DEC_USE_COUNT;
	return;
}

/* pvfs_read_inode()
 *
 * This function is called by iget().  However, we don't have the
 * information we need (ie. file name) from the parameters here, so
 * instead all the work that this function would normally do is done in
 * dir.c:pvfs_lookup().  The PVFS lookup function (in v2 anyway) returns
 * metadata, so this also happens to save us a call to the manager.
 */
static void pvfs_read_inode(struct inode *inode)
{
	PDEBUG(D_INODE, "pvfs_read_inode() called (does nothing).\n");
	return;
}

/* pvfs_put_inode()
 *
 * This is called from iput(), and really only helps with removing
 * inodes from the cache when they are no longer needed.  The code here
 * works because the i_count variable is just about to be decremented in
 * iput() (after this call returns).  By setting i_nlink to zero, we
 * cause the system to remove the inode from the cache, which results in
 * the call to pvfs_delete_inode().
 *
 * pvfs_delete_inode() frees the associated PVFS-specific data.
 */
static void pvfs_put_inode(struct inode *inode) 
{
#ifdef HAVE_LINUX_STRUCT_FILE_OPERATIONS_OWNER
	if (atomic_read(&inode->i_count) == 1) { /* NOTE: IS THIS OK FOR 2.2 ALSO? */
#else
	if (inode->i_count == 1) {
#endif
		inode->i_nlink = 0;
	}
	return;
}

/* pvfs_delete_inode()
 *
 * This doesn't really DELETE the inode; it is just a mechanism for
 * removing an inode from the inode cache.  It, for example, gets
 * called on all the inodes in the cache for files when a file system is
 * unmounted.
 *
 * Here is where we remove our PVFS-specific data stored in the inode.
 *
 */
static void pvfs_delete_inode(struct inode *inode)
{
	struct pvfs_inode *pinode;

	pinode = pvfs_inop(inode);

	kfree(pinode);
	inode->u.generic_ip = NULL;
	clear_inode(inode);
	return;
}

#ifdef HAVE_LINUX_3_PARAM_SUPER_STATFS
static int pvfs_statfs(struct super_block *sb, struct statfs *buf, 
int bufsize)
#else
static int pvfs_statfs(struct super_block *sb, struct statfs *buf)
#endif
{
	int error = 0;
	struct pvfs_statfs pbuf;
#ifdef HAVE_LINUX_3_PARAM_SUPER_STATFS
	struct statfs sbuf;

	memset(&sbuf, 0, sizeof(sbuf));
#endif

	PDEBUG(D_SUPER, "pvfs_statfs called.\n");


	if ((error = ll_pvfs_statfs(pvfs_sbp(sb), &pbuf)) != 0) {
		PERROR("pvfs_statfs failed\n");
		return error;
	}

#if 0
	sbuf.f_fsid = (fsid_t) 0;
#endif

#ifdef HAVE_LINUX_3_PARAM_SUPER_STATFS
	/* in 2.2 we put the values in a local structure and then
	 * copy_to_user() them into the user buffer.
	 */
	sbuf.f_type = PVFS_SUPER_MAGIC;
	sbuf.f_bsize = pbuf.bsize;
	sbuf.f_blocks = pbuf.blocks;
	sbuf.f_bfree = pbuf.bfree;
	sbuf.f_bavail = pbuf.bavail;
	sbuf.f_files = pbuf.files;
	sbuf.f_ffree = pbuf.ffree;
	sbuf.f_namelen = pbuf.namelen;

	error = copy_to_user(buf, &sbuf, bufsize);
#else
	/* in 2.4 we write straight into the buffer handed to us;
	 * this gets to the user at a higher level
	 */
	buf->f_type = PVFS_SUPER_MAGIC;
	buf->f_bsize = pbuf.bsize;
	buf->f_blocks = pbuf.blocks;
	buf->f_bfree = pbuf.bfree;
	buf->f_bavail = pbuf.bavail;
	buf->f_files = pbuf.files;
	buf->f_ffree = pbuf.ffree;
	buf->f_namelen = pbuf.namelen;
#endif

	return error;
}

/* init_pvfs: used by filesystems.c to register pvfs */

struct file_system_type pvfs_fs_type = {
   "pvfs", 0, pvfs_read_super, NULL
};

int init_pvfs_fs(void)
{
	return register_filesystem(&pvfs_fs_type);
}


/* pvfs_super_iget()
 *
 * Special routine for getting the inode for the superblock of a PVFS
 * file system.
 *
 * Returns NULL on failure, pointer to inode structure on success.
 */
static struct inode *pvfs_super_iget(struct super_block *sb, char *name)
{
	int len;
	struct pvfs_meta meta;
	struct pvfs_inode *pinode;
	struct inode *inode;

	/* big enough for <host>:port/<name> */
	char namebuf[PVFSHOSTLEN + PVFSDIRLEN + 8];

	/* put the full name together */
	sprintf(namebuf, "%s:%d%s", pvfs_sbp(sb)->mgr,
	pvfs_sbp(sb)->port, name);

	len = strlen(namebuf);

	PDEBUG(D_SUPER, "pvfs_super_iget: %s\n", namebuf);

	if (ll_pvfs_lookup(pvfs_sbp(sb), namebuf, len, &meta) < 0) {
		PERROR("pvfs_super_iget crapped out at ll_pvfs_lookup\n");
		return NULL;
	}

	if ((inode = iget(sb, meta.handle)) == NULL) {
		PERROR("pvfs_super_iget crapped out at iget\n");
		return NULL;
	}

	/* allocate and fill in the inode structure... */
	if ((pinode = (struct pvfs_inode *) kmalloc(sizeof(struct pvfs_inode)
	+ len + 1, GFP_KERNEL)) == NULL)
	{
		iput(inode); /* TODO: is this all we need to do? */
		return NULL;
	}
	
	pinode->handle = meta.handle;
	pvfs_meta_to_inode(&meta, inode);
	pinode->name = (int8_t *)pinode + sizeof(struct pvfs_inode);
	strcpy(pinode->name, namebuf);
	pinode->super = pvfs_sbp(sb);
	inode->u.generic_ip = pinode;

	PDEBUG(D_SUPER, "pvfs_super_iget: final name = %s\n",
	pvfs_inop(inode)->name);

	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &pvfs_dir_inode_operations;
#ifdef HAVE_LINUX_STRUCT_INODE_I_FOP
		inode->i_fop = &pvfs_dir_operations;
#endif
	}
   else if (S_ISREG(inode->i_mode)) {
		inode->i_op = &pvfs_file_inode_operations;
#ifdef HAVE_LINUX_STRUCT_INODE_I_FOP
		inode->i_fop = &pvfs_file_operations;
#endif
#ifdef HAVE_LINUX_STRUCT_ADDRESS_SPACE_OPERATIONS
		inode->i_data.a_ops = &pvfs_file_aops;
#endif
	}
   else {
		inode->i_op = NULL;
#ifdef HAVE_LINUX_STRUCT_INODE_I_FOP
		inode->i_fop = NULL;
#endif
	}

	return inode;
}

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */
