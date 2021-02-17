/*
 * debug.h copyright (c) 1996 Rob Ross, all rights reserved.
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
 * Contact:  Rob Ross  	rbross@parl.clemson.edu
 *           Walt Ligon walt@parl.clemson.edu
 * 
 */

#include <sys/time.h>

extern int debug_on;

typedef struct iodstat iodstat, *iodstat_p;

struct iodstat {
	struct timeval st_wait;
	struct timeval st_serv;
	struct timeval end_serv;
	int	 type;
	int	 numaccess;
	int	 fd;
	int 	 offset; 
};

#ifdef DEBUG
#define LOG(x) if (debug_on>0) { fprintf(stderr, x); fflush(stderr); }
#define LOG1(x,y) if (debug_on>0) { fprintf(stderr, x, (y)); fflush(stderr); }
#define LOG2(x,y,z) if (debug_on>0) { fprintf(stderr, x, (y), (z)); \
	fflush(stderr); }
#define LOG3(w,x,y,z) if (debug_on>0) { fprintf(stderr, w, (x), (y), (z)); \
	fflush(stderr); }
#define LOG4(v,w,x,y,z) if (debug_on>0) { fprintf(stderr,v,(w),(x),(y),(z)); \
	fflush(stderr); }
#else
#define LOG(x);
#define LOG1(x,y);
#define LOG2(x,y,z);
#define LOG3(w,x,y,z);
#define LOG4(v,w,x,y,z);
#endif

#if 1
#define ERR(x) { fprintf(stderr, x); fflush(stderr); }
#define ERR1(x,y) { fprintf(stderr, x, (y)); fflush(stderr); }
#define ERR2(x,y,z) { fprintf(stderr, x, (y), (z)); \
	fflush(stderr); }
#define ERR3(w,x,y,z) { fprintf(stderr, w, (x), (y), (z)); \
	fflush(stderr); }
#else
#define ERR(x);
#define ERR1(x,y);
#define ERR2(x,y,z);
#define ERR3(w,x,y,z);
#endif

#if 1
#define PERROR(x) { perror(x); fflush(stderr); }
#else
#define PERROR(x);
#endif
