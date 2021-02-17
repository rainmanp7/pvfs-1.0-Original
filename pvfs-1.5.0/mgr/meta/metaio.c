/*
 * metaio.c copyright (c) 1998 Clemson University, all rights reserved.
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

/* METAIO - function calls used to access PVFS metadata files
 *
 * Currently there are six functions that may be used to access these
 * files: meta_creat(), meta_open(), meta_read(), meta_write(),
 * meta_unlink() and meta_close().  Only these functions should be used
 * when accessing/manipulating PVFS metadata files.
 *
 * NEW METADATA FILE FORMAT:
 * At location 0 in the file, the string "PVFS" will be stored, with no
 * null termination.  Immediately following this is a struct fmeta.
 *
 * NOTE: THIS HASN'T BEEN TURNED ON!!!
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* PVFS INCLUDES */
#include <meta.h>

extern int errno;
#ifdef USE_NEW_META_FORMAT
static char magic[] = "PVFS";
#define MAGIC_SZ 4
#else
#define MAGIC_SZ 0
#endif

/* META_OPEN() - opens a file as a PVFS metadata file and checks to make
 * sure that the file is indeed a PVFS metadata file
 *
 * Note: THIS IS NOT FOR CREATING NEW METADATA FILES!  Use meta_creat()
 * for that.  This will just give you an error.
 *
 * Returns a file descriptor (>=0) on success, -1 on failure.
 */
int meta_open(char *pathname, int flags)
{
	int fd;
	char buf[4];

	/* no creating files with this; use meta_creat() */
	flags &= ~O_CREAT;

	if ((fd = open(pathname, flags, 0)) < 0) return(-1);

#ifdef USE_NEW_META_FORMAT
	/* ensure this is a valid metadata file */
	if (read(fd, buf, MAGIC_SZ) < 0) {
		errno = EINVAL;
		return(-1);
	}
	if (bcmp(buf, magic, MAGIC_SZ)) {
		errno = EINVAL;
		return(-1);
	}
#endif

	/* looks good, return fd */
	return(fd);
} /* end of meta_open() */


/* META_CREAT() - creates a new PVFS metadata file
 */
int meta_creat(char *pathname, int flags)
{
	int fd, old_umask, cflags = O_RDWR | O_CREAT | O_EXCL,
	    mode = S_IRWXU | S_IRGRP | S_IROTH;

	/* create the file */
	old_umask = umask(0);
	fd = open(pathname, cflags, mode);
	umask(old_umask);
	if(fd < 0){
		return(-1);
	}

#ifdef USE_NEW_META_FORMAT
	/* write in the identifier */
	if (write(fd, magic, MAGIC_SZ) < MAGIC_SZ) return(-1);
#endif
	/* MAYBE WRITE IN DUMMY METADATA? */
	close(fd);

	/* reopen the new file using the specified flags */
	return(meta_open(pathname, flags));
} /* end of meta_creat() */


/* META_READ() - read the file metadata from a PVFS metadata file
 *
 * Returns the size of the fmeta structure if successful, or -1 on
 * error.
 */
int meta_read(int fd, struct fmeta *meta_p)
{
	/* seek past magic */
#ifdef USE_NEW_META_FORMAT
	if (lseek(fd, MAGIC_SZ, SEEK_SET) < 0) return(-1);
#else
	if (lseek(fd, 0, SEEK_SET) < 0) return(-1);
#endif
	
	return(read(fd, meta_p, sizeof(struct fmeta)));
} /* end of meta_read() */


/* META_WRITE() - write file metadata into a PVFS metadata file
 *
 * Returns the size of the fmeta structure if successful, or -1 on
 * error.
 */
int meta_write(int fd, struct fmeta *meta_p)
{
	/* seek past magic */
#ifdef USE_NEW_META_FORMAT
	if (lseek(fd, MAGIC_SZ, SEEK_SET) < 0) return(-1);
#else
	if (lseek(fd, 0, SEEK_SET) < 0) return(-1);
#endif

	return(write(fd, meta_p, sizeof(struct fmeta)));
} /* end of meta_write() */


/* META_CLOSE() - closes an open PVFS metadata file
 */
int meta_close(int fd)
{
	/* all we do right now is close the file... */
	return close(fd);
} /* end meta_close() */


/* META_UNLINK() - removes a PVFS metadata file
 */
int meta_unlink(char *pathname)
{
	int fd, flags = O_RDONLY;

	/* ensure this is a valid metadata file -- call meta_open() */
	if ((fd = meta_open(pathname, flags)) < 0) return(-1);
	meta_close(fd);

	/* looks good, try to unlink it */
	return unlink(pathname);
} /* end of meta_unlink() */

/*
 * GET_PARENT() - strips the child filename or directory from the
 * pathname and returns the new length.
 * This does cause side effects, pathname will be modified!
 */
int get_parent(char *pathname)
{
	int length;

	/* check for special case of "/" only */
	if (!strcmp(pathname, "/"))
		return(-1);
	/* strip off extra slashes */
	for (length=strlen(pathname)-1;length > 0 && pathname[length] == '/';
		length--);
	/* strip off file or directory name */
	for (;length >= 0 && pathname[length] != '/'; length--);
	/* strip off extra slashes */
	for (;length > 0 && pathname[length] == '/'; length--);
	switch (length) {
		case -1:	pathname[0] = '\0'; /* CWD used -- NULL parent */
					break;
		case  0:	strcpy(pathname, "/"); /* root directory */
					break;
		default: pathname[length+1] = '\0'; /* normal strip */
	}
	return(length);
}
/*
 *	META_ACCESS() - determines access capabilities for a given file and a
 *	given user. Returns 0 on success or -1 on failure and sets errno
 *	appropriate to the corresponding error.
 * If fd=0, meta_access will assume pathname points to a directory.
 * Values for the parameters are like that of access() except for fd
 * which does not exist in the access system call.
 */

int meta_access(int fd, char *pathname, uid_t uid, gid_t gid, int mode)
{
	char temp[MAXPATHLEN];
	int i,length;
	uid_t st_uid;
	gid_t st_gid;
	umode_t st_mode;
	fmeta metadat;
	dmeta dmetadat;

	/* check for valid values for parameters */
	if ((fd < 0) || (uid < 0) || (gid < 0) || (mode & 0xfffffff8)) {
		errno = EINVAL;
		return (-1);
	}
	i = check_pvfs(pathname);
	switch (i) {
		case 0:	return(0); /* nonpvfs, no need to check */
		case 1: 
		case 2:	break;
		default:
			errno = ENOENT;
			return(-1); /* file not found */
	}
	/* 
	 * determine if all directories in the path have execute permissions
	 * for the given user.  Could be recursive if the directory is a pvfs
	 * mounted file system directory
	 */
	strncpy(temp, pathname, MAXPATHLEN);
	length = get_parent(temp); /* strips to just parent directory */
	if (length >= 0) {
		i = check_pvfs(temp);
		switch (i) {
			/* nonpvfs, no need to check */
			case 0:	break;
			/* recursive call to pvfs directories */
			case 2:	if (meta_access(0, temp, uid, gid, X_OK) < 0)
							return(-1);
						break;
			/* if its a pvfs file or an error, this is incorrect */
			default:	errno = ENOTDIR;
						return(-1);
		}
	}
	/* get file metadata */
	if ((fd != 0) && ((i = meta_read(fd, &metadat)) != sizeof(fmeta)) 
		&& (errno != EISDIR)) {
		perror("meta_access: meta_read");
		return (-1);
	}
	/* directory? */
	if ((fd == 0) || (i < 0)) {
		/* get directory metadata */
		if (get_dmeta(pathname, &dmetadat) < 0)
				return(-1);
		st_uid  = dmetadat.dr_uid;
		st_gid  = dmetadat.dr_gid;
		st_mode = dmetadat.dr_mode;
	}
	else { /* yep, it's a meta file */
		/* seek back to beginning of file for future reads */
		if (lseek(fd, 0, SEEK_SET) < 0) return(-1);
		st_uid  = metadat.u_stat.st_uid;
		st_gid  = metadat.u_stat.st_gid;
		st_mode = metadat.u_stat.st_mode;
	}
	/* give root permission, no matter what */
	if (uid == 0)
		return(0);
	/* uid test */
	if ((uid == st_uid) && (((st_mode >> 6) & mode) == mode))
		return(0);
	/* gid test */ 
	if ((gid == st_gid) && (((st_mode >> 3) & mode) == mode))
		return(0);
	/* check for other permissions */
	if ((mode & st_mode) == mode)
		return(0); /* success */

	/* access denied */
	errno = EACCES;
	return(-1);
}
