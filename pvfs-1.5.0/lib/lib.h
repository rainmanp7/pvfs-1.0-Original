/*
 * lib.h copyright (c) 1996 Rob Ross, all rights reserved.
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
 * Contact:  Rob Ross  rbross@parl.eng.clemson.edu  or
 * 
 */

#ifndef  LIB_H
#define  LIB_H

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <meta.h>
#include <req.h>
#include <desc.h>
#ifdef FS_H_IS_OK
#include <linux/fs.h> /* NR_OPEN declaration */
#endif
#include <debug.h>
#include <build_job.h>
#include <sockset.h>
#include <jlist.h>
#include <pvfs_config.h>
#include "../config.h"

#undef PERROR
#ifdef DEBUG
#define PERROR(x) {perror(x); fflush(stderr);}
#else
#define PERROR(x);
#endif

#ifndef NR_OPEN
#define NR_OPEN 256
#endif

/*PROTOTYPES*/
void *pvfs_exit(void);
int unix_lstat(const char *fn, struct stat *s_p);
int unix_stat(const char *fn, struct stat *s_p);
int unix_fstat(int fd, struct stat *s_p);
int pvfs_detect(const char *fn, char **, struct sockaddr **, int64_t *);

int send_mreq_saddr(struct sockaddr *saddr_p, mreq_p req_p,
	void *data_p, mack_p ack_p);
int send_mreq_dmeta(dmeta_p dir_p, mreq_p req_p, void *data_p,
	mack_p ack_p);

char *dmeta2fname(dmeta_p);

#endif
