/* pvfs_ioctl.c copyright (c) 1996 Rob Ross, all rights reserved.
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
	Contact:  Rob Ross  	rbross@parl.eng.clemson.edu
				 Matt Cettei mcettei@parl.eng.clemson.edu

 */

/* Notes:
 * - Currently, SETPART is not implemented for normal files.
 * - SETPART resets the file offset to 0
 *
 */

static char pvfs_ioctl_c[] = "$Header: /projects/cvsroot/pvfs/lib/pvfs_ioctl.c,v 1.3 2000/10/30 14:44:49 rbross Exp $";

/* UNIX INCLUDES */
#include <sys/ioctl.h>
#include <errno.h>

/* PVFS INCLUDES */
#include <lib.h>

/* GLOBALS/EXTERNS */
extern fdesc_p pfds[];
extern int errno;

int pvfs_ioctl(int fd, int cmd, void *data)
{
	fpart     p_part={0,0,0,0,0};     /* partition */

	if (fd < 0 || fd >= NR_OPEN || (pfds[fd] && pfds[fd]->fs == FS_RESV)) {
		errno = EBADF;
		return(-1);
	}

	if (!pfds[fd]) return ioctl(fd, cmd, data);

	switch(cmd) {
		case GETPART:
			if (pfds[fd]->part_p) *(fpart_p)data = *pfds[fd]->part_p;
			else *(fpart_p)data = p_part;
			return(0);
		case SETPART:
			if (!pfds[fd]->part_p) {
				if (!(pfds[fd]->part_p = malloc(sizeof(fpart)))) {
					PERROR("pvfs_ioctl: malloc");
					return(-1);
				}
			}
			*pfds[fd]->part_p = *(fpart_p)data;
			pfds[fd]->fd.off = 0; /* reset file pointer */
			return(0);
		case GETMETA:
			*(pvfs_filestat_p)data = pfds[fd]->fd.meta.p_stat;
			return(0);
		default:
			if (pfds[fd]->fs==FS_UNIX || pfds[fd]->fs==FS_PDIR) {
				return ioctl(fd, cmd, data);
			}
			ERR1("pvfs_ioctl: command %x not implemented\n", cmd);
			errno = EINVAL;
			return(-1);
	}
}
