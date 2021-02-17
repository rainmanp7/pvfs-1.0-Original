/*
 * fslist.c copyright (c) 1999 Clemson University, all rights reserved.
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
 * FSLIST.C - functions to handle the creation and modification of 
 * lists of filesystem information
 *
 * This is built on top of the llist.[ch] files
 *
 */

#include <stdlib.h>
#include <fslist.h>
#include <flist.h>
#include <debug.h>

/* prototypes for internal functions */
static int fs_ino_cmp(void *key, void *fs_p);
static void fs_free(void * fs_p);

fslist_p fslist_new(void)
{
	return(llist_new());
}

int fs_add(fslist_p fsl_p, fsinfo_p fs_p)
{
	fs_p->fl_p = flist_new(); /* get new file list */
	return(llist_add(fsl_p, (void *) fs_p));
}

fsinfo_p fs_search(fslist_p fsl_p, ino_t fs_ino)
{
	return((fsinfo_p) llist_search(fsl_p, (void *) (&fs_ino), fs_ino_cmp));
}

int fs_rem(fslist_p fsl_p, ino_t fs_ino)
{
	fsinfo_p fs_p;
	
	fs_p = (fsinfo_p) llist_rem(fsl_p, (void *) (&fs_ino), fs_ino_cmp);
	if (fs_p) {
		fs_free(fs_p);
		return(0);
	}
	return(-1);
}

int forall_fs(fslist_p fsl_p, int (*fn)(void *))
{
	return(llist_doall(fsl_p, fn));
}

int fslist_dump(fslist_p fsl_p)
{
	return(forall_fs(fsl_p, fs_dump));
}

int fs_dump(void *v_p)
{
	fsinfo_p fs_p = (fsinfo_p) v_p;

	LOG2("fs_ino: %ld\nnr_iods: %d\n", fs_p->fs_ino, fs_p->nr_iods);
	flist_dump(fs_p->fl_p);
	return(0);
}

void fslist_cleanup(fslist_p fsl_p)
{
	llist_free(fsl_p, fs_free);
}

static int fs_ino_cmp(void *key, void *fs_p)
{
	if (fs_p && (ino_t) (*(ino_t *)key) == ((fsinfo_p) fs_p)->fs_ino) return(0);
	else return(1);
}

static void fs_free(void * fs_p)
{
	flist_cleanup(((fsinfo_p) fs_p)->fl_p);
	free((fsinfo_p) fs_p);
}
