/*
 * flist.c copyright (c) 1996 Clemson University, all rights reserved.
 * 
 * 
 * Written by Rob Ross and Matt Cettei.
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
 * Contact:  Rob Ross  	rbross@parl.eng.clemson.edu
 *           Matt Cettei mcettei@parl.eng.clemson.edu
 * 
 */

/*
 * FLIST.C - functions to handle the creation and modification of 
 * lists of file information
 *
 * Note: a SINGLE entry is added to the list for an open file, 
 * no matter how many times the file has been opened.
 *
 * This is built on top of the llist.[ch] files
 *
 */

#include <stdlib.h>
#include <sys/types.h>
#include <flist.h>
#include <dfd_set.h>

/* prototypes for internal functions */
static int f_ino_cmp(void *key, void *f_p);

flist_p flist_new(void)
{
	return(llist_new());
}

int flist_empty(flist_p fl_p)
{
	return(llist_empty(fl_p));
}

int f_add(flist_p fl_p, finfo_p f_p)
{
	return(llist_add(fl_p, (void *) f_p));
}

finfo_p f_search(flist_p fl_p, ino_t f_ino)
{
	return((finfo_p) llist_search(fl_p, (void *) (&f_ino), f_ino_cmp));
}

int f_rem(flist_p fl_p, ino_t f_ino)
{
	finfo_p f_p;
	
	f_p = (finfo_p) llist_rem(fl_p, (void *) (&f_ino), f_ino_cmp);
	if (f_p) {
		f_free(f_p->f_name);
		f_free(f_p);
		return(0);
	}
	return(-1);
}

void flist_cleanup(flist_p fl_p)
{
	llist_free(fl_p, f_free);
}

static int f_ino_cmp(void *key, void *f_p)
{
	if ((ino_t)(*(ino_t *) key) == ((finfo_p)f_p)->f_ino) return(0);
	else return(1);
}

finfo_p f_new(void) {
	finfo_p f_p;

	if (!(f_p = (finfo_p) malloc(sizeof(finfo)))) return(0);

	f_p->cap = 0;
	f_p->cnt = 1;
	f_p->grp_cap = -1;
	f_p->f_name = 0;
	f_p->unlinked = -1;
	dfd_init(&f_p->socks, 1);
	return(f_p);
}

void f_free(void *f_p)
{
	dfd_finalize(&((finfo_p)f_p)->socks);
	free((finfo_p)f_p);
}

int flist_dump(flist_p fl_p)
{
	return(llist_doall(fl_p, f_dump));
}

int f_dump(void *v_p)
{
	finfo_p f_p = (finfo_p) v_p; /* quick cast */

	fprintf(stderr, "i %ld, b %d, p %d, s %d, n %d, %s\n",
		f_p->f_ino, f_p->p_stat.base, f_p->p_stat.pcount,
		f_p->p_stat.ssize, f_p->cnt, f_p->f_name);
	return(0);
}

int forall_finfo(flist_p fl_p, int (*fn)(void *))
{
	return(llist_doall(fl_p, fn));
}
