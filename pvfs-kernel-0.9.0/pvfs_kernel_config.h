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

#ifndef __PVFS_KERNEL_CONFIG_H
#define __PVFS_KERNEL_CONFIG_H

/* Kernel configuration issues */
#ifdef __KERNEL__

/* Some of these conditional statements were borrowed from Don Becker's
 * 3c509.c module code.
 */
#include <linux/config.h>
#if defined(CONFIG_SMP) && ! defined(__SMP__)
#define __SMP__
#endif
#if defined(CONFIG_MODVERSIONS) && ! defined(MODVERSIONS)
#define MODVERSIONS
#endif

#include <linux/version.h>
#include <linux/module.h>
#if LINUX_VERSION_CODE < 0x20500 && defined(MODVERSIONS)
#include <linux/modversions.h>
#endif

#endif /* __KERNEL__ */

#include <config.h>

/* Decide if we are going to be able to support mapping user buffers
 * into kernel space or not.  If the old PVFS kernel patch is there or
 * the kiovec symbols are there, then we will be able to support this.
 */
#if defined(HAVE_KIOVEC_SYMS) || defined(HAVE_PVFS_KERNEL_PATCH)
#define HAVE_PVFS_BUFMAP 1
#else
#undef HAVE_PVFS_BUFMAP
#endif

/* Decide if we need to call lock_kernel() and unlock_kernel() in our
 * code.  Right now I do it with a cheezy version code check.  Not the
 * best.
 */
#if LINUX_VERSION_CODE > 0x202ff
#define NEED_TO_LOCK_KERNEL
#else
#undef NEED_TO_LOCK_KERNEL
#endif

/* DEBUGGING VALUES */
enum {
	D_SUPER = 1,  /* superblock calls */
	D_INODE = 2,  /* inode calls */
	D_FILE = 4,  /* file calls */
	D_DIR = 8,  /* directory calls */
	D_LLEV = 16, /* low level calls */
	D_LIB = 32,
	D_UPCALL = 64,
	D_PSDEV = 128,
	D_PIOCTL = 256,
	D_SPECIAL = 512,
	D_TIMING = 1024,
	D_DOWNCALL = 2048
};
 
#ifdef __KERNEL__
#define PDEBUG(mask, format, a...)                                \
  do {                                                            \
    if (pvfs_debug & mask) {                                      \
      printk("(%s, %d): ",  __FILE__, __LINE__);                  \
      printk(format, ## a);                                       \
	 }                                                             \
  } while (0) ;
#define PERROR(format, a...)                                      \
  do {                                                            \
    printk("(%s, %d): ",  __FILE__, __LINE__);                    \
    printk(format, ## a);                                         \
  } while (0) ;

#else
#define PDEBUG(mask, format, a...)                                \
  do {                                                            \
    if (pvfs_debug & mask) {                                      \
      printf("(%s, %d): ",  __FILE__, __LINE__);                  \
      printf(format, ## a);                                       \
    }                                                             \
  } while (0) ;
#define PERROR(format, a...)                                      \
  do {                                                            \
    printf("(%s, %d): ",  __FILE__, __LINE__);                    \
    printf(format, ## a);                                         \
	 fflush(stdout);                                               \
  } while (0) ;
#endif

/* Buffer allocation values:
 * PVFS_BUFFER_STATIC - allocates a static buffer and holds it for the
 *   duration the module is loaded
 * PVFS_BUFFER_DYNAMIC - allocates a memory region in the kernel for the
 *   duration of each request
 * PVFS_BUFFER_MAPPED - maps the user's buffer into kernel space,
 *   eliminating additional memory use and a copy
 */
enum {
	PVFS_BUFFER_STATIC = 1,
	PVFS_BUFFER_DYNAMIC = 2,
	PVFS_BUFFER_MAPPED = 3
};

/* Values:
 * PVFS_DEFAULT_IO_SIZE - default buffer size to service at one time.  This
 *   is used to prevent very large transfers from grabbing all available
 *   physical memory.
 * PVFS_DEFAULT_BUFFER - default buffering technique in the kernel.
 * PVFS_OPT_IO_SIZE - value returned on statfs; determines the transfer
 *   size used by many common utilities?  Well, maybe not...we still
 *   seem to be getting 64K transfers.
 * PVFS_DEFAULT_DEBUG_MASK - used in pvfs_mod.c and pvfsd.c as the
 *   default mask for debugging.  This can be changed for the module with
 *   the "debug" parameter on insmod.
 * PVFSD_NOFILE - not currently used; will eventually be the number of
 *   file descriptors the pvfsd allows itself to use
 * PVFSD_MAJOR - major number for pvfsd device file
 */
enum {
	PVFS_DEFAULT_IO_SIZE = 16*1024*1024,
	PVFS_MIN_IO_SIZE = 64*1024,
	PVFS_DEFAULT_BUFFER = PVFS_BUFFER_DYNAMIC,
	PVFS_OPT_IO_SIZE = 128*1024, 
	PVFS_DEFAULT_DEBUG_MASK = 0,
	PVFSD_NOFILE = 65536,
	PVFSD_MAJOR = 60 /* experimental number */
};

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */

#endif
