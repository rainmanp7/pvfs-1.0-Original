/* build_job.c copyright (c) 1996 Rob Ross, all rights reserved.
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
	Contact:  Rob Ross  rbross@parl.eng.clemson.edu  or
				 Matt Cettei mcettei@parl.eng.clemson.edu

 */


static char build_job_c[] = "$Header: /projects/cvsroot/pvfs/lib/build_job.c,v 1.7 2000/11/22 16:06:05 rbross Exp $";

/* UNIX INCLUDES */
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>

/* PVFS INCLUDES */
#include <build_job.h>
#include <desc.h>
#include <req.h>
#include <jlist.h>
#include <alist.h>
#include <minmax.h>
#include <sockset.h>
#include <debug.h>
#include <sockio.h>
#include <dfd_set.h>

/* GLOBALS */
jlist_p active_p = NULL;
sockset socks;

/* DEFINES */
#define REQTYPE  r_p->req.rw.rw_type
#define OFFSET r_p->req.rw.off
#define FSIZE  r_p->req.rw.fsize
#define GSIZE  r_p->req.rw.gsize
#define LSIZE  r_p->req.rw.lsize
#define GCOUNT r_p->req.rw.gcount
#define STRIDE r_p->req.rw.stride

#define PCOUNT ((int64_t)(f_p->fd.meta.p_stat.pcount))

/* FUNCTIONS */

/* BUILD_SIMPLE_JOBS() - builds a set of jobs to send a request
 * to the iods for a file and receive the acks into the iod ack fields
 * for the file
 *
 * Returns 0 on success, -1 on failure
 */
int build_simple_jobs(fdesc_p f_p, ireq_p r_p)
{
	int i;

	if (!active_p) active_p = jlist_new();

	initset(&socks); /* clear out socket set */

	for (i=0; i < PCOUNT; i++) {
		jinfo_p j_p;
		ainfo_p a_p;
		ireq_p  tmpr_p;
		int s;

		f_p->fd.iod[i].ack.status = 0;
		f_p->fd.iod[i].ack.dsize  = 0;
		
		if ((s = f_p->fd.iod[i].sock) < 0) /* need to connect to iod */ {
			if ((s = new_sock()) < 0) return(-1);
build_simple_jobs_connect:
			if (connect(s, (struct sockaddr *) &(f_p->fd.iod[i].addr),
				sizeof(struct sockaddr_in)) < 0) {
				if (errno == EINTR) goto build_simple_jobs_connect;
				if (errno == EAGAIN) {
					sleep(2);
					goto build_simple_jobs_connect;
				}
				PERROR("build_simple_jobs: connect");
				return(-1);
			}

			/* success */
			f_p->fd.iod[i].sock = s;

			/* don't check returns; we don't care if this fails */
			(void) set_sockopt(s, SO_SNDBUF, CLIENT_SOCKET_BUFFER_SIZE);
			(void) set_sockopt(s, SO_RCVBUF, CLIENT_SOCKET_BUFFER_SIZE);
		}
		if (!(j_p = j_new(PCOUNT))) {
			return(-1);
		}
		dfd_set(s, &j_p->socks);
		if (s >= j_p->max_nr) j_p->max_nr = s+1;

		/* add request */
		if (!(a_p = (ainfo_p) malloc(sizeof(ainfo)))) {
			return(-1);
		}
		if (!(tmpr_p = (ireq_p) malloc(sizeof(ireq)))) {
			return(-1);
		}
		*tmpr_p = *r_p;
		a_p->type = A_REQ;
		a_p->sock = s;
		a_p->u.req.size = sizeof(ireq);
		a_p->u.req.cur_p = a_p->u.req.req_p = (char *) tmpr_p;
		if (a_add_start(j_p->al_p, a_p) < 0) {
			return(-1);
		}

		/* add ack */
		if (!(a_p = (ainfo_p) malloc(sizeof(ainfo)))) {
			return(-1);
		}
		a_p->type = A_ACK;
		a_p->sock = s;
		a_p->u.ack.iod_nr = i;
		a_p->u.ack.size = sizeof(iack);
		a_p->u.ack.cur_p = a_p->u.ack.ack_p = (char *) &f_p->fd.iod[i].ack;
		if (a_add_end(j_p->al_p, a_p) < 0) {
			return(-1);
		}

		/* add job to list */
		if (j_add(active_p, j_p) < 0) /* error adding job to list */ {
			alist_cleanup(j_p->al_p);
			free(j_p);
			return(-1);
		}
		addsockwr(s, &socks);
	}
	return(0);
}


/* BUILD_RW_JOBS() - construct jobs to fulfill an I/O request
 * PARAMETERS:
 *    f_p   - pointer to file descriptor
 *    buf_p - pointer to buffer to be sent or to receive into
 *    size  - size of request (in bytes)
 *    type  - type of jlist request
 *
 * Returns -1 on error, 0 on success?
 */
int build_rw_jobs(fdesc_p f_p, char *buf_p, int64_t size, int type)
{
	ireq    req;
	ireq_p  r_p = &req; /* nice way to use the same defines throughout */
	ainfo_p a_p;
	jinfo_p j_p;
	int i, atype, sock, myerr;
	int64_t offset, gcount;

	/* build the requests */
	if (!active_p) active_p = jlist_new();
	LOG3("build_jobs: a = %lx, s = %Ld, t = %d\n", buf_p, size, type);

	build_rw_req(f_p, size, &req, type, 0); /* last # not currently used */
	LOG("  build_jobs: done building request\n");

	/* sooner or later this will matter... */
	switch (type) {
		case J_READ:
			atype = A_READ; 
			break;
		case J_WRITE:
		default:
			atype = A_WRITE;
			break;
	}

	/* add the accesses */
	offset = r_p->req.rw.off;
	gcount = r_p->req.rw.gcount;
	if (FSIZE) {
		if ((buf_p = add_accesses(f_p, offset, FSIZE, buf_p, atype)) == NULL) {
			return -1;
		}
		offset += FSIZE + (STRIDE - GSIZE);
	}
	while (gcount-- > 0) {
		if ((buf_p = add_accesses(f_p, offset, GSIZE, buf_p, atype)) == NULL) {
			return -1;
		}
		offset += STRIDE;
	}
	if (LSIZE) {
		if ((buf_p = add_accesses(f_p, offset, LSIZE, buf_p, atype)) == NULL) {
			return -1;
		}
	}

	LOG("  build_jobs: done calling add_accesses\n");

	initset(&socks); /* clear any other jobs out */
	for (i = 0; i < PCOUNT; i++) {
		sock = f_p->fd.iod[i].sock;
		if (sock >= 0 && (j_p = j_search(active_p, sock)) != NULL) {
			/* add reception of ack */
			if ((a_p = (ainfo_p) malloc(sizeof(ainfo))) == NULL) {
				PERROR("build_jobs: malloc (ainfo1)");
				return(-1);
			}
			a_p->type = A_ACK;
			a_p->sock = sock;
			a_p->u.ack.size = sizeof(iack);
			a_p->u.ack.ack_p = a_p->u.ack.cur_p =
				(char *)&(f_p->fd.iod[i].ack);
			a_p->u.ack.iod_nr = i;

			switch(type) {
				case J_READ:
					if (a_add_start(j_p->al_p, a_p) < 0) {
						myerr = errno;
						ERR("build_jobs: a_add_start failed (ack)\n");
						errno = myerr;
						return(-1);
					}
					break;
				case J_WRITE:
				default:
					if (a_add_end(j_p->al_p, a_p) < 0) {
						myerr = errno;
						ERR("build_jobs: a_add_end failed\n");
						errno = myerr;
						return(-1);
					}
					break;
			}
			LOG1("  build_jobs: added ack recv for iod %d\n", i);
			if (!(f_p->fd.flag & O_PASS)) /* add request */ {
				if ((r_p = (ireq_p) malloc(sizeof(ireq))) == NULL) {
					myerr = errno;
					PERROR("build_jobs: malloc (ireq)");
					errno = myerr;
					return(-1);
				}
				*r_p = req;
				if ((a_p = (ainfo_p) malloc(sizeof(ainfo))) == NULL) {
					myerr = errno;
					PERROR("build_jobs: malloc (ainfo2)");
					errno = myerr;
					return(-1);
				}
				a_p->type = A_REQ;
				a_p->sock = sock;
				a_p->u.req.size = sizeof(ireq);
				a_p->u.req.cur_p = a_p->u.req.req_p = (char *) r_p;
				if (a_add_start(j_p->al_p, a_p) < 0) {
					myerr = errno;
					ERR("build_jobs: a_add_start failed (req)\n");
					errno = myerr;
					return(-1);
				}
				addsockwr(sock, &socks);
				LOG1("  build_jobs: added req send for iod %d\n", i);
			} /* end of requesting process code */
			else /* get ready to read or write */ {
				switch(type) {
					case J_READ:
						addsockrd(sock, &socks);
						break;
					case J_WRITE:
						addsockwr(sock, &socks);
						break;
					default:
						ERR("build_jobs: bad job type\n");
				}
			}
		}
		/* else there's no job for this socket */
	}
	LOG("build_jobs: done.\n");
	return(0);
}
#undef PCOUNT

#define SSIZE ((int64_t) (f_p->fd.meta.p_stat.ssize))
#define SOFF  ((int64_t) (f_p->fd.meta.p_stat.soff))
#define PCOUNT ((int64_t) (f_p->fd.meta.p_stat.pcount))

/* ADD_ACCESSES() - adds accesses to handle a portion of a request
 * PARAMETERS:
 *    f_p   - pointer to file descriptor
 *    rl    - location of access relative to start of partition
 *    rs    - size of request
 *    buf_p - pointer to buffer for data (to send or receive)
 *    type  - A_READ or A_WRITE
 *
 * Note: this is called once for each contiguous region in request
 * Returns new buffer pointer location on success, NULL on failure
 */

void *add_accesses(fdesc_p f_p, int64_t rl, int64_t rs, char *buf_p, int type)
{
	int i, pn, sock, myerr; /* partition #, size of access */
	int64_t sz, blk;
	jinfo_p j_p;
	ainfo_p a_p;

	LOG2("  Will access frag(rl = %d; rs = %d).\n", rl, rs);
	/* compute iod holding rl */
	if (PCOUNT == 1) {
		pn = 0;
		sz = rs;
	}
	else {
		blk = (rl - SOFF) / SSIZE;
		pn = blk % PCOUNT;
		/* find distance from rl to end of stripe */
		sz = SSIZE - ((rl - SOFF) % SSIZE);
	}

	while (rs > (int64_t) 0) {
		/* find socket # */
		if ((sock = f_p->fd.iod[pn].sock) < 0) {
			LOG1("add_accesses: opening connection to iod %d\n", pn);
			/* NEED TO HAVE THIS OPEN CONNECTIONS */
			if ((sock = new_sock()) < 0) {
				myerr = errno;
				PERROR("add_accesses: new_sock");
				errno = myerr;
				return(NULL);
			}
			LOG2("add_accesses: new sock = %d (%d)\n", sock, 1 << sock);
add_accesses_connect:
			if (pn > PCOUNT - 1 || pn < 0)
				ERR3("add_accesses: bad pn calculation (rl = %Ld, blk = %Ld, pn = %d)\n",
				rl, blk, pn);
			if (connect(sock, (struct sockaddr *) &(f_p->fd.iod[pn].addr),
				sizeof(struct sockaddr_in)) < 0) {
				if (errno == EINTR) goto add_accesses_connect;
				if (errno == EAGAIN) {
					sleep(2);
					goto add_accesses_connect;
				}
				myerr = errno;
				PERROR("add_accesses: connect");
				close(sock);
				errno = myerr;
				return(NULL);
			}
			f_p->fd.iod[pn].sock = sock;
		}

		if (sz > rs) sz = rs; /* request ends before end of stripe */
		LOG2("    Will read/write %Ld bytes from iod %d.\n", sz, pn);

		/* find appropriate job */
		if ((j_p = j_search(active_p, sock)) == NULL) /* need new job */ {
			if (!(j_p = j_new(PCOUNT))) {
				ERR("add_accesses: j_new failed\n");
				return(NULL);
			}

			dfd_set(sock, &j_p->socks);
			if (sock >= j_p->max_nr) j_p->max_nr = sock+1;
			j_p->type = type;
			for (i=0; i < PCOUNT; i++) j_p->psize[i] = 0;

			if (j_add(active_p, j_p) < 0) /* error adding job to list */ {
				ERR("add_accesses: j_add failed\n");
				free(j_p);
				return(NULL);
			}
		}
		/* create access */
		if ((a_p = (ainfo_p) malloc(sizeof(ainfo))) == NULL) {
			myerr = errno;
			PERROR("add_accesses: malloc");
			errno = myerr;
			return(NULL);
		}
		a_p->type      = type;
		a_p->sock      = sock;
		a_p->u.rw.loc  = buf_p;
		a_p->u.rw.size = sz;

		/* update job info */
		j_p->size     += sz;
		j_p->psize[pn]+= sz;

		/* add access */
		if (a_add_end(j_p->al_p, a_p) < 0) /* error adding to list */ {
			myerr = errno;
			ERR("add_accesses: a_add_end failed\n");
			errno = myerr;
			return(NULL);
		}

		buf_p += sz;
		rs -= sz;
		sz = SSIZE;
		pn = (pn + 1) % PCOUNT;
	}
	return(buf_p);
} /* end of ADD_ACCESSES() */
#undef SOFF
#undef SSIZE
		

#define DSIZE    r_p->dsize

/* BUILD_RW_REQ() - Builds a request for an application based on the
 * current logical partition.
 *
 * Note: num parameter was only used in group I/O requests and corresponds
 * to the number of the process (0..n)
 *
 * NOTE: NUM IS NO LONGER USED.
 *
 * Also: type is in terms of a jlist type; it must be converted to an IOD_RW type
 */
int build_rw_req(fdesc_p f_p, int64_t size, ireq_p r_p, int type, int num)
{
	/* set up request */
	r_p->majik_nr     = IOD_MAJIK_NR;
	r_p->type         = IOD_RW;
	DSIZE             = 0;
	r_p->req.rw.f_ino = f_p->fd.meta.u_stat.st_ino;
	r_p->req.rw.cap   = f_p->fd.cap;

	switch (type) {
		case J_READ:
			REQTYPE = IOD_RW_READ;
			break;
		case J_WRITE:
			REQTYPE = IOD_RW_WRITE;
			break;
		default:
			REQTYPE = IOD_RW_WRITE;
			break;
	}

	if (!f_p->part_p) /* no partition specified! */ {
		OFFSET = f_p->fd.off;
		FSIZE  = size;
		LSIZE = GSIZE = GCOUNT = STRIDE = 0;
	}
	else if (!f_p->part_p->gsize || !f_p->part_p->stride ||
		f_p->part_p->gsize == f_p->part_p->stride)
	{
		OFFSET = f_p->fd.off + f_p->part_p->offset;
		FSIZE  = size;
		LSIZE = GSIZE = GCOUNT = STRIDE = 0;
	}
	else {
		/* file pointer is in terms of logical part.; offset is in terms */
		/* of the entire physical file (if all catenated) */
		OFFSET = ((f_p->fd.off / f_p->part_p->gsize) *
			f_p->part_p->stride) + (f_p->fd.off %
			f_p->part_p->gsize) + f_p->part_p->offset;

		FSIZE  = MIN(f_p->part_p->gsize -
			(f_p->fd.off % f_p->part_p->gsize), size);
		GSIZE  = f_p->part_p->gsize;
		STRIDE = f_p->part_p->stride;
		GCOUNT = MAX((size - FSIZE) / f_p->part_p->gsize, 0);
		LSIZE  = MAX(size - (FSIZE + (GCOUNT * GSIZE)), 0);
	}

	if (f_p->fd.flag & O_PASS) /* group connection */ {
		/* shift partition in accordance with group number */
		if (f_p->part_p) OFFSET += f_p->part_p->gstride * f_p->fd.grp_nr;
	}

#ifdef DEBUG
	if (debug_on) fprintf(stderr,
		"offset = %Ld; fsize = %Ld; gsize = %Ld; gcount = %Ld; "
		"stride = %Ld; lsize = %Ld.\n", OFFSET, FSIZE, GSIZE, GCOUNT, STRIDE,
		LSIZE);
#endif
	return 0;
} /* end of BUILD_RW_REQ() */

#undef REQTYPE
#undef CLIENTID
#undef DSIZE
#undef FSIZE
#undef GSIZE
#undef LSIZE
#undef GCOUNT
#undef STRIDE
#undef OFFSET
