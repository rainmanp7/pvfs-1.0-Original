/*
 * do_job.c copyright (c) 1996 Rob Ross, all rights reserved.
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
 */

/*
 * DO_JOB.C - performs IOD interaction for application tasks
 *
 */

/* INCLUDES */
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>
#include <values.h>
#include <minmax.h>
#include <alist.h>
#include <jlist.h>
#include <req.h>
#include <sockio.h>
#include <sockset.h>
#include <debug.h>
#include <prune.h>



extern int errno;

static char do_job_c[] = "$Header: /projects/cvsroot/pvfs/lib/do_job.c,v 1.7 2000/11/22 16:06:05 rbross Exp $";

/* DO_JOBS() - 
 * PARAMETERS:
 * jl_p - list of jobs corresponding to sockets in ss_p
 * ss_p - sockset checked for readiness for access
 * msec - time to wait (in msec) before timing out; -1 blocks
 *        indefinitely
 *
 * Returns -1 on error, 0 on success.
 */
int do_jobs(jlist_p jl_p, sockset_p ss_p, int msec)
{
	int s, ret;
	jinfo_p j_p;
	int myerr;

	/* NEED TO BE LOOKING FOR ERRORS... */
	if (check_socks(ss_p, msec) < 0) {
		myerr = errno;
		ERR1("do_jobs: check socks failed: %s\n", strerror(errno));
		errno = myerr;
		return(-1); /* wait some amount of time */
	}
	while ((s = nextsock(ss_p)) >= 0) /* still sockets ready */ {
		if (!(j_p = j_search(jl_p, s))) /* no job for socket */ {
			ERR1("do_jobs: no job for %d (so why are we watching it?)\n", s);
			delsock(s, ss_p);
		}
		else {
			if (do_job(s, j_p, ss_p) < 0) {
				myerr = errno;
				ERR1("do_jobs: do_job failed: %s\n", strerror(errno));
				close(s); /* just added 03/27/2000; seems to make sense... */
				errno = myerr;
				return(-1);
			}
			if (alist_empty(j_p->al_p)) {
				/* remove job from joblist */
				LOG("do_jobs: no more accesses - removing job\n");
				if ((ret = (j_rem(jl_p, s))) < 0) {
					ERR("do_jobs: j_rem failed\n");
				}
			}
		}
	}
	return(0);
}

/* do_job() - works on a given job, not blocking
 *
 * This function has grown ridiculously long, I know.  I apologize.
 *
 * Basically this function looks at the job, finds the first access for
 * a given socket, checks to make sure that this is indeed the first
 * access in the job, and if so starts working.
 *
 * Rules at the bottom decide when to stop if we don't run out of
 * buffers.  They are designed to try to continue working whenever it
 * seems reasonable, and stop whenever we know that there isn't going to
 * be data ready.
 *
 * Returns -1 if an error is encountered, 0 if we don't do anything, and
 * 0 otherwise.
 */
int do_job(int sock, jinfo_p j_p, sockset_p ss_p)
{
	ainfo_p a_p;
	int smallsize;
	int comp, oldtype, myerr;
	int val;

	/* find first access in job for socket (if at top of list) */
	if (!(a_p = a_get_start(j_p->al_p))) {
		ERR("do_job: something bad happened finding top access\n");
		delsock(sock, ss_p);
		close(sock);
		errno = EINVAL;
		return(-1);
	}
	if (a_p->sock != sock) /* top access not for this socket */ {
		LOG2("top sock(%d) != current sock(%d)\n", a_p->sock, sock);
		return(0);
	}
	oldtype = a_p->type;

	/* while there are accesses for this socket and we don't block */
	while (a_p) {
		/* try to perform the access (read/write/ack/whatever) */
		switch (a_p->type) {
			case A_READ:
			case A_WRITE:
				/* can't try to send more than MAXINT...thus smallsize */
				smallsize = MIN((u_int64_t)MAXINT, a_p->u.rw.size);
				LOG3("read/write (s = %d, ad = %lu, sz = %d) ", a_p->sock,
					a_p->u.rw.loc, smallsize);
				if (a_p->type == A_READ) comp = nbrecv(a_p->sock,
					a_p->u.rw.loc, smallsize);
				
				else comp = nbsend(a_p->sock, a_p->u.rw.loc, smallsize);

				if (comp < 0) /* error */ {
					myerr = errno;
					PERROR("do_job: nbsend/nbrecv");
					/* check the error and try to decide what to do */
					a_ptr_rem(j_p->al_p, a_p);
					errno = myerr;
					return(-1);
				}
				else if ((a_p->u.rw.size -= comp) <= 0) /* done with access */ {
					LOG("done!\n");
					a_ptr_rem(j_p->al_p, a_p);
				}
				else /* partially completed */ {
					LOG("partial\n");
					a_p->u.rw.loc  += comp; /* subtract from size above */
					return(0);
				}
				break;
			case A_ACK:
				LOG1("ack (s = %d) ", a_p->sock);
				comp = nbrecv(a_p->sock, a_p->u.ack.cur_p, a_p->u.ack.size);
				if (comp < 0) {
					myerr = errno;
					PERROR("do_job: nbrecv (ack)");
					/* check the error and try to decide what to do */
					a_ptr_rem(j_p->al_p, a_p);
					errno = myerr;
					return(-1);
				}
				else if ((a_p->u.ack.size -= comp) > 0) /* partially completed */ {
					LOG("partial\n");
					a_p->u.ack.cur_p += comp;
					return(0);
				}
				LOG("done!\n");

				/* check if requested data size > actual size for J_READ */
				if (j_p->type == J_READ
				&& j_p->psize[a_p->u.ack.iod_nr] >
					((iack_p)a_p->u.ack.ack_p)->dsize)
				{
					LOG("do_job: getting less data than expected\n");
					if (prune_alist(j_p->al_p, A_READ, a_p->sock,
					j_p->psize[a_p->u.ack.iod_nr],
					((iack_p)a_p->u.ack.ack_p)->dsize) < 0) {
						ERR("do_job: prune failed\n");
					}
				}
				/* if ack value smaller, prune the access list */
				a_ptr_rem(j_p->al_p, a_p); /* remove ack access from list */
				break;
			case A_REQ:
				LOG1("req (s = %d) ", a_p->sock);
				comp = nbsend(a_p->sock, a_p->u.req.cur_p, a_p->u.req.size);
				if (comp < 0) {
					myerr = errno;
					PERROR("do_job: nbsend (req)");
					/* check the error and try to decide what to do */
					a_ptr_rem(j_p->al_p, a_p);
					errno = myerr;
					return(-1);
				}
				else if ((a_p->u.req.size -= comp) <= 0) {
					LOG("done!\n");
					free(a_p->u.req.req_p); /* free memory */
					a_ptr_rem(j_p->al_p, a_p);
				}
				else {
					LOG("partial\n");
					a_p->u.req.cur_p += comp;
					return(0);
				}
				break;
			default:
				ERR1("do_job: invalid access type (%d)\n", a_p->type);
				errno = EINVAL;
				return(-1);
		} /* end of switch */
		/* finished this access; get the next one */
		if (a_p = a_search(j_p->al_p, sock)) /* more work for this socket */ {
			/* update socket set */
			if (oldtype != a_p->type) {
				switch (a_p->type) {
					case A_ACK:
#ifdef TCP_CORK
						if (j_p->type == J_WRITE) {
							/* turn off TCP_CORK; write is finished */
							val = 0;
							setsockopt(sock, SOL_TCP, TCP_CORK, &val, sizeof(val));
						}
#endif
					case A_READ:
						delsock(sock, ss_p);
						addsockrd(sock, ss_p);
						break;
					case A_WRITE:
#ifdef TCP_CORK
						if (j_p->type == J_WRITE) {
							/* turn on TCP_CORK; write is starting */
							val = 1;
							setsockopt(sock, SOL_TCP, TCP_CORK, &val, sizeof(val));
						}
#endif
					case A_REQ:
						delsock(sock, ss_p);
						addsockwr(sock, ss_p);
						break;
					default:
						break;
				}
			}
			switch (oldtype) {
				case A_ACK:
					/* if new request is a READ, continue processing.
					 * Otherwise stop here.
					 */
					if (a_p->type != A_READ) return 0;
					break;
				case A_READ:
					/* just keep going for now */
					break;
				case A_WRITE:
					/* if new request is an ACK, stop.  Otherwise continue
					 * processing.
					 */
					if (a_p->type == A_ACK) return 0;
					break;
				case A_REQ:
					/* if the new request is a WRITE, continue processing.
					 * Otherwise stop here.
					 */
					if (a_p->type != A_WRITE) return 0;
					break;
				default:
					break;
			}
			/* see if the next access is the next one for this socket */
			if (a_get_start(j_p->al_p) != a_p) return(0);
			oldtype = a_p->type;
		}
		else /* done with this socket */ {
			delsock(sock, ss_p);
			if (alist_empty(j_p->al_p)) /* done w/job */ {
				/* job is removed one function depth up in do_jobs */
				return(0);
			}
			/* remove socket from job's set */
         dfd_clr(sock, &j_p->socks);
			if (sock + 1 >= j_p->max_nr) /* need to reduce max_nr */ {
				int i;
				for (i=sock; i >= 0; i--) if (dfd_isset(i, &j_p->socks)) break;
				j_p->max_nr = i+1;
			}
		}
	} /* end of while(a_p) */
	return(0);
}















