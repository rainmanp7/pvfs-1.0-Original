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

#ifndef __PVFS_V1_XFER_H
#define __PVFS_V1_XFER_H

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <malloc.h>

#ifdef LINUX
#include <linux/types.h>
#include <linux/dirent.h>
#endif

#include <ll_pvfs.h>

/* pvfs_comm_init()
 *
 * Sets up any necessary data structures for use in PVFS data transfer.
 * Should be called once before any transfers take place.
 *
 * Returns 0 on success, or -errno on failure.
 */
int pvfs_comm_init(void);

/* do_pvfs_op(op, resp)
 *
 * Performs a PVFS operation.  See ll_pvfs.h for the valid operation
 * types and the structures involved.
 *
 * Returns 0 on success, -errno on failure.
 */
int do_pvfs_op(struct pvfs_upcall *op, struct pvfs_downcall *resp);

/* pvfs_comm_shutdown()
 *
 * Performs any operations necessary to cleanly shut down PVFS
 * communication.  Should be called once after all desired communication
 * is complete.
 */
void pvfs_comm_shutdown(void);

/* pvfs_comm_idle()
 *
 * Performs operations associated with closing idle connections and open
 * files that are no longer in use.
 */
void pvfs_comm_idle(void);

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */

#endif
