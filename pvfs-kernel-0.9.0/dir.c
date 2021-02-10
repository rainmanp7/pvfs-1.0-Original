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
#include <pvfs_linux.h>
#include <ll_pvfs.h>

#ifdef HAVE_LINUX_LOCKS_H
#include <linux/locks.h>
#endif
#ifdef HAVE_LINUX_SMP_LOCK_H
#include <linux/smp_lock.h>
#endif

/* dentry ops */
static int pvfs_dentry_revalidate(struct dentry *de, int);

/* dir inode-ops */
static struct dentry *pvfs_lookup(struct inode *dir, struct dentry *target);
static int pvfs_unlink(struct inode *dir_inode, struct dentry *entry);
static int pvfs_mkdir(struct inode *dir_inode, struct dentry *entry, int mode);
static int pvfs_rmdir(struct inode *dir_inode, struct dentry *entry);

/* dir file-ops */
static int pvfs_readdir(struct file *file, void *dirent, filldir_t filldir);
static ssize_t pvfs_dir_read(struct file *file, char *buf, size_t count,
	loff_t *ppos);

#ifdef HAVE_LINUX_STRUCT_FILE_OPERATIONS_OWNER
/* USING THIS IDENTIFIER TO TELL US THE NEW STYLE FORMAT IS OK */
struct dentry_operations pvfs_dentry_operations =
{
	d_revalidate:     pvfs_dentry_revalidate
};
#else
/* NOT CURRENTLY USED ANYWHERE? */
struct dentry_operations pvfs_dentry_operations =
{
	pvfs_dentry_revalidate, /* revalidate */
	NULL, /* hash */
	NULL, /* compare */
	NULL /* delete */
};
#endif


#ifdef HAVE_LINUX_STRUCT_FILE_OPERATIONS_OWNER
/* Assuming new dir_inode_operations based on file_operations check */
struct inode_operations pvfs_dir_inode_operations =
{
	/* put this in right later... pvfs_notify_change */
	/* &pvfs_dir_operations, */
	create: pvfs_file_create,    /* create TODO: DOES THIS NEED TO BE HERE? */
	lookup: pvfs_lookup,      /* lookup */
	unlink: pvfs_unlink, /* unlink called on parent directory to unlink a file */
	mkdir: pvfs_mkdir,           /* mkdir */
	rmdir: pvfs_rmdir,              /* rmdir */
	rename: pvfs_rename,         /* rename */
	revalidate: pvfs_revalidate_inode,   /* revalidate */
	setattr: pvfs_notify_change
};
#else
struct inode_operations pvfs_dir_inode_operations =
{
	&pvfs_dir_operations,
	pvfs_file_create,	        /* create TODO: DOES THIS NEED TO BE HERE? */
	pvfs_lookup,	        /* lookup */
	NULL,	        /* link */
	pvfs_unlink,   /* unlink called on parent directory to unlink a file */
	NULL,	        /* symlink */
	pvfs_mkdir,	        /* mkdir */
	pvfs_rmdir,   	        /* rmdir */
	NULL,	        /* mknod */
	pvfs_rename,	        /* rename */
	NULL,	                /* readlink */
	NULL,	                /* follow_link */
	NULL,	                /* readpage */
	NULL,		        /* writepage */
	NULL,		        /* bmap */
	NULL,	                /* truncate */
	NULL,                   /* permission, replaces fs/namei.c:permission() */
	NULL,                   /* smap */
	NULL,                   /* update page */
	pvfs_revalidate_inode   /* revalidate */
};
#endif

#ifdef HAVE_LINUX_STRUCT_FILE_OPERATIONS_OWNER
struct file_operations pvfs_dir_operations = {
	read: pvfs_dir_read,   /* read -- returns -EISDIR  */
	readdir: pvfs_readdir  /* readdir */
};
#else
struct file_operations pvfs_dir_operations = {
	NULL,                   /* lseek */
	pvfs_dir_read,          /* read -- returns -EISDIR  */
	NULL,                   /* write -- bad */
	pvfs_readdir,           /* readdir */
	NULL,                   /* select */
	NULL,                   /* ioctl */
	NULL,                   /* mmap */
	NULL,                   /* open */
	NULL,
	NULL,           /* release */
	NULL,             /* fsync */
	NULL,                   
	NULL,
	NULL
};
#endif

/* inode operations for directories */

/* pvfs_lookup(dir, entry)
 *
 * Notes:
 * - name to look for is in the entry (entry->d_name.name, entry->d_name.len)
 * - dir is the directory in which we are doing the lookup
 *
 * Behavior:
 * - call ll_pvfs_lookup to get a handle
 * - grab an inode for the new handle
 * - fill in the inode metadata
 * - add the dentry/inode pair into the dcache
 * - (optionally) set up pointer to new dentry functions
 *
 * Returns NULL pretty much all the time.  I know, this seems really
 * screwed up, but the ext2 fs seems to be doing the same thing...
 */
static struct dentry *pvfs_lookup(struct inode *dir, struct dentry *entry)
{
	int error = 0, len_dir, len_file;
	struct pvfs_inode *pinode;
	struct pvfs_meta meta;
	struct inode *inode = NULL;
	char *ptr;

	PDEBUG(D_DIR, "pvfs_lookup called on %ld for %s\n", dir->i_ino,
	entry->d_name.name);
	
	PDEBUG(D_DIR, "name might be %s/%s\n", pvfs_inop(dir)->name,
	entry->d_name.name);

	len_dir = strlen(pvfs_inop(dir)->name);
	len_file = entry->d_name.len;
	if ((pinode = (struct pvfs_inode *) kmalloc(sizeof(struct pvfs_inode)
	+ len_dir + len_file + 2, GFP_KERNEL)) == NULL)
		return NULL;

	/* fill in pvfs_inode name field first */
	pinode->name = (int8_t *)pinode + sizeof(struct pvfs_inode);
	ptr = pinode->name;
	strcpy(ptr, pvfs_inop(dir)->name);
	ptr += len_dir;
	*ptr = '/';
	ptr++;
	strcpy(ptr, entry->d_name.name);

	/* do the lookup, grabs metadata */
	error = ll_pvfs_lookup(pvfs_sbp(dir->i_sb), pinode->name,
		len_dir + len_file + 1, &meta);
	if (error < 0 && error != -ENOENT) {
		/* real error */
		d_drop(entry);
		kfree(pinode);
		return NULL;
	}

	if (error == -ENOENT) {
		/* no file found; need to insert NULL inode so that create calls
		 * can work correctly (or so says Gooch)
		 */
		entry->d_time = 0;
		entry->d_op = &pvfs_dentry_operations;
		d_add(entry, NULL);
		kfree(pinode);
		return NULL;
	}

	if ((inode = iget(dir->i_sb, meta.handle)) == NULL) {
		/* let's drop the dentry here... */
		d_drop(entry);
		kfree(pinode);
		return NULL;
	}

	/* fill in inode structure, remainder of pvfs_inode */
	pinode->handle = inode->i_ino;
	pinode->super = pvfs_sbp(inode->i_sb);
	pvfs_meta_to_inode(&meta, inode);
	inode->u.generic_ip = pinode;

	entry->d_time = 0;
	entry->d_op = &pvfs_dentry_operations;
	d_add(entry, inode);

	PDEBUG(D_DIR, "saved name is %s\n", pvfs_inop(inode)->name);

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

	return NULL;
}


/* pvfs_mkdir()
 *
 * Called on parent directory with dentry of directory to create.
 *
 * This is pretty much a carbon copy of pvfs_file_create().
 */
static int pvfs_mkdir(struct inode *dir, struct dentry *entry, int mode)
{
	int error = 0, len_dir, len_file;
	struct inode *inode;
	struct pvfs_meta meta;
	struct pvfs_inode *pinode;
	char *ptr;

	PDEBUG(D_DIR, "pvfs_mkdir called on %ld for %s\n", dir->i_ino,
	entry->d_name.name);

	len_dir = strlen(pvfs_inop(dir)->name);
	len_file = entry->d_name.len;
	if ((pinode = (struct pvfs_inode *) kmalloc(sizeof(struct pvfs_inode)
	+ len_dir + len_file + 2, GFP_KERNEL)) == NULL)
		return -ENOMEM;

	/* build pvfs_inode name field first */
	pinode->name = (int8_t *)pinode + sizeof(struct pvfs_inode);
	ptr = pinode->name;
	strcpy(ptr, pvfs_inop(dir)->name);
	ptr += len_dir;
	*ptr = '/';
	ptr++;
	strcpy(ptr, entry->d_name.name);

	/* do the create */
	meta.valid = V_MODE | V_UID | V_GID | V_TIMES;
	meta.uid = current->fsuid;
	meta.gid = current->fsgid;
	meta.mode = mode;
	meta.mtime = meta.atime = meta.ctime = CURRENT_TIME;

	PDEBUG(D_DIR, "pvfs_mkdir calling ll_pvfs_mkdir\n");
	if ((error = ll_pvfs_mkdir(pvfs_sbp(dir->i_sb), pinode->name,
	len_dir + len_file + 1, &meta )) < 0)
	{
		kfree(pinode);
		return error;
	}

	/* do a lookup so we can fill in the inode */
	PDEBUG(D_DIR, "pvfs_mkdir calling ll_pvfs_lookup\n");
	if ((error = ll_pvfs_lookup(pvfs_sbp(dir->i_sb), pinode->name,
	len_dir + len_file + 1, &meta)) < 0)
	{
		kfree(pinode);
		return error;
	}

	/* fill in inode structure and remainder of pvfs_inode */
	if ((inode = iget(dir->i_sb, meta.handle)) == NULL) {
		kfree(pinode);
		return -ENOMEM;
	}

	pinode->handle = inode->i_ino;
	pinode->super = pvfs_sbp(inode->i_sb);
	pvfs_meta_to_inode(&meta, inode);
	inode->u.generic_ip = pinode;

	d_instantiate(entry, inode); /* do I know what this does?  nope! */

	PDEBUG(D_DIR, "saved name is %s\n", pvfs_inop(inode)->name);

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

	return error;
}

/* pvfs_unlink(dir, entry)
 *
 * Called on parent directory with dentry of file to unlink.  We call
 * d_delete(entry) when done to take the dentry out of the cache.  This
 * call also frees the inode associated with the unlinked file.
 * 
 */
int pvfs_unlink(struct inode *dir, struct dentry *entry)
{
	int error = 0;

	PDEBUG(D_DIR, "pvfs_unlink called on %ld for %s\n", dir->i_ino,
	entry->d_name.name);
	if (entry->d_inode == NULL) {
		/* negative dentry ?!? */
		PERROR("pvfs_unlink: negative dentry?\n");
		/* TODO: should look up the file? */
		return -ENOENT;
	}
	if (entry->d_inode->i_nlink == 0) {
		PERROR("pvfs_unlink: deleting nonexistent file?\n");
	}
#if 0
	/* do we really need to mark the directory inode as dirty? */
	mark_inode_dirty(dir);
#endif
	error = ll_pvfs_unlink(pvfs_inop(entry->d_inode));

	if (error == 0) d_delete(entry); /* this also frees the inode */
	return error;
}


/* pvfs_rmdir()
 *
 * Called on parent directory with dentry of directory to remove.
 *
 * TODO: I'm not so sure that I know everything this call should be
 * doing; look at minix/namei.c:minix_rmdir() for some possible ideas.
 */
int pvfs_rmdir(struct inode *dir, struct dentry *entry)
{
	int error = 0;

	PDEBUG(D_DIR, "pvfs_rmdir called on %ld for %s\n", dir->i_ino,
	entry->d_name.name);
	if (entry->d_inode == NULL) {
		/* negative dentry ?!? */
		PERROR("pvfs_rmdir: negative dentry?\n");
		/* TODO: should look up the file? */
		return -ENOENT;
	}
	if (entry->d_inode->i_nlink == 0) {
		PERROR("pvfs_rmdir: deleting nonexistent directory?\n");
	}
#if 0
	/* do we really need to mark the directory inode as dirty? */
	mark_inode_dirty(dir);
#endif
	error = ll_pvfs_rmdir(pvfs_inop(entry->d_inode));

	if (error == 0) d_delete(entry); /* this also frees the inode */
	return error;
}


/* pvfs_rename(old_dir, old_dentry, new_dir, new_dentry)
 *
 * PARAMETERS:
 * old_dir - inode of parent directory of file/dir to be moved
 * new_dir - inode of parent directory of destination
 * old_dentry - dentry referring to the file/dir to move
 * new_dentry - dentry referring to the destination; inode might or
 *   might not be available.
 *
 * NOTES:
 * If the target name already exists, there will be a valid inode in the
 * new_dentry.  If the inode pointer is NULL, then there is nothing
 * referred to by that name.
 *
 * Also need to update the dcache and such to take changes into account!
 *
 * see fs/nfs/dir.c:nfs_rename() for an example.
 * see fs/minix/namei.c:minix_rename() for a different example.
 *
 * It's not enough to just mark the various inodes as dirty.  We need to
 * ensure that the directory cache is updated.  This turns out to be
 * easy; just call d_move(old_dentry, new_dentry) to get the dentries
 * updated!  Or at least, that seems to be working <smile>...
 *
 * Returns 0 on success, -errno on failure.
 */
int pvfs_rename(struct inode *old_dir, struct dentry *old_dentry, 
struct inode *new_dir, struct dentry *new_dentry)
{
	int error = -ENOSYS;
	struct inode *new_inode;
	struct pvfs_inode *old_pinode, *new_pinode;

	old_pinode = pvfs_inop(old_dentry->d_inode);
	new_inode = new_dentry->d_inode;
	if (new_inode == NULL) {
		/* no file at new name; need to create a fake pvfs_inode */
		/* namebuf is big enough for <host>:port/<name> */
		char namebuf[PVFSHOSTLEN + PVFSDIRLEN + 7];
		struct pvfs_inode fake_pinode;

		PDEBUG((D_DIR | D_FILE), "pvfs_rename, no inode for new file\n");
		if ((strlen(old_pinode->name) + new_dentry->d_name.len + 2) >
		PVFSHOSTLEN + PVFSDIRLEN + 7) {
			/* should never happen */
			PERROR("pvfs_rename: string too long?!?\n");
			return -ENOSYS;
		}
		sprintf(namebuf, "%s/%s", pvfs_inop(new_dir)->name,
		new_dentry->d_name.name);
		fake_pinode.handle = 0;
		fake_pinode.name = namebuf;
		fake_pinode.super = pvfs_sbp(new_dir->i_sb);

		PDEBUG((D_DIR | D_FILE), "pvfs_rename called, %s -> %s (new)\n",
		old_pinode->name, namebuf);

		error = ll_pvfs_rename(old_pinode, &fake_pinode);
	}
	else {
		/* a file already exists with new name */
		new_pinode = pvfs_inop(new_dentry->d_inode);
		PDEBUG((D_DIR | D_FILE), "pvfs_rename called, %s -> %s\n",
		old_pinode->name, new_pinode->name);

		error = ll_pvfs_rename(old_pinode, new_pinode);
	}
	
	/* Update the dcache */
	d_move(old_dentry, new_dentry);

	return error;
}


/* pvfs_readdir()
 *
 */
int pvfs_readdir(struct file *file, void *dirent, filldir_t filldir)
{
	int error = 0, len;
	struct pvfs_dirent dbuf;
	struct inode *inode;
	pvfs_off_t pos;

	inode = file->f_dentry->d_inode;

	PDEBUG(D_DIR, "pvfs_readdir called for %ld.\n", inode->i_ino);

	pos = file->f_pos;
	error = ll_pvfs_readdir(pvfs_inop(inode), &dbuf, &pos);
	if (error) return error;
	if (dbuf.len == 0) return 0;

	len = strlen(dbuf.name);
#ifdef HAVE_LINUX_6_PARAM_FILLDIR
	error = filldir(dirent, dbuf.name, len, dbuf.off, dbuf.handle, DT_DIR);
#else
	/* 2.2 */
	error = filldir(dirent, dbuf.name, len, dbuf.off, dbuf.handle);
#endif
	if (error) return error;

	/* ll_pvfs_readdir gives us the position to start from the next time */
	file->f_pos = pos;

	return error;
}

/* pvfs_fsync(file, dentry, [datasync])
 *
 */
#ifdef HAVE_LINUX_3_PARAM_FILE_FSYNC
int pvfs_fsync(struct file *file, struct dentry *dentry, int datasync)
#else
int pvfs_fsync(struct file *file, struct dentry *dentry)
#endif
{
	int error = 0;
	struct pvfs_inode *pinode;

#ifdef NEED_TO_LOCK_KERNEL
	lock_kernel();
#endif
	pinode = pvfs_inop(dentry->d_inode);

	error = ll_pvfs_fsync(pinode);
#ifdef NEED_TO_LOCK_KERNEL
	unlock_kernel();
#endif

	return error;
}

/* pvfs_dir_read()
 *
 * Returns -EISDIR.  Just here to get the right error value back.
 */
static ssize_t pvfs_dir_read(struct file *file, char *buf, size_t count,
loff_t *ppos)
{
	return -EISDIR;
}

/* pvfs_revalidate_inode()
 *
 * Performs a getmeta operation to ensure that the data in the inode is
 * up-to-date.
 */
int pvfs_revalidate_inode(struct dentry *dentry)
{
	int error = 0;
	struct inode *inode;
	struct pvfs_meta meta;
	struct pvfs_phys phys;

#ifdef NEED_TO_LOCK_KERNEL
	lock_kernel();
#endif

	inode = dentry->d_inode;
	if (inode == NULL) {
		PERROR("pvfs_revalidate_inode called for NULL inode\n");
#ifdef NEED_TO_LOCK_KERNEL
		unlock_kernel();
#endif
		return -EINVAL;
	}

	PDEBUG(D_DIR, "pvfs_revalidate_inode called for %s (%ld).\n",
	pvfs_inop(inode)->name, (unsigned long) pvfs_inop(inode)->handle);

	error = ll_pvfs_getmeta(pvfs_inop(inode), &meta, &phys);
	if (error < 0) {
		PDEBUG(D_DIR,
		"pvfs_revalidate: ll_pvfs_getmeta() failed; updating dcache\n");

		/* if the getmeta fails, we need to update the dcache to reflect
		 * the change.
		 */
		d_delete(dentry);
#ifdef NEED_TO_LOCK_KERNEL
		unlock_kernel();
#endif
		return error;
	}

	/* TODO: DO I NEED TO LOCK THE INODE BEFORE MODIFYING?!? */
	/* NOTE: not doing anything with the phys for now */
	pvfs_meta_to_inode(&meta, inode);

#ifdef NEED_TO_LOCK_KERNEL
	unlock_kernel();
#endif
	return error;
}


/* called when a cache lookup succeeds, if we were using it... */
static int pvfs_dentry_revalidate(struct dentry *dentry, int flags)
{
	struct inode *inode = dentry->d_inode;

	PDEBUG(D_DIR, "pvfs_dentry_revalidate called for %s.\n",
	dentry->d_name.name);
#ifdef NEED_TO_LOCK_KERNEL
	lock_kernel();
#endif

	if (inode == NULL){
		PDEBUG(D_DIR, "pvfs_dentry_revalidate found NULL inode.\n");
		goto dentry_bad;
	}

	if (is_bad_inode(inode)){
		PDEBUG(D_DIR, "pvfs_dentry_revalidate found bad inode.\n");
		goto dentry_bad;
	}

	/* the return values follow the lead of the nfs_lookup_revalidate
	 * function in the 2.2.x kernel.  NFS also does some dcache
	 * management in this function but I will leave that alone for now :) */

#ifdef NEED_TO_LOCK_KERNEL
	unlock_kernel();
#endif
	return 1; /* everything is ok */

dentry_bad:
#ifdef NEED_TO_LOCK_KERNEL
	unlock_kernel();
#endif
	return 0; /* bad; needs to be revalidated */
}


/* pvfs_meta_to_inode()
 *
 * Copies a pvfs_meta structure's values into an inode structure.
 */
int pvfs_meta_to_inode(struct pvfs_meta *mbuf, struct inode *ibuf)
{
	int error = 0;

	/* leaving the handle/i_ino fields alone for now */

	ibuf->i_mode = mbuf->mode;
	ibuf->i_uid = mbuf->uid;
	ibuf->i_gid = mbuf->gid;
	ibuf->i_size = mbuf->size;
	ibuf->i_atime = mbuf->atime;
	ibuf->i_mtime = mbuf->mtime;
	ibuf->i_ctime = mbuf->ctime;
	ibuf->i_blksize = mbuf->blksize;
	ibuf->i_blocks = mbuf->blocks;

	return error;
}

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */
