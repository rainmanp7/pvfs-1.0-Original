/*
 * metaio.h copyright (c) 1998 Clemson University, all rights reserved.
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
 * Contact:  Rob Ross    rbross@parl.eng.clemson.edu
 */

int meta_open(char *pathname, int flags);
int meta_creat(char *pathname, int flags);
int meta_read(int fd, struct fmeta *meta_p);
int meta_write(int fd, struct fmeta *meta_p);
int meta_close(int fd);
int meta_unlink(char *pathname);
int meta_access(int fd, char *pathname, uid_t uid, gid_t gid, int mode);
int get_parent(char *pathname); /* this should go somewhere else */
