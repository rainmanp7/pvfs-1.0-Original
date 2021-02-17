/*
 * jobs.h copyright (c) 1997 Clemson University, all rights reserved.
 *
 * Written by Rob Ross and Matt Cettei.
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
 * Contact:  Rob Ross    rbross@parl.eng.clemson.edu
 *           Matt Cettei mcettei@parl.eng.clemson.edu
 */

#ifndef BUILD_JOB_H
#define BUILD_JOB_H

#include <jlist.h>
#include <req.h>

/* prototypes */
jinfo_p build_job(int sock, ireq_p, finfo_p);
int do_job(int sock, jinfo_p j_p);
int j_merge(jinfo_p j1_p, jinfo_p j2_p);

/* NEXTPL() modifies fl */
/* Description of cryptic variables:
 * pl - 	partition location; location (offset) into file containing
 *		  	data for this partition
 *	fl	-	file location; location (offset) into the logical file that 
 *			is defined by the application's view (?)
 * rl -	relative location (?); whatever you call it, it is the offset
 * 		in the file if the partitions were spliced together into one
 *			correctly ordered file (?).
 * df -	I don't know yet.
 */
#define NEXTPL(fp,rl,fl) do { \
	register int64_t __df, \
	         __st, /* size of a stride in bytes */ \
            __off; /* loc. in "whole file" where file on this node starts */ \
	if ((__df = ((rl - (__off = (fp->p_stat.soff + fp->pnum*fp->p_stat.ssize))) \
	% (__st = fp->p_stat.pcount*fp->p_stat.ssize))) < 0) \
	{ \
		fl = __off; \
	} \
	else if (__df >= fp->p_stat.ssize) fl = rl + (__st - __df); \
	else fl = rl; \
} while(0)

#define FLTOPL(fp,fl) (((fl - fp->p_stat.soff) / fp->p_stat.ssize) \
	/ fp->p_stat.pcount * fp->p_stat.ssize \
	+ ((fl - fp->p_stat.soff) % fp->p_stat.ssize))

#define TOPEND(fp,pl) (fp->p_stat.ssize - (pl % fp->p_stat.ssize))

#endif

