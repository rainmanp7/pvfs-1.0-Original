/*
 * get_dmeta.c copyright (c) 1997 Clemson University, all rights reserved.
 *
 * Written by Matt Cettei and Walt Ligon.
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
 *           Walt Ligon  walt@parl.clemson.edu
 */

static char get_dmeta_c[] = "$Header: /projects/cvsroot/pvfs/mgr/meta/get_dmeta.c,v 1.4 2000/10/30 14:51:58 rbross Exp $";

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <meta.h>
#include <sys/param.h>

#define TMPSZ 512

/* GLOBALS */
extern int errno;

char *dmeta2fname(struct dmeta *d_p);
int get_dmeta(char * fname, dmeta_p dir);

/* get_dmeta() - grabs directory metadata in the directory of a given
 * file
 *
 * NOTE: metadata is only valid until next call to get_dmeta()
 */
int get_dmeta(char * fname, dmeta_p dir)
{
	int fd, len, is_dir = 0, use_cwd = 0, rd_len;
	struct stat filestat;
	char nbuf[MAXPATHLEN];
	static char dmbuf[4096];
	struct dmeta *d_p;

	strncpy(nbuf, fname, MAXPATHLEN);

	/* strip off any trailing /'s on the file name */
	for (len = strlen(nbuf); len > 0 && nbuf[len] == '/'; nbuf[len--] = '\0');
	if (len < 0) return(-1);

	/* determine if name refers to a directory or file, find appropriate
	 * directory name
	 */
	if (stat(nbuf, &filestat) < 0 || !S_ISDIR(filestat.st_mode)) {
		/* need to strip off trailing name, pass the rest to dmetaio
		 * calls so we get the dir. metadata for the parent directory
		 *
		 * we don't want to wax the last bit of the name, because we need
		 * it later...
		 */
		for (/* len already set */; len >= 0 && nbuf[len] != '/'; len--);
		if (len <= 0) /* no directory; use current working directory */ {
			/* should not ever happen as of v1.4.2 and later */
			use_cwd = 1;
		}
		else /* the fname wasn't just a file name */ {
			nbuf[len] = '\0';
		}
	}
	else /* it's a directory */ {
		is_dir = 1;
	}
	
	/* open the dir. metadata file, read it, get the info */
	if (use_cwd) {
		fprintf(stderr,
		"use current working directory!?!?!  shouldn't be here!!!\n");
		return -1;
	}
	else if ((fd = dmeta_open(nbuf, O_RDONLY)) < 0) {
		perror("dmeta_open");
		return(-1);
	}
	if (dmeta_read(fd, dmbuf, 4096) < 0) {
		perror("dmeta_read");
		return(-1);
	}
	dmeta_close(fd);

	/* fill in the filename part of the dmeta structure */
	/* this is a big hack to keep someone's idiot code from breaking */
	d_p = (struct dmeta *) dmbuf;

	/* WORKAROUND FOR SUBDIRECTORY PROBLEM:
	 * Here we need to substitute the subdirectory that was passed in for
	 * the one that is in the directory metadata.  This is to work around
	 * the problem of recursive renames changing the actual subdirectory
	 * without updating the metadata.
	 *
	 * In actuality, we're moving more and more away from this root
	 * directory, subdirectory, file name nonsense, but for the moment we
	 * need to keep things up to date.  The next generation PVFS will not
	 * have any of this.
	 *
	 * We assume that the root directory is ok in the dmeta structure and
	 * that the sd_path pointer will point past the rd_path area.  We
	 * then rewrite the sd_path and the fname and reset the fname
	 * pointer.
	 */
	rd_len = strlen(d_p->rd_path)+1;
	strcpy(d_p->sd_path, &nbuf[rd_len]);
	d_p->fname = d_p->sd_path + strlen(d_p->sd_path)+1;

	if (is_dir) {
		*d_p->fname = '\0';
	}
	else {
		strcpy(d_p->fname, &nbuf[len+1]); /* could check len... */
	}

	/* copy the dmeta structure to the specified location */
	*dir = *(struct dmeta *)dmbuf;

	return(0);
}

/* DMETA2FNAME() - returns pointer to the filename stored in parts in
 * the dmeta structure
 *
 * There are numerous spots in the code where this is done in an
 * inappropriate manner; this function should be used in all those
 * places <smile>!
 *
 * Returns a pointer to a region where the file name is stored; this
 * region is overwritten the next time this call is used.
 */
char *dmeta2fname(struct dmeta *d_p)
{
	static char buf[MAXPATHLEN+2];
	
	if (!d_p) {
		errno = EINVAL;
		return(0);
	}

	strcpy(buf, d_p->rd_path);
	strcat(buf, "/");
	strcat(buf, d_p->sd_path);
	strcat(buf, "/");
	strcat(buf, d_p->fname);

	return(buf);
}
