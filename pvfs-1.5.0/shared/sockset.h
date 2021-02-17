/*
 * sockset.h copyright (c) Clemson University, all rights reserved
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
 * Contact:  Rob Ross  	rbross@parl.eng.clemson.edu
 *           Matt Cettei mcettei@parl.eng.clemson.edu
 * 
 */

/*
 * SOCKSET.H - defines and structures to translate generic socket selection
 *                routines into platform specific calls
 */

#ifndef SOCKSET_H
#define SOCKSET_H

/* TYPEDEFS */
typedef struct sockset sockset, *sockset_p;

/* INCLUDES */
#include <dfd_set.h>

/* STRUCTURES */
struct sockset {
	int max_index;
	dyn_fdset read; /* used to save set between selects */
	dyn_fdset write; /* used to save set between selects */
	dyn_fdset tmp_read; /* used to store results of last select */
	dyn_fdset tmp_write; /* used to store results of last select */
	struct timeval timeout;
};

/* PROTOTYPES */
void addsockrd(int, sockset_p);
void addsockwr(int, sockset_p);
void delsock(int, sockset_p);
int nextsock(sockset_p);
int dumpsocks(sockset_p);
void initset(sockset_p);
void finalize_set(sockset_p);
int check_socks(sockset_p, int);
int randomnextsock(int i);



#endif
