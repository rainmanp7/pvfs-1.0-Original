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

#ifndef _PVFS_LINUX_H
#define _PVFS_LINUX_H

/* pvfs_linux.h
 *
 * This file includes defines of common parameters, debugging print
 * macros, and some configuration values.
 *
 */
#include <config.h>

#include <linux/kernel.h>
#include <linux/posix_types.h>
#include <linux/types.h>
#include <linux/param.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/malloc.h>
#include <linux/wait.h>		
#include <linux/fs.h>

/* 2.4 */
#ifdef HAVE_LINUX_SCHED_H
#include <linux/sched.h>
#endif
#ifdef HAVE_LINUX_PAGEMAP_H
#include <linux/pagemap.h>
#endif

#include <ll_pvfs.h>

extern struct file_system_type pvfs_fs_type;

/* operations */
extern struct inode_operations pvfs_dir_inode_operations;
extern struct inode_operations pvfs_file_inode_operations;

extern struct file_operations pvfs_dir_operations;
extern struct file_operations pvfs_file_operations;

#ifdef HAVE_LINUX_STRUCT_ADDRESS_SPACE_OPERATIONS
/* new 2.4 structure */
extern struct address_space_operations pvfs_file_aops;
#endif

/* operations shared over more than one file */
#ifdef HAVE_LINUX_3_PARAM_FILE_FSYNC
int pvfs_fsync(struct file *file, struct dentry *dentry, int datasync);
#else
int pvfs_fsync(struct file *file, struct dentry *dentry);
#endif

int pvfs_release(struct inode *i, struct file *f);
int pvfs_permission(struct inode *inode, int mask);
int pvfs_revalidate_inode(struct dentry *);
int pvfs_meta_to_inode(struct pvfs_meta *mbuf, struct inode *ibuf);
int pvfs_rename(struct inode *old_inode, struct dentry *old_dentry, 
	struct inode *new_inode, struct dentry *new_dentry);
int pvfs_open(struct inode *i, struct file *f);

int pvfs_file_create(struct inode *dir, struct dentry *entry, int mode);

/* from inode.c */
int iattr_to_pvfs_meta(struct iattr *attr, struct pvfs_meta *meta);
int pvfs_notify_change(struct dentry *entry, struct iattr *iattr);

/* global variables */
extern int pvfs_debug;

enum {
	PVFS_SUPER_MAGIC = 0x1234abcd
};

/* simple functions to get to PVFS-specific info in superblocks and
 * inodes
 */

static inline struct pvfs_super *pvfs_sbp(struct super_block *sb)
{
	return sb->u.generic_sbp;
}

static inline struct pvfs_inode *pvfs_inop(struct inode *ino)
{
	return ino->u.generic_ip;
}

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */

#endif
