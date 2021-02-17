/*
 * build_job.h copyright (c) 1996 Clemson University, all rights reserved.
 *
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
 *
 * Contact:  Rob Ross  rbross@parl.eng.clemson.edu  or
 *           Matt Cettei mcettei@parl.eng.clemson.edu
 */

#ifndef BUILD_JOB_H
#define BUILD_JOB_H

/* PVFS INCLUDES */
#include <desc.h>
#include <req.h>

int build_rw_jobs(fdesc_p f_p, char *buf_p, int64_t size, int type);
void *add_accesses(fdesc_p f_p, int64_t rl, int64_t rs, char *buf_p, int type);
int build_rw_req(fdesc_p f_p, int64_t size, ireq_p r_p, int type, int num);

#endif
