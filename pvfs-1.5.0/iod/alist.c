/*
 * alist.c copyright (c) 1997 Clemson University, all rights reserved.
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

/*
 * ALIST.C - functions to handle the creation and modification of 
 * lists of accesses
 *
 * This is built on top of the llist.[ch] files
 * NOTE: This file is used both in the iod and in library functions
 *
 */

#include <stdlib.h>
#include <alist.h>

static char alist_c[] = "$Header: /projects/cvsroot/pvfs/iod/alist.c,v 1.3 2000/02/06 03:29:53 rbross Exp $";

static void a_free(void *a_p);
static int a_sock_cmp(void *key, void *a_p);
static int a_type_cmp(void *key, void *a_p);

alist_p alist_new(void)
{
	return((alist_p) llist_new());
}

int alist_empty(alist_p al_p)
{
	return(llist_empty(al_p));
}	

int a_add_start(alist_p al_p, ainfo_p a_p)
{
	return(llist_add_to_head(al_p, (void *) a_p));
}

int a_add_end(alist_p al_p, ainfo_p a_p)
{
	return(llist_add_to_tail(al_p, (void *) a_p));
}

ainfo_p a_sock_search(alist_p al_p, int sock)
{
	return((ainfo_p) llist_search(al_p, (void *) &sock, a_sock_cmp));
}

ainfo_p a_type_search(alist_p al_p, int type)
{
	return((ainfo_p) llist_search(al_p, (void *) &type, a_type_cmp));
}

static int a_sock_cmp(void *key, void *a_p) 
{
	if (*(int *)key == ((ainfo_p) a_p)->sock) return(0);
	return(1);
}

int a_ptr_cmp(void *key, void *a_p)
{
	if ((ainfo_p) key == (ainfo_p) a_p) return(0);
	return(1);
}

int a_type_cmp(void *key_p, void *a_p)
{
	if (*(int *)key_p == ((ainfo_p)a_p)->type) return(0);
	return(1);
}

ainfo_p a_get_start(alist_p al_p)
{
	return(llist_head(al_p));
}

int a_sock_rem(alist_p al_p, int sock)
{
	ainfo_p a_p;
	
	a_p = (ainfo_p) llist_rem(al_p, (void *) &sock, a_sock_cmp);
	if (a_p) {
		a_free(a_p);
		return(0);
	}
	return(-1);
}

int a_ptr_rem(alist_p al_p, ainfo_p a_p)
{
	ainfo_p tmp_p;
	
	tmp_p = (ainfo_p) llist_rem(al_p, (void *) a_p, a_ptr_cmp);
	if (tmp_p) {
		a_free(tmp_p);
		return(0);
	}
	return(-1);
}

void alist_cleanup(alist_p al_p)
{
	llist_free(al_p, a_free);
}

static void a_free(void *a_p)
{
	free((ainfo_p) a_p);
}

int alist_dump(alist_p al_p)
{
	return(llist_doall(al_p, a_dump));
}

int a_dump(void *v_p)
{
	ainfo_p a_p = (ainfo_p) v_p; /* quick cast */

	printf("  type: %d\n  sock: %d\n  rw.off: %Ld\n  rw.size: %Ld\n\n",
		a_p->type, a_p->sock, a_p->u.rw.off, a_p->u.rw.size);
	return(0);
}
