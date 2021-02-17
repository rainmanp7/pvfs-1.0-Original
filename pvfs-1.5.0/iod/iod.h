/*
 * iod.h copyright (c) 1999 Clemson University, all rights reserved.
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
 *
 * Contact:  Rob Ross    rbross@parl.clemson.edu
 */

#ifndef IOD_H
#define IOD_H

#include <minmax.h>
#include <req.h>
#include <alist.h>
#include <flist.h>
#include <jlist.h>
#include <pvfs_config.h>

#define STATNUM 5000

/* PROTOTYPES */
char *fname(ino_t f_ino);
int send_error(int status, int eno, int sock, iack_p);
ainfo_p ack_a_new(void);

int do_close_req(int sock, ireq_p r_p, iack_p a_p);
int do_open_req(int sock, ireq_p r_p, iack_p a_p);
int do_rw_req(int sock, ireq_p r_p, iack_p a_p);
int do_stat_req(int sock, ireq_p r_p, iack_p a_p);
int do_unlink_req(int sock, ireq_p r_p, iack_p a_p);
int do_shutdown_req(int sock, ireq_p r_p, iack_p a_p);
int do_gstart_req(int sock, ireq_p r_p, iack_p a_p);
int do_gadd_req(int sock, ireq_p r_p, iack_p a_p);
int do_instr_req(int sock, ireq_p r_p, iack_p a_p);
int do_ftruncate_req(int sock, ireq_p r_p, iack_p a_p);
int do_truncate_req(int sock, ireq_p r_p, iack_p a_p);
int do_fdatasync_req(int sock, ireq_p r_p, iack_p a_p);
int do_noop_req(int sock, ireq_p r_p, iack_p a_p);
int do_statfs_req(int sock, ireq_p r_p, iack_p a_p);
int update_fsize(finfo_p f_p);
int count_access(jinfo_p job_p, int *off_p);

void *do_signal(int);

/* SOCKINFO.C PROTOTYPES */
int is_active(int s);
int set_active(int s);
int clr_active(int s);

/* DEFINES */
/* mmap defines */
#define PAGENO(x) ((x) & (~(IOD_ACCESS_PAGE_SIZE - 1)))
#define PAGEOFF(x) ((x) & (IOD_ACCESS_PAGE_SIZE - 1))

#define IOD_MMAP_FLAGS MAP_FILE | MAP_SHARED | MAP_VARIABLE
#define IOD_MMAP_PROT PROT_READ

#ifndef MAP_VARIABLE
#define MAP_VARIABLE 0
#endif


#endif
