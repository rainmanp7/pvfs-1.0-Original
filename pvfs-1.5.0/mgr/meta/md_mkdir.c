/* md_mkdir.c copyright (c) 1996 Clemson University, all rights reserved.
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
 * 
 */

static char md_mkdir_c[] = "$Header: /projects/cvsroot/pvfs/mgr/meta/md_mkdir.c,v 1.4 2000/02/01 16:49:12 rbross Exp $";
/*This file contains the chown metadata file call for the PVFS*/
/*Assumes that space for request has been allocated. */

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <linux/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <meta.h>
#include <req.h>

extern int errno;

/* MD_MKDIR() - given a pathname to a directory and a dmeta structure
 * describing the directory, creates the metadata files needed in the
 * directory (currently only the .pvfsdir file).
 *
 * This function used to modify some of the structures in the dmeta_p
 * structure, but we don't do that any more.
 *
 * This function does not assume that the dirpath has a trailing /.
 *
 * put_dmeta() does ALMOST the same thing as this function, but doesn't
 * understand that the fname field should be used as part of the name
 * when we're making a new .pvfsdir file.  So this should always be used
 * when we're making a new file...
 *
 * Returns 0 on success, -1 on error.
 */
int md_mkdir(char *dirpath, dmeta_p dir)
{
	FILE *fp;
	char temp[MAXPATHLEN];

	/* create dotfile */
	strncpy(temp, dirpath, MAXPATHLEN);
	strcat(temp,"/.pvfsdir");
	umask(0033);
	if ((fp = fopen(temp, "w+")) == 0) {
		perror("Error creating dot file");
		return(-1);
	}

	/* write dotfile */
	fprintf(fp, "%Ld\n%d\n%d\n", dir->fs_ino, dir->dr_uid, dir->dr_gid);
	fprintf(fp, "%07o\n%d\n%s\n", dir->dr_mode, dir->port, dir->host);
	fprintf(fp, "%s\n%s/%s\n", dir->rd_path, dir->sd_path, dir->fname);
	fclose(fp);

	/* return success */
	return(0);

}
