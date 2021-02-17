/*
 * flist.c copyright (c) 1996 Clemson University, all rights reserved.
 *
 * Written by Rob Ross, updated by Matt Cettei.
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
 * FLIST.C - functions to handle the creation and modification of 
 * lists of file information
 *
 * This is built on top of the llist.[ch] files
 *
 */

#include <stdlib.h>
#include <sys/types.h>
#include <flist.h>
#include <unistd.h>
#include <sys/mman.h>

static char header[] = "$Header: /projects/cvsroot/pvfs/iod/flist.c,v 1.3 2000/02/06 03:29:53 rbross Exp $";

/* prototypes for internal functions */
static void f_free(void *);
static int f_ino_cap_cmp(void *, void *);
static int f_ino_cmp(void *i_p, void *f_p);
static int f_sock_cmp(void *s_p, void *f_p);

/* internal structures */
struct inocap {
	int64_t f_ino;
	int32_t cap;
};

flist_p flist_new(void) {
	return(llist_new());
}

int f_add(flist_p fl_p, finfo_p f_p) {
	return(llist_add(fl_p, (void *) f_p));
}

/* f_ino_cap_search() - find an item in the list
 *
 * Finds an item matching the inode and capability given, or the inode
 * if a capability of -1 is passed in.
 *
 * Returns NULL on failure, or a pointer to the item on success.
 */
finfo_p f_ino_cap_search(flist_p fl_p, int64_t f_ino, int32_t cap) {
	struct inocap ic;
	ic.f_ino = f_ino;
	ic.cap   = cap;

	if (cap == -1) return((finfo_p) llist_search(fl_p, (void *) &f_ino,
		f_ino_cmp));
	else return((finfo_p) llist_search(fl_p, (void *) &ic, f_ino_cap_cmp));
}

/* f_sock_rem() - removes a file info item from a list of such items
 * matching a given socket number (using f_sock_cmp)
 */
int f_sock_rem(flist_p fl_p, int s) {
	finfo_p f_p;

	f_p = (finfo_p) llist_rem(fl_p, (void *)&s, f_sock_cmp);
	if (f_p) {
		f_free(f_p);
		return(0);
	}
	return(-1);
}

/* f_ino_cap_rem() - remove an item from the list
 *
 * Finds an item matching the given inode and cap, and if found 
 * removes the item from the list and frees the associated memory
 *
 * If the cap value is -1, all items matching the inode are removed.
 * In this case the call is considered successful if at least one item
 * is removed.
 *
 * Returns 0 if successful, -1 on error.
 */
int f_ino_cap_rem(flist_p fl_p, int64_t f_ino, int32_t cap) {
	finfo_p f_p;
	struct inocap ic;
	ic.f_ino = f_ino;
	ic.cap   = cap;

	if (cap == -1) {
		int i=0;
		while ((f_p = llist_rem(fl_p, (void *) &f_ino, f_ino_cmp)) != NULL) i++;
		if (i) return(0);
		else return(-1);
	}
	
	f_p = (finfo_p) llist_rem(fl_p, (void *) &ic, f_ino_cap_cmp);
	if (f_p) {
		f_free(f_p);
		return(0);
	}
	return(-1);
}

void flist_cleanup(flist_p fl_p) {
	llist_free(fl_p, f_free);
}

static int f_sock_cmp(void *s_p, void *f_p) {
	int s;
	if (*(int *)s_p == (s = ((finfo_p)f_p)->sock)) return(0);
	else if (s == -1 && *(int *)s_p == ((finfo_p)f_p)->grp.sched) return(0);
	else return(1);

}

static int f_ino_cmp(void *i_p, void *f_p) {
	if (*(ino_t *)i_p == ((finfo_p)f_p)->f_ino) return(0);
	else return(1);
}

static int f_ino_cap_cmp(void *ic_p, void *f_p) {
	if (((struct inocap *)ic_p)->f_ino == ((finfo_p)f_p)->f_ino 
	&& ((struct inocap *)ic_p)->cap == ((finfo_p)f_p)->cap) return(0);
	else return(1);
}

/* f_free() - frees up data structures corresponding to open files
 * 
 * This function also ensures that the file is closed if still open
 * and any memory mapped region is unmapped
 */
static void f_free(void *v_p)
{
	finfo_p f_p = (finfo_p) v_p;
	if (f_p->grp.conn) free(f_p->grp.conn);
	if (f_p->fd >= 0) close(f_p->fd);
	if (f_p->mm.mloc != (char *) -1) munmap(f_p->mm.mloc, f_p->mm.msize);
	free(f_p);
}

finfo_p f_new(void)
{
	finfo_p f_p;

	if (!(f_p = (finfo_p) malloc(sizeof(finfo)))) return(0);

	f_p->f_ino = -1;
	f_p->cap   = -1;
	f_p->fd    = -1;
	f_p->sock  = -1;
	f_p->fsize = -1;
	f_p->pnum  = -1;
	f_p->mm.mloc = (char *)-1;
	f_p->grp.sched   = -1;
	f_p->grp.nr_conn = -1;
	f_p->grp.conn    = 0;

	return(f_p);
}

/* flist_purge_sock() - searches through all file info in a file list
 * and removes all files opened for a given socket (usually 0 or 1)
 *
 * returns 0 in all cases at the moment
 */
int flist_purge_sock(flist_p fl_p, int s)
{
	while (!f_sock_rem(fl_p, s));
	return(0);
}

int flist_dump(flist_p fl_p)
{
	return(llist_doall(fl_p, f_dump));
}

int f_dump(void *v_p)
{
	finfo_p f_p = (finfo_p) v_p; /* quick cast */

	printf("inode: %Ld\ncap: %d\nbase: %d\npcount: %d\nssize: %d\n"
		"soff: %d\nbsize: %ld\nfsize: %ld\npnum: %d\nmloc: %lx\n"
		"msize: %lu\nmoff: %lu\nmprot: %d\nnr_conn: %d\n",
		f_p->f_ino, f_p->cap, f_p->p_stat.base,
		f_p->p_stat.pcount, f_p->p_stat.ssize, f_p->p_stat.soff,
		(long int) f_p->p_stat.bsize, (long int) f_p->fsize,
		f_p->pnum, (unsigned long) f_p->mm.mloc,
		(unsigned long) f_p->mm.msize, (unsigned long) f_p->mm.moff,
		f_p->mm.mprot, f_p->grp.nr_conn);
	return(0);
}
