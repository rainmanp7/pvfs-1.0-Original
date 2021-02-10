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

#include <linux/mm.h>
#ifdef HAVE_LINUX_HIGHMEM_H
#include <linux/highmem.h>
#endif
#ifdef HAVE_LINUX_PAGEMAP_H
#include <linux/pagemap.h>
#endif
#ifdef HAVE_LINUX_LOCKS_H
#include <linux/locks.h>
#endif
#ifdef HAVE_LINUX_SMP_LOCK_H
#include <linux/smp_lock.h>
#endif
#include <asm/uaccess.h>



#include <pvfs_linux.h>
#include <ll_pvfs.h>

/* external variables */
extern int pvfs_maxsz;

/* file operations */
static int pvfs_readpage(struct file *file, struct page * page);
static ssize_t pvfs_file_read(struct file *file, char *buf, size_t count,
	loff_t *ppos);
static ssize_t pvfs_file_write(struct file *file, const char *buf,
	size_t count, loff_t *ppos);
static void pvfs_file_truncate(struct inode *);
static int pvfs_file_mmap(struct file *, struct vm_area_struct *);

#ifdef HAVE_LINUX_STRUCT_ADDRESS_SPACE_OPERATIONS
struct address_space_operations pvfs_file_aops = {
	readpage:      pvfs_readpage     /* readpage */
};
#endif

#ifdef HAVE_LINUX_STRUCT_FILE_OPERATIONS_OWNER
struct inode_operations pvfs_file_inode_operations = {
	create:     pvfs_file_create,      /* create */
	rename:     pvfs_rename,           /* rename */
	truncate:   pvfs_file_truncate,    /* truncate */
	revalidate: pvfs_revalidate_inode, /* revalidate */
	setattr:    pvfs_notify_change     /* setattr */
};
#else
struct inode_operations pvfs_file_inode_operations = {
	&pvfs_file_operations,	/* default file operations */
	pvfs_file_create,			/* create */
	NULL,		        /* lookup */
	NULL,			/* link */
	NULL,	        /* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* rmdir */
	NULL,			/* mknod */
	pvfs_rename,		        /* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
	pvfs_readpage,    	/* readpage */
	NULL,			/* writepage */
	NULL,			/* bmap */
	pvfs_file_truncate,		/* truncate */
	NULL,        /* permission */
	NULL,                   /* smap */
	NULL,                   /* update page */
	pvfs_revalidate_inode   /* revalidate */
};
#endif

/* file_operations notes:
 *
 * we're going to use mm/filemap.c:generic_file_mmap() here.  It will 
 * rely on our readpage function to work, so we'll have to get that
 * going.
 *
 */
#ifdef HAVE_LINUX_STRUCT_FILE_OPERATIONS_OWNER
struct file_operations pvfs_file_operations = {
	read:    pvfs_file_read,  /* read */
	write:   pvfs_file_write, /* write */
	mmap:    pvfs_file_mmap,  /* mmap - we'll try the default */
	open:    pvfs_open,       /* open called on first open instance of file */
	release: pvfs_release,    /* release called when last open instance closed */
	fsync:   pvfs_fsync       /* fsync */
};
#else
struct file_operations pvfs_file_operations = {
	NULL,		        /* lseek - default should work for pvfs */
	pvfs_file_read,         /* read */
	pvfs_file_write,        /* write */
	NULL,          		/* readdir */
	NULL,			/* select - default */
	NULL,		        /* ioctl */
	pvfs_file_mmap,         /* mmap - we'll try the default */
	pvfs_open,              /* open called on first open instance of a file */
	NULL,
	pvfs_release,           /* release called when last open instance closed */
	pvfs_fsync,		/* fsync */
	NULL,                   /* fasync */
	NULL,                   /* check_media_change */
	NULL,                   /* revalidate */
	NULL                    /* lock */
};
#endif

/* pvfs_file_truncate()
 *
 * This is called from fs/open.c:do_truncate() and is called AFTER
 * the notify_change.  The notify_change MUST use inode_setattr() or
 * some similar method to get the updated size into the inode, or we
 * won't have it to use.
 *
 * Perhaps notify_change() should be setting the inode values but isn't?
 */
static void pvfs_file_truncate(struct inode *inode)
{
	int error;
	struct pvfs_meta meta;

	PDEBUG(D_FILE, "pvfs_truncate called for %s, size = %Ld\n",
	pvfs_inop(inode)->name, (long long) inode->i_size);

	memset(&meta, 0, sizeof(meta));
	meta.valid = V_SIZE;
	meta.size = inode->i_size;

	if ((error = ll_pvfs_setmeta(pvfs_inop(inode), &meta)) != 0) {
		PERROR("pvfs_truncate failed\n");
		return;
	}

	/* if we were so inclined, we could update all our other values here,
	 * since the setmeta gives us updated values back.
	 */

	return;
}


/* pvfs_file_mmap()
 *
 * NOTES:
 * Tried setting the VM_WRITE flag in the vma to get through our tests
 * in pvfs_map_userbuf, but that didn't work.
 */
static int pvfs_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	int error = 0;

	PDEBUG(D_FILE, "pvfs_file_mmap called.\n");
	
	error = generic_file_mmap(file, vma);
	return error;
}


/* pvfs_readpage()
 *
 * Inside the page structure are "offset", the offset into the file, and
 * the page address, which can be found with "page_address(page)".
 *
 * See fs/nfs/read.c for readpage example.
 */
static int pvfs_readpage(struct file *file, struct page *page)
{
	int error = 0;
	struct inode *inode;
	char *buf;
	pvfs_off_t offset;
	size_t count = PAGE_SIZE;

	/* from generic_readpage() */
	atomic_inc(&page->count);
	set_bit(PG_locked, &page->flags);
#if 0
	/* NOTE: THIS WAS COMMENTED OUT IN 2.4 CODE; I JUST WENT AHEAD AND
	 * COMMENTED IT OUT HERE TOO...
	 */
	set_bit(PG_free_after, &page->flags); /* not necessary ??? */
#endif
	
	/* from brw_page() */
	clear_bit(PG_uptodate, &page->flags);
	clear_bit(PG_error, &page->flags);

#ifdef HAVE_KMAP
	/* this should help readpage work correctly for big mem machines */
	buf = (char *)kmap(page);
#else
	buf = (char *)page_address(page);
#endif

#ifdef HAVE_LINUX_STRUCT_PAGE_OFFSET
	/* THIS WORKED FOR 2.2.15 and before... */
	offset = page->offset;
#else
	/* THIS HOPEFULLY WILL WORK NOW (IT'S WHAT PHIL HAD IN 2.4) */
	offset = ((loff_t)page->index) << PAGE_CACHE_SHIFT;
#if 0
	/* THIS IS WHAT I HAD BEFORE */
	offset = pgoff2loff(page->index);
#endif
#endif

	inode = file->f_dentry->d_inode;

	memset(buf, 0, count);

	PDEBUG(D_FILE, "pvfs_readpage called for %s (%ld), offset %ld, size %ld\n",
	pvfs_inop(inode)->name, (unsigned long) pvfs_inop(inode)->handle,
	(long) offset, (long) count);

	error = ll_pvfs_file_read(pvfs_inop(inode), buf, count, &offset, 1);
	if (error <= 0) goto pvfs_readpage_error;

	/* from brw_page() */
	set_bit(PG_uptodate, &page->flags);
pvfs_readpage_error:
#ifdef HAVE_KMAP
	kunmap(page);
#endif
#ifdef HAVE_UNLOCKPAGE
	/* this supposedly helps prevent smp races in 2.4 */
	UnlockPage(page);
#else
	/* from brw_page() */
	clear_bit(PG_locked, &page->flags);
	wake_up(&page->wait);
#endif
#if 0
	free_page(page_address(page)); /* NFS does this */
#endif
	__free_page(page); /* after_unlock_page() does this */

	return error;
}


/* pvfs_file_read(file, buf, count, ppos)
 *
 * Notes:
 * - generally the pointer to the position passed in is a pointer to
 * 	file->f_pos, so that doesn't need to be updated as well.
 *
 * TODO: Combine with pvfs_file_write() to save space.
 */
static ssize_t pvfs_file_read(struct file *file, char *cbuf, size_t count,
	loff_t *ppos)
{
	int error = 0, retsz = 0;
	struct inode *inode;
	pvfs_off_t pvfs_pos = *ppos;
	size_t xfersize;
	char *buf = (char *) cbuf;

	/* should we error check here? do we know for sure that the dentry is
	 * there?
	 */
	inode = file->f_dentry->d_inode;

	PDEBUG(D_FILE, "pvfs_file_read called for %s (%ld), offset %ld, size %ld\n",
	pvfs_inop(inode)->name, (unsigned long) pvfs_inop(inode)->handle,
	(long) pvfs_pos, (long) count);

	if (access_ok(VERIFY_WRITE, buf, count) == 0) return -EFAULT;

	/* split our operation into blocks of pvfs_maxsz or smaller */
	do {
		xfersize = (count < pvfs_maxsz) ? count : pvfs_maxsz;

		error = ll_pvfs_file_read(pvfs_inop(inode), buf, xfersize, &pvfs_pos, 0);
		if (error <= 0) return error;
		retsz += error;

		/* position is updated by ll_pvfs_file_read() */
		count -= xfersize;
		buf += xfersize;
	} while (count > 0);

	*ppos = pvfs_pos;

	return retsz;
}

/* pvfs_file_write(file, buf, count, ppos)
 *
 * Notes:
 * - generally the pointer to the position passed in is a pointer to
 * 	file->f_pos, so that doesn't need to be updated as well.
 */
static ssize_t pvfs_file_write(struct file *file, const char *cbuf,
size_t count, loff_t *ppos)
{
	int error = 0, retsz = 0;
	struct inode *inode;
	pvfs_off_t pvfs_pos = *ppos;
	size_t xfersize;
	char *buf = (char *) cbuf;

	/* should we error check here? do we know for sure that the dentry is
	 * there?
	 */
	inode = file->f_dentry->d_inode;

	PDEBUG(D_FILE, "pvfs_file_write called for %s (%ld), offset %ld, size %ld\n",
	pvfs_inop(inode)->name, (unsigned long) pvfs_inop(inode)->handle,
	(long) pvfs_pos, (long) count);


	if (access_ok(VERIFY_READ, (char *)buf, count) == 0) return -EFAULT;

	/* split our operation into blocks of pvfs_maxsz or smaller */
	do {
		xfersize = (count < pvfs_maxsz) ? count : pvfs_maxsz;

		error = ll_pvfs_file_write(pvfs_inop(inode), buf, xfersize, &pvfs_pos, 0);
		if (error <= 0) return error;
		retsz += error;

		/* position is updated by ll_pvfs_file_write() */
		count -= xfersize;
		buf += xfersize;
	} while (count > 0);

	*ppos = pvfs_pos;
	if (pvfs_pos > inode->i_size)
		inode->i_size = pvfs_pos;

	return retsz;
}

/* pvfs_release()
 *
 * Called when the last open file reference for a given file is
 * finished.  We use this as an opportunity to tell the daemon to close
 * the file when it wants to.
 */
int pvfs_release(struct inode *inode, struct file *f)
{
	int error = 0;
	struct pvfs_inode *pinode;

	pinode = pvfs_inop(inode);

	PDEBUG(D_FILE, "pvfs_release called for %s (%ld)\n", pinode->name,
	(unsigned long) pinode->handle);

	error = ll_pvfs_hint(pinode, HINT_CLOSE);

	return error;
}


/* pvfs_open()
 *
 * Called from fs/open.c:filep_open()
 *
 * NOTES:
 * Truncation and creation are handled by fs/namei.c:open_namei()
 * We need to take care of O_APPEND here.
 *
 * The inode for the file is not necessarily up to date.  This is
 * important in the O_APPEND case because we need to have the most
 * recent size, so we're going to force a revalidate_inode() in the case
 * of an append.
 */
int pvfs_open(struct inode *inode, struct file *file)
{
	int error = 0;

#ifdef NEED_TO_LOCK_KERNEL
	lock_kernel();
#endif

	if ((file->f_flags & O_APPEND)) {
		/* force a revalidate */
		PDEBUG(D_FILE, "pvfs_open: getting most up to date size\n");

		if ((error = pvfs_revalidate_inode(file->f_dentry)) < 0) {
#ifdef NEED_TO_LOCK_KERNEL
			unlock_kernel();
#endif
			return error;
		}

		/* set the file position to point to one byte past the last byte
		 * in the file
		 */
		file->f_pos = inode->i_size;
	}
#ifdef NEED_TO_LOCK_KERNEL
	unlock_kernel();
#endif
	return error;
}

/* pvfs_file_create()
 */
int pvfs_file_create(struct inode *dir, struct dentry *entry, int mode)
{
	int error = 0, len_dir, len_file;
	struct inode *inode;
	struct pvfs_meta meta;
	struct pvfs_phys phys;
	struct pvfs_inode *pinode;
	char *ptr;

	PDEBUG(D_FILE, "pvfs_file_create called for %ld\n", dir->i_ino);

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

	phys.blksize = DEFAULT_BLKSIZE;
	phys.dist = DEFAULT_DIST;
	phys.nodect = DEFAULT_NODECT;
	
	PDEBUG(D_FILE, "pvfs_file_create calling ll_pvfs_create\n");
	if ((error = ll_pvfs_create(pvfs_sbp(dir->i_sb), pinode->name,
	len_dir + len_file + 1, &meta, &phys)) < 0)
	{
		kfree(pinode);
		return error;
	}

	/* do a lookup so we can fill in the inode */
	PDEBUG(D_FILE, "pvfs_file_create calling ll_pvfs_lookup\n");
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

	PDEBUG(D_FILE, "pvfs_file_create: saved name is %s\n",
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

	return 0;
}			     

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */
