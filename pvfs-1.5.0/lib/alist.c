/*
 * alist.c copyright (c) 1996 Rob Ross, all rights reserved.
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
#include <debug.h>

static char alist_c[] = "$Header: /projects/cvsroot/pvfs/lib/alist.c,v 1.3 1999/08/16 18:12:29 rbross Exp $";

static void a_free(void *a_p);
static int a_sock_cmp(void *key, void *a_p);
static int a_ptr_cmp(void *key, void *a_p);

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

ainfo_p a_search(alist_p al_p, int sock)
{
	return((ainfo_p) llist_search(al_p, (void *) &sock, a_sock_cmp));
}

static int a_sock_cmp(void *key, void *a_p) 
{
	if (*((int *) key) == ((ainfo_p) a_p)->sock) return(0);
	else return(1);
}

static int a_ptr_cmp(void *key, void *a_p)
{
	if ((ainfo_p) key == (ainfo_p) a_p) return(0);
	else return(1);
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

	LOG2("\n  type: %d\n  sock: %d\n", a_p->type, a_p->sock);
	switch(a_p->type) {
		case A_READ:
		case A_WRITE:
			LOG2("  loc: %lu\n  size: %d\n", a_p->u.rw.loc, a_p->u.rw.size);
			break;
		case A_REQ:
			LOG3("  req_p: %lu\n  cur_p: %lu\n  size: %d\n",
				a_p->u.req.req_p, a_p->u.req.cur_p, a_p->u.req.size);
			break;
		case A_ACK:
			LOG4("  iod_nr: %d\n  ack_p: %lu\n  cur_p: %lu\n  size: %lu\n", 
				a_p->u.ack.iod_nr, a_p->u.ack.ack_p, a_p->u.ack.cur_p, 
				a_p->u.ack.size);
			break;
	}
	return(0);
}

ainfo_p ainfo_new(void) 
{
	ainfo_p a_p;

	if ((a_p = (ainfo_p) malloc(sizeof(ainfo))) == NULL) {
   	return(0);
   }
 	a_p->type = -1;
 	a_p->sock = -1;
	return(a_p);
}


ainfo_p a_get_start(alist_p al_p)
{
	return(llist_head(al_p));
}
