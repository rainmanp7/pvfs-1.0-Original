/*
 * jlist.c copyright (c) 1999 Clemson University, all rights reserved.
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

/*
 * JLIST.C - functions to handle the creation and modification of 
 *
 * This is built on top of the llist.[ch] files
 * Note: this file is used both in the iod and in library functions
 *
 */

#include <stdlib.h>
#include <jlist.h>

static char jlist_c[] = "$Header: /projects/cvsroot/pvfs/iod/jlist.c,v 1.6 2000/11/22 16:06:05 rbross Exp $";

/* prototypes for internal functions */
static int j_sock_cmp(void *key, void *j_p);
static int j_ptr_cmp(void *key, void *j_p);
static int j_cnt(void *v_p);
static int internal_jlist_count(jlist_p jl_p);

static int j_count;
static int tmp_count;

/* jlist_count - quick and dirty counting system that works if only one
 * jlist is in use
 */
int jlist_count(void *foo)
{
	return j_count;
}

jlist_p jlist_new(void)
{
	j_count = 0;
	return(llist_new());
}

int jlist_empty(jlist_p jl_p)
{
	return(llist_empty(jl_p));
}

int j_add(jlist_p jl_p, jinfo_p j_p)
{
	int i;
	i = llist_add(jl_p, (void *) j_p);
	if (i == 0) j_count++;
	return i;
}

jinfo_p j_search(jlist_p jl_p, int sock)
{
	return((jinfo_p) llist_search(jl_p, (void *) (&sock), j_sock_cmp));
}

int j_sock_rem(jlist_p jl_p, int sock)
{
	jinfo_p j_p;
	
	j_p = (jinfo_p) llist_rem(jl_p, (void *) (&sock), j_sock_cmp);
	if (j_p) {
		j_count--;
		j_free(j_p);
		return(0);
	}
	return(-1);
}

int j_ptr_rem(jlist_p jl_p, void *j_p)
{
	jinfo_p tmp_p;
	
	tmp_p = (jinfo_p) llist_rem(jl_p, j_p, j_ptr_cmp);
	if (tmp_p) {
		j_count--;
		j_free(tmp_p);
		return(0);
	}
	return(-1);
}

static int j_sock_cmp(void *key, void *j_p)
{
	if (dfd_isset((int)(*(int *)key), &((jinfo_p) j_p)->socks)) return(0);
	return(1);
}

static int j_ptr_cmp(void *key, void *j_p)
{
	if ((jinfo_p) key == (jinfo_p) j_p) return(0);
	return(1);
}

void jlist_cleanup(jlist_p jl_p)
{
	llist_free(jl_p, j_free);
}

/* j_free() - frees the memory allocated to store a job */
void j_free(void *v_p)
{
	jinfo_p j_p = (jinfo_p) v_p;

	alist_cleanup(j_p->al_p);
	dfd_finalize(&j_p->socks);
	free(j_p);
}

int jlist_dump(jlist_p jl_p)
{
	return(llist_doall(jl_p, j_dump));
}

static int internal_jlist_count(jlist_p jl_p)
{
	tmp_count = 0;

	return(llist_doall(jl_p, j_cnt));
}

static int j_cnt(void *v_p)
{
	tmp_count++;
	return 0;
}

int j_dump(void *v_p)
{
	jinfo_p j_p = (jinfo_p) v_p;
	
	fprintf(stderr, "type: %d\nsize: %Ld, socket:%d\n", j_p->type, j_p->size,
	j_p->max_nr - 1);
	alist_dump(j_p->al_p);
	return(0);
}

jinfo_p j_new(void)
{
	jinfo_p j_p;

	if (!(j_p = (jinfo_p) malloc(sizeof(jinfo)))) return(0);

	if (!(j_p->al_p = alist_new())) return(0);

	j_p->type   = -1;
	j_p->nospc  = 0;
	j_p->size   = 0;
	j_p->file_p = 0;
	j_p->lastb  = -1;
	j_p->max_nr = 0;
	dfd_init(&j_p->socks, 1);
	return(j_p);
}

/* jlist_purge_sock() - searches through all jobs in job list and
 * removes all references to a given socket
 */
int jlist_purge_sock(jlist_p jl_p, int s)
{
	jinfo_p j_p;

	while ((j_p = j_search(jl_p, s)) != NULL) {
		/* still a job w/this socket */
		/* remove all references in job */
		while (a_sock_rem(j_p->al_p, s) == 0);
		dfd_clr(s, &j_p->socks);

		/* if there are no accesses left in job, remove it altogether */
		if (alist_empty(j_p->al_p)) j_ptr_rem(jl_p, j_p);
	}
	return(0);
}
