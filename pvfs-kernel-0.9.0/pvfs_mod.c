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

#include <config.h>
#include <pvfs_kernel_config.h>
#include <pvfs_linux.h>

MODULE_AUTHOR("Rob Ross <rbross@parl.clemson.edu> and Phil Carns <pcarns@parl.clemson.edu>");
MODULE_DESCRIPTION("pvfs: PVFS VFS support module");
MODULE_PARM(debug, "1i"); /* one integer debugging parameter */
MODULE_PARM_DESC(debug, "debugging level, 0 is no debugging output");
MODULE_PARM(maxsz, "1i"); /* one integer max size parameter */
MODULE_PARM_DESC(maxsz, "maximum request size, used to limit memory utilization");
MODULE_PARM(buffer, "s"); /* what kind of buffer to use to move data around */
MODULE_PARM_DESC(buffer, "buffering technique, static, dynamic, or mapped");

/* we don't need any symbols exported, so let's keep the ksyms table
 * clean.
 */
EXPORT_NO_SYMBOLS;

static int init_pvfs_fs(void);
int pvfsdev_init(void);
void pvfsdev_cleanup(void);

/* GLOBALS */
static int debug = -1;
static int maxsz = -1;
static char *buffer = NULL;
int pvfs_debug = PVFS_DEFAULT_DEBUG_MASK;
int pvfs_buffer = PVFS_DEFAULT_BUFFER;
int pvfs_maxsz = PVFS_DEFAULT_IO_SIZE;

int init_module (void)
{
	int i;
	char b_static[] = "static";
	char b_dynamic[] = "dynamic";
	char b_mapped[] = "mapped";
	char *buftype = NULL;

	/* parse module options, print a configuration line */
	if (debug > -1) pvfs_debug = debug;
	if (maxsz >= PVFS_MIN_IO_SIZE) pvfs_maxsz = maxsz;
	if (buffer != NULL) {
		if (strcmp(buffer, b_static) == 0)
			pvfs_buffer = PVFS_BUFFER_STATIC;
#ifdef HAVE_PVFS_BUFMAP
		else if (strcmp(buffer, b_mapped) == 0)
			pvfs_buffer = PVFS_BUFFER_MAPPED;
#endif
		else if (strcmp(buffer, b_dynamic) == 0)
			pvfs_buffer = PVFS_BUFFER_DYNAMIC;
		else {
			printk("buffer value %s is invalid; defaulting to %s\n",
			buffer, b_dynamic);
			pvfs_buffer = PVFS_BUFFER_DYNAMIC;
		}
	}
	switch (pvfs_buffer) {
		case PVFS_BUFFER_DYNAMIC: buftype = b_dynamic; break;
		case PVFS_BUFFER_MAPPED:  buftype = b_mapped;  break;
		case PVFS_BUFFER_STATIC:  buftype = b_static;  break;
		default: printk("pvfs: init_module: should not hit here\n");
	}
	printk("pvfs: debug = 0x%x, maxsz = %d bytes, buffer = %s\n", pvfs_debug,
	pvfs_maxsz, buftype);

	/* initialize all the good stuff */
	i = pvfsdev_init();
	if (i) return i;

	i = init_pvfs_fs();
	return i;
}

void cleanup_module (void)
{
	unregister_filesystem(&pvfs_fs_type);
	pvfsdev_cleanup();
}

static int init_pvfs_fs(void)
{
	return register_filesystem(&pvfs_fs_type);
}

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */
