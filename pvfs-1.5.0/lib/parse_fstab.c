/*
 * parse_fstab.c copyright (c) 1999 Clemson University, all rights reserved.
 *
 * Written by Rob Ross.
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

#include <mntent.h>
#include <string.h>
#include <llist.h>
#include <debug.h>
#include <pvfs_config.h>

typedef llist_p fstab_list_p;

extern int pvfs_checks_disabled;

static fstab_list_p pvfstab_p=0;

static int add_fstab_entry(fstab_list_p tab_p, struct mntent *ent);
static fstab_list_p fstab_list_new(void);
void free_fstab_entry(void *e_p);
static void dump_fstab_entry(void *e_p);
static int fstab_entry_dir_cmp(void *key, void *e_p);
struct mntent *search_fstab(char *dir);

int parse_fstab(char *fn)
{
	FILE *tab;
	struct mntent *ent;

	if (pvfstab_p) return(0); /* only need to do this once */
	if (!(pvfstab_p = fstab_list_new())) return(-1);

	pvfs_checks_disabled = 1;
	if (!(tab = setmntent(fn, "r"))) {
		perror("parse_fstab (opening pvfstab file)");
		pvfs_checks_disabled = 0;
		return(-1);
	}

	while (ent = getmntent(tab)) {
		if (strcmp(ent->mnt_type, "pvfs")) continue; /* not PVFS */
		LOG1("parse_fstab: added entry %s\n", ent->mnt_dir);
		add_fstab_entry(pvfstab_p, ent);
	}
	endmntent(tab);
	pvfs_checks_disabled = 0;
	return(0);
}

/* add_fstab_entry() - add a mntent entry into the fstab list
 *
 * Function first removes trailing /s from fsname and dir, leaving a
 * single / in the fsname if the root directory is indicated.
 *
 * Next it allocates space for a new copy of the entry, using a single
 * malloc to allocate space for the mntent structure and all strings.
 *
 * Next it copies the mntent structure and all strings into the
 * allocated space and updates the appropriate pointers.
 *
 * Finally it adds this new structure into our list.
 *
 * Returns 0 on success, -1 on failure.
 */
static int add_fstab_entry(fstab_list_p tab_p, struct mntent *ent)
{
	int sz_f, sz_d, sz_t, sz_o, ret;
	struct mntent *newent;
	char *ptr;

	/* perform cleanup on the entry (remove extra /s on fs and dir) */
	ptr = strchr(ent->mnt_fsname, '\0');
	while (*(--ptr)=='/' && ptr > ent->mnt_fsname && *(ptr-1)!=':') *ptr='\0';
	ptr = strchr(ent->mnt_dir, '\0');
	while (*(--ptr) == '/' && ptr > ent->mnt_dir) *ptr='\0';

	/* grab some space to hold the entry (add up string sizes) */
	sz_f = strlen(ent->mnt_fsname) + 1;
	sz_d = strlen(ent->mnt_dir) + 1;
	sz_t = strlen(ent->mnt_type) + 1;
	sz_o = strlen(ent->mnt_opts) + 1;
	if (!(newent = (struct mntent *)
		malloc(sizeof(struct mntent)+sz_f+sz_d+sz_t+sz_o))) return(-1);

	/* copy everything into the newly allocated space */
	memcpy(newent, ent, sizeof(struct mntent));
	ptr = (char *)newent + sizeof(struct mntent);
	newent->mnt_fsname = ptr;
	memcpy(ptr, ent->mnt_fsname, sz_f);
	ptr += sz_f;
	newent->mnt_dir = ptr;
	memcpy(ptr, ent->mnt_dir, sz_d);
	ptr += sz_d;
	newent->mnt_type = ptr;
	memcpy(ptr, ent->mnt_type, sz_t);
	ptr += sz_t;
	newent->mnt_opts = ptr;
	memcpy(ptr, ent->mnt_opts, sz_o);

	dump_fstab_entry(newent);

	/* add into the list, free on error */
	if ((ret = llist_add_to_tail(tab_p, (void *) newent)) < 0) {
		/* free up space */
		free(newent);
		return(ret);
	}
	return(0);
}

static fstab_list_p fstab_list_new(void)
{
	return((fstab_list_p) llist_new());
}

void free_fstab_entry(void *e_p)
{
	free(e_p);
	return;
}

static void dump_fstab_entry(void *e_p)
{
	struct mntent *ent = (struct mntent *)e_p;
	LOG3("fsname: %s, dir: %s, opts: %s\n", ent->mnt_fsname,
		ent->mnt_dir, ent->mnt_opts);
	return;
}

/* fstab_entry_dir_cmp() - find a matching entry for a given file name
 *
 * ASSUMPTIONS:
 * - mnt_dir entries never have trailing slashes unless root is
 *   specified (which should never be)
 *
 * Returns 0 on match, non-zero if no match.
 */
static int fstab_entry_dir_cmp(void *key, void *e_p)
{
	int sz, cmp;
	struct mntent *me_p = (struct mntent *)e_p;

	LOG2("cmp: %s, %s\n", (char *)key, me_p->mnt_dir);

	/* get length of directory (not including terminator) */
	sz = strlen(me_p->mnt_dir);

	/* compare dir to first part of key, drop out if no match */
	cmp = strncmp((char *)key, me_p->mnt_dir, sz);
	if (cmp) return(cmp);

	/* make sure that next character in key is a / or \0 */
	(char *)key += sz;
	if (*(char *)key == '/' || *(char *)key == '\0') return(0);
	return(-1);
}

struct mntent *search_fstab(char *dir)
{
	char* path_to_pvfstab = NULL;

	if (!pvfstab_p){
		if((path_to_pvfstab = getenv(PVFSTAB_ENV)) == NULL){
			parse_fstab(PVFSTAB_PATH);
		}
		else{
			parse_fstab(path_to_pvfstab);
		}
	}
	return((struct mntent *) llist_search(pvfstab_p, (void *) dir,
		fstab_entry_dir_cmp));
}











