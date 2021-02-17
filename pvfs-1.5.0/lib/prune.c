/*
 * prune.c copyright (c) 1996 Rob Ross, all rights reserved.
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

/* INCLUDES */
#include <alist.h>
#include <jlist.h>
#include <debug.h>

static char prune_c[] = "$Header: /projects/cvsroot/pvfs/lib/prune.c,v 1.2 1999/08/13 23:26:20 rbross Exp $";

/* PRUNE_ALIST() - Searches an access list for accesses matching
 * the provided socket and type, and removes any accesses that would
 * result in more than n_size bytes of data transfer of the given type
 * on the given socket
 *
 * PARAMETERS:
 * al_p   - pointer to access list to prune
 * t      - type of access (defined in alist.h)
 * s      - socket # for accesses
 * o_size - old job size for this socket
 * n_size - new job size for this socket (desired size)
 *
 * NOTE: This function is dependent on the access list implementation!!!
 *
 * Returns 0 on success, -1 on failure.
 */
int prune_alist(alist_p al_p, int t, int s, int o_size, int n_size)
{
	int t_size=0;
	llist_p l_p;
	if (n_size < 0 || o_size < n_size || !al_p || !al_p->next) return(-1);
	if (o_size == n_size) return(0);

	l_p = (llist_p)al_p->next; /* first item in list */
	while (l_p && t_size < n_size) {
		ainfo_p a_p = (ainfo_p)l_p->item; /* only for clarity */
		if (a_p->type == t && a_p->sock == s) {
			switch(t) {
				case A_READ:
				case A_WRITE:
					t_size += a_p->u.rw.size;
					if (t_size > n_size) a_p->u.rw.size -= t_size-n_size;
					break;
			}
		}
		l_p = l_p->next;
	}
	while (l_p) {
		llist_p tmpl_p = l_p;
		l_p = l_p->next;
		if (((ainfo_p)tmpl_p->item)->type == t
		&&  ((ainfo_p)tmpl_p->item)->sock == s)
			a_ptr_rem(al_p, (ainfo_p)tmpl_p->item);
	}
	return(0);
}

