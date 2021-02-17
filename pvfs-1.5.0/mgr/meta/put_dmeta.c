/*
 * put_dmeta.c copyright (c) 1996 Clemson University, all rights reserved.
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

static char put_dmeta_c[] = "$Header: /projects/cvsroot/pvfs/mgr/meta/put_dmeta.c,v 1.7 2000/10/30 14:51:58 rbross Exp $";

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <linux/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <meta.h>

/* GLOBALS */
extern int errno;

/* put_dmeta()
 *
 * This function now assumes that it is passed a directory name.  It
 * appends "/.pvfsdir" to this name and uses that as the pvfsdir
 * filename.
 *
 * When writing out the metadata, it relies on the fname passed in
 * having the same root as the one in the dir structure.
 */
int put_dmeta(char * fname, dmeta_p dir)
{
	int i, fd, length, dlength;
	char temp[MAXPATHLEN], *subdir;
	FILE *fp;
	mode_t old_umask;

   /* build name of dotfile */
   strncpy(temp, fname, MAXPATHLEN-1);
   length = strlen(temp);

   /* open dotfile */
	if (length + 9 >= MAXPATHLEN-1) {
		return(-1);
	}
   strcat(temp, "/.pvfsdir");
   old_umask = umask(0033);

	subdir = fname + strlen(dir->rd_path) + 1;

	/* should use the dmeta_xxx calls */
   if ((fp = fopen(temp, "w+")) == 0) {
      perror("Error opening dot file");
		umask(old_umask);
      return(-1);
   }

   /* write dotfile */
   fprintf(fp,"%Ld\n%d\n%d\n%07o\n%d\n%s\n%s\n%s\n\n",dir->fs_ino,
	dir->dr_uid,dir->dr_gid,dir->dr_mode,dir->port,dir->host,
	dir->rd_path,subdir);
   fclose(fp);
	umask(old_umask);

   /* return data */
   return 0;
}
