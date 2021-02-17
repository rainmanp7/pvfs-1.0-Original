/*
 * sockset.c copyright (c) Clemson University, all rights reserved
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
 * Matt Cettei mcettei@parl.eng.clemson.edu
 * 
 */

/*
 * CHECK_SOCK.C - globals and functions used to translate generic socket
 *					 selection routines into platform specific calls
 */

static char check_sock_c[]="$Header: /projects/cvsroot/pvfs/shared/sockset.c,v 1.4 2000/10/03 21:33:13 rbross Exp $";

/* UNIX INCLUDES */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

/* PVFS INCLUDES */
#include <sockset.h>
#include <dfd_set.h>
#include <pvfs_config.h> /* only for __RANDOM_NEXTSOCK__ define */

/* GLOBALS */
extern int errno;
#ifdef __RANDOM_NEXTSOCK__
static int sockset_randomnextsock = 0;
#endif

/* FUNCTIONS */

void addsockrd(int sock, sockset_p set_p)
{
	dfd_set(sock, &set_p->read);
	/* max_index MUST equal (max. sock # + 1) */
	if (sock >= set_p->max_index) set_p->max_index = sock+1;
}

void addsockwr(int sock, sockset_p set_p)
{
	dfd_set(sock, &set_p->write);
	if (sock >= set_p->max_index) set_p->max_index = sock+1;
}

void delsock(int sock, sockset_p set_p)
{
	/* make sure we're not going to check for this any more */
	dfd_clr(sock, &set_p->read);
	dfd_clr(sock, &set_p->write);
	dfd_clr(sock, &set_p->tmp_read);
	dfd_clr(sock, &set_p->tmp_write);

	if (sock + 1 >= set_p->max_index) /* need to reduce max_index */ {
		int i;
		for (i = sock; i >= 0; i--) {
			if (dfd_isset(i, &set_p->read) || dfd_isset(i, &set_p->write)) break;
		}
		set_p->max_index = i+1;
	}
}

int randomnextsock(int i)
{
#ifdef __RANDOM_NEXTSOCK__
	struct timeval curtime;

	sockset_randomnextsock = i;

	gettimeofday(&curtime, NULL);
	srand(curtime.tv_usec);

	return sockset_randomnextsock;
#else
	return 1;
#endif
}

/* nextsock() - find a ready but unserviced socket in our sockset
 *
 * Marks socket as "serviced" before returning, so it won't be returned
 * again until checksock() is used again and it is shown to be ready.
 *
 * Returns socket number on success, -1 on failure.
 */
int nextsock(sockset_p set_p)
{
	int i = 0, startval = 0;
	int max_index;

	max_index = set_p->max_index;

	if (max_index <= 0) return -1;

#ifdef __RANDOM_NEXTSOCK__
	if (sockset_randomnextsock) {
		startval = rand() % max_index;
	}

	for (i = startval; i < max_index; i++) {
		if (dfd_isset(i, &set_p->tmp_read) || dfd_isset(i, &set_p->tmp_write)) {
			dfd_clr(i, &set_p->tmp_read);
			dfd_clr(i, &set_p->tmp_write);
			return i;
		}
	}
	/* now i = max_index; test the low region */
#else
	/* we want to avoid using two loops in the normal case, so we check
	 * the entire range with this last one.
	 */
	startval = max_index;
#endif
	for (i = 0; i < startval; i++) {
		if (dfd_isset(i, &set_p->tmp_read) || dfd_isset(i, &set_p->tmp_write)) {
			dfd_clr(i, &set_p->tmp_read);
			dfd_clr(i, &set_p->tmp_write);
			return i;
		}
	}
	return -1;
} /* end of nextsock() */

int dumpsocks(sockset_p set_p)
{
	int i;
	fprintf(stderr, "max_index = %d\n", set_p->max_index);
	for (i = 0; i < set_p->max_index; i++) {
		if (dfd_isset(i, &set_p->read)) fprintf(stderr, "%d:\tREAD\n", i);
		if (dfd_isset(i, &set_p->write)) fprintf(stderr, "%d:\tWRITE\n", i);
	} 
	return(0);
} /* end of DUMPSOCKS() -- select() version */


void finalize_set(sockset_p set_p)
{
	dfd_finalize(&set_p->read);
	dfd_finalize(&set_p->write);
	dfd_finalize(&set_p->tmp_read);
	dfd_finalize(&set_p->tmp_write);
}

void initset(sockset_p set_p)
{
	set_p->max_index = 0;
	/* call dfd_init() in case the sets have never been initialized;
	 * if that fails then call dfd_zero() to clean everything out
	 */

	if (dfd_init(&set_p->read, 0) < 0) dfd_zero(&set_p->read);
	if (dfd_init(&set_p->write, 0) < 0) dfd_zero(&set_p->write);
	if (dfd_init(&set_p->tmp_read, 0) < 0) dfd_zero(&set_p->tmp_read);
	if (dfd_init(&set_p->tmp_write, 0) < 0) dfd_zero(&set_p->tmp_write);
}

/* time given in msec */
/* Returns # of sockets in descriptor set (?), or -1 on error
 */
int check_socks(sockset_p set_p, int time)
{
	int ret;
	int setsize;


check_socks_restart:
	/* copy the real structures into the temporary ones */
	dfd_copy(&set_p->tmp_read, &set_p->read);
	dfd_copy(&set_p->tmp_write, &set_p->write);

	if (time == -1) {
		ret = dfd_select(set_p->max_index, &set_p->tmp_read,
		&set_p->tmp_write, NULL, NULL);
	}
	else {
		set_p->timeout.tv_sec = time / 1000;
		set_p->timeout.tv_usec = (time % 1000) * 1000;
		ret = dfd_select(set_p->max_index, &set_p->tmp_read,
		&set_p->tmp_write, NULL, &set_p->timeout);
	}
	if (ret < 0 && errno == EINTR) goto check_socks_restart;
	return(ret);
}






