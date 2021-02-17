/*
 * jlist.c copyright (c) 1996 Rob Ross, all rights reserved.
	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as published
	by the Free Software Foundation; either version 2 of the License, or
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of 
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
	Contact:  Rob Ross  rbross@parl.eng.clemson.edu  or
				 Matt Cettei mcettei@parl.eng.clemson.edu
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
#include <debug.h>

static char jlist_c[] = "$Header: /projects/cvsroot/pvfs/lib/jlist.c,v 1.4 2000/11/22 16:06:05 rbross Exp $";

/* prototypes for internal functions */
static int j_sock_cmp(void *key, void *j_p);
static void j_free(void *j_p);

jlist_p jlist_new(void)
{
	return(llist_new());
}

int jlist_empty(jlist_p jl_p)
{
	return(llist_empty(jl_p));
}

int j_add(jlist_p jl_p, jinfo_p j_p)
{
	return(llist_add(jl_p, (void *) j_p));
}

jinfo_p j_search(jlist_p jl_p, int sock)
{
	return((jinfo_p) llist_search(jl_p, (void *) &sock, j_sock_cmp));
}

int j_rem(jlist_p jl_p, int sock)
{
	jinfo_p j_p;
	
	j_p = (jinfo_p) llist_rem(jl_p, (void *) &sock, j_sock_cmp);
	if (j_p) {
		j_free(j_p);
		return(0);
	}
	return(-1);
}

static int j_sock_cmp(void *key, void *j_p)
{
	if (dfd_isset((int)(*(int *) key), &((jinfo_p) j_p)->socks)) return(0);
	else return(1);
}

void jlist_cleanup(jlist_p jl_p)
{
	llist_free(jl_p, j_free);
}

/* j_free() - frees the memory allocated to store a job */
static void j_free(void *j_p)
{
	alist_cleanup(((jinfo_p) j_p)->al_p);
	dfd_finalize(&((jinfo_p)j_p)->socks);
	free((jinfo_p) j_p);
}

int jlist_dump(jlist_p jl_p)
{
	return(llist_doall(jl_p, j_dump));
}

int j_dump(void *v_p)
{
	jinfo_p j_p = (jinfo_p) v_p;
	
	LOG2("type: %d\nsize: %d\n", j_p->type, j_p->size);
	alist_dump(j_p->al_p);
	return(0);
}

jinfo_p j_new(int pcount) 
{
	jinfo_p j_p;
	int i;

	if ((j_p = (jinfo_p) malloc(sizeof(jinfo)
		+ sizeof(int64_t)*(pcount-1))) == NULL) {
		return(0);
	}
	if (!(j_p->al_p = alist_new())) /* error creating access list */ {
		free(j_p);
		return(0);
	}

	j_p->type = 0;
	j_p->max_nr = 0;
	dfd_init(&j_p->socks, 1);
	j_p->size = 0; /* new job - no accesses */
	for (i=0; i < pcount; i++) j_p->psize[i] = 0;
	return(j_p);
}




