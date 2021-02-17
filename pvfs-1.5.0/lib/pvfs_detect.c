/*
 * pvfs_detect.c copyright (c) 1999 Clemson University, all rights reserved.
 *
 * The original canonicalize function was part of the GNU C Library.  It
 * has been modified from its original form.  Modifications and
 * additional code written by Rob Ross.
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
 * Contact:  Rob Ross <rbross@parl.clemson.edu>
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <mntent.h>

#include <lib.h> /* bunches of defines, including the PVFS ones */

static char canonicalize_outbuf[MAXPATHLEN];

static char *canonicalize (const char *name);
struct mntent *search_fstab(char *dir);

/* pvfs_detect() - given a file name, attempts to determine if the given
 * file is a PVFS one
 *
 * Returns -1 on error.
 * Returns 0 if file name does not appear to refer to a PVFS file.
 * Returns 1 if file name refers to an existing PVFS file OR if it
 *   refers to a file that does not yet exist but that would exist
 *   on a PVFS file system.
 * Returns 2 if file name refers to an existing PVFS directory.
 *
 * IF the name refers to a PVFS file, we return a pointer to our static
 * region holding the canonicalized file name.  This isn't too
 * thread-safe <smile>, but neither are a lot of other things in this
 * version of the library...
 */
int pvfs_detect(const char *fn, char **sname_p, struct sockaddr **saddr_p,
int64_t *fs_ino_p)
{
	struct mntent *ent;
	int len, s;
	char *remainder, *fsdir;
	mreq req;
	mack ack;
	static struct sockaddr saddr;
	static char snambuf[MAXPATHLEN+1];
	u_int16_t port;
	char host[1024];

	/* canonicalize the filename, result in canonicalize_outbuf */
	if (!canonicalize(fn)) return(-1);

	LOG1("canonicalize: %s\n", canonicalize_outbuf);

	/* check for pvfs fstab match, return 0 if no match */
	if (!(ent = search_fstab(canonicalize_outbuf))) {
		LOG("search_fstab didn't find a match\n");
		return(0);
	}

	/* piece together the filename at the server end
	 *
	 * Steps:
	 * 1) skip over hostname and ':' in fsname
	 * 2) replace directory name in filename with one on server
	 *
	 * Assumption: unless mnt_dir refers to the root directory, there
	 * will be no trailing /.
	 */
	if (!(fsdir = strchr(ent->mnt_fsname, ':'))) return(-1);
	fsdir++;
	len = strlen(ent->mnt_dir);
	remainder = canonicalize_outbuf + len;
	snprintf(snambuf, MAXPATHLEN, "%s/%s", fsdir, remainder);
	LOG1("pvfs name: %s\n", snambuf);

	/* ideally we would like to hit the manager here to see if this is a
	 * directory or a file, but we don't want to use stat because it
	 * causes traffic to the iods.
	 *
	 * So instead we do an access call, which now returns the file stats
	 * too (except for size).  This way we can check to see if we're
	 * looking at a file or directory.
	 */
	req.uid = getuid();
	req.gid = getgid();
	req.type = MGR_ACCESS;
	req.dsize = strlen(snambuf); /* null terminated at other end */
	req.req.access.fname = 0;
	req.req.access.mode = F_OK;
	ack.majik_nr = 0;

	/* piece together host and port for server */
	if (!(remainder = strchr(ent->mnt_fsname, ':'))) return(-1);
	if (!(len = remainder - ent->mnt_fsname)) return(-1);
	strncpy(host, ent->mnt_fsname, len);
	*(host + len) = '\0';

	if (!(remainder = strstr(ent->mnt_opts, "port="))) return(-1);
	remainder += 5; /* strlen("port=") */
	port = atoi(remainder);

	/* send the request; don't return an error just because we get -1
	 * back from send_mreq_saddr; file might have just not existed
	 */
	if (init_sock(&saddr, host, port)) return(-1);
	s = send_mreq_saddr(&saddr, &req, snambuf, &ack);
	if (ack.majik_nr != MGR_MAJIK_NR) return(-1);

	*saddr_p = &saddr;
	*sname_p = snambuf;
	*fs_ino_p = ack.ack.access.meta.fs_ino;

	if (ack.eno == ENOENT) {
		return(1);
	}
	if (S_ISDIR(ack.ack.access.meta.u_stat.st_mode)) return(2);

	/* fill in dmeta structure */
	return(1);
}

/* MODIFIED CANONICALIZE FUNCTION FROM GLIBC BELOW */

/* Return the canonical absolute name of file NAME.	A canonical name
	does not contain any `.', `..' components nor any repeated path
	separators ('/') or symlinks.

	Path components need not exist.	If they don't, it will be assumed
	that they are not symlinks.	This is necessary for our work with
	PVFS.

	Output is returned in canonicalize_outbuf.

	If RESOLVED is null, the result is malloc'd; otherwise, if the
	canonical name is PATH_MAX chars or more, returns null with `errno'
	set to ENAMETOOLONG; if the name fits in fewer than PATH_MAX chars,
	returns the name in RESOLVED.	If the name cannot be resolved and
	RESOLVED is non-NULL, it contains the path of the first component
	that cannot be resolved.	If the path can be resolved, RESOLVED
	holds the same value as the value returned.
 */

#define __set_errno(x) errno = (x)
#define __alloca alloca
#define __getcwd getcwd
#define __readlink readlink
#define __lxstat(x,y,z) unix_lstat((y),(z))

static char *canonicalize (const char *name)
{
	char *rpath, *dest, *extra_buf = NULL;
	const char *start, *end, *rpath_limit;
	long int path_max;
	int num_links = 0, err = 0;

	if (name == NULL) {
		/* As per Single Unix Specification V2 we must return an error if
			 either parameter is a null pointer.	We extend this to allow
			 the RESOLVED parameter be NULL in case the we are expected to
			 allocate the room for the return value.	*/
		__set_errno (EINVAL);
		return NULL;
	}

	if (name[0] == '\0') {
		/* As per Single Unix Specification V2 we must return an error if
			 the name argument points to an empty string.	*/
		__set_errno (ENOENT);
		return NULL;
	}

#ifdef PATH_MAX
	path_max = PATH_MAX;
#else
	path_max = pathconf (name, _PC_PATH_MAX);
	if (path_max <= 0)
		path_max = 1024;
#endif

	rpath = __alloca (path_max);
	rpath_limit = rpath + path_max;

	if (name[0] != '/') {
		if (!__getcwd (rpath, path_max))
			goto error;
		dest = strchr (rpath, '\0');
	}
	else {
		rpath[0] = '/';
		dest = rpath + 1;
	}

	for (start = end = name; *start; start = end) {
		struct stat st;
		int n;

		/* Skip sequence of multiple path-separators.	*/
		while (*start == '/')
			++start;

		/* Find end of path component.	*/
		for (end = start; *end && *end != '/'; ++end)
			/* Nothing.	*/;

		if (end - start == 0)
			break;
		else if (end - start == 1 && start[0] == '.')
			/* nothing */;
		else if (end - start == 2 && start[0] == '.' && start[1] == '.') {
			/* Back up to previous component, ignore if at root already.	*/
			if (dest > rpath + 1)
				while ((--dest)[-1] != '/');
		}
		else {
			size_t new_size;

			if (dest[-1] != '/')
				*dest++ = '/';

			if (dest + (end - start) >= rpath_limit) {
				ptrdiff_t dest_offset = dest - rpath;

				__set_errno (ENAMETOOLONG);
				goto error;
			}

			dest = (char *)__mempcpy (dest, start, end - start);
			*dest = '\0';

			/* we used to crap out in this case; now we simply note that we
			 * hit an error and stop trying to stat from now on. -- Rob
			 */
			if (!err && __lxstat (_STAT_VER, rpath, &st) < 0) {
				err++;
			}
			if (!err && (S_ISLNK (st.st_mode))) {
				char *buf = __alloca (path_max);
				size_t len;

				if (++num_links > MAXSYMLINKS) {
					__set_errno (ELOOP);
					goto error;
				}

				n = __readlink (rpath, buf, path_max);
				if (n < 0)
					goto error;
				buf[n] = '\0';

				if (!extra_buf)
					extra_buf = __alloca (path_max);

				len = strlen (end);
				if ((long int) (n + len) >= path_max) {
					__set_errno (ENAMETOOLONG);
					goto error;
				}

				/* Careful here, end may be a pointer into extra_buf... */
				memmove (&extra_buf[n], end, len + 1);
				name = end = memcpy (extra_buf, buf, n);

				if (buf[0] == '/')
					dest = rpath + 1;	/* It's an absolute symlink */
				else
					/* Back up to previous component, ignore if at root already: */
					if (dest > rpath + 1)
						while ((--dest)[-1] != '/');
			}
		}
	}
	if (dest > rpath + 1 && dest[-1] == '/')
		--dest;
	*dest = '\0';

	memcpy(canonicalize_outbuf, rpath, dest - rpath + 1);
	return(canonicalize_outbuf);

error:
	/* copy in component causing trouble */
	strcpy (canonicalize_outbuf, rpath);
	return(canonicalize_outbuf);
}


