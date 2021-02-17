/*
 * jobs.c copyright (c) 1997 Clemson University, all rights reserved.
 * 
 * Written by Rob Ross and Matt Cettei.
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

/* jobs.c
 *
 * This file contains code for building and servicing "jobs", which are
 * sequences of "accesses" which perform I/O operations requested by
 * applications.
 *
 * A single "job" results from a single request from an application and
 * will (in general) result in data transfer between one file and one
 * socket.
 *
 * There was, at one time, a "scheduled I/O" concept in PVFS which was
 * a technique for collective I/O.  We have pretty much stopped playing
 * with this code/idea, but some of the code is still around.  I try to
 * remove it when I see it.
 */

#include <stdio.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sockset.h>
#include <sockio.h>
#include <sys/socket.h>
#include <values.h>

#include <pvfs_config.h>
#include <iod.h>
#include <debug.h>
#include <jobs.h>
#include <minmax.h>

/* TODO: MAKE THE TCP_CORK STUFF WORK */
#undef TCP_CORK

/* INTERNAL PROTOTYPES */
static inline int do_access(alist_p al_p, ainfo_p a_p, int8_t *nospc);
static int64_t do_write(int sock, int64_t loc, int64_t size, int fd, int8_t *);
int mmap_page(finfo_p f_p, int64_t offset, int64_t size);
static int add_accesses(int sock, finfo_p f_p, jinfo_p j_p, int8_t type,
	int64_t offset, int64_t size);
static int j_wrap_tcpopt(int s, jinfo_p j_p, int name, int startval,
	int endval);


/* GLOBALS */
extern sockset socks;
extern int timing_on;			/* toggles timing record */
extern flist_p open_p;			/* list of open files */
extern jlist_p jobs_p; 			/* list of active jobs */
extern iodstat jobstat[STATNUM]; 	/* pointers to iodstats  */
extern int stat_ind;				/* index into iodstats	 */

static char jobs_c[] = "$Header: /projects/cvsroot/pvfs/iod/jobs.c,v 1.14 2000/11/22 16:06:05 rbross Exp $";

extern int errno;

#define F_SZ r_p->req.rw.fsize
#define G_SZ r_p->req.rw.gsize
#define G_CT r_p->req.rw.gcount
#define L_SZ r_p->req.rw.lsize
#define STR  r_p->req.rw.stride
#define OFF  r_p->req.rw.off

jinfo_p build_job(int sock, ireq_p r_p, finfo_p f_p)
{
	jinfo_p j_p;
	int atype, ret, hit_eof = 0;
	int64_t offset, gcount;

	if (!(j_p = j_new())) return(0);
	dfd_set(sock, &j_p->socks);
	if (sock >= j_p->max_nr) j_p->max_nr = sock+1;
	j_p->file_p = f_p;         /* save pointer to open file info */
	j_p->type   = r_p->req.rw.rw_type;

	switch(r_p->req.rw.rw_type) {
		case IOD_RW_READ:
			atype = A_READ;
			break;
		case IOD_RW_WRITE:
			atype = A_WRITE;
			break;
		default:
			atype = -1;
			break;
	}

	offset = OFF;
	gcount = G_CT;
	if (F_SZ > 0) {
		ret = add_accesses(sock, f_p, j_p, atype, offset, F_SZ);
		if (ret < 0) /* error in add_accesses */ {
			ERR("build_job: add_accesses failed for f_size");
			free(j_p);
			return(NULL);
		}
		else hit_eof = ret;
		offset += F_SZ + (STR - G_SZ);
	}
	while (gcount-- > 0 && !hit_eof) {
		ret = add_accesses(sock, f_p, j_p, atype, offset, G_SZ);
		if (ret < 0) /* error in add_accesses */ {
			ERR("build_job: add_accesses failed for g_size");
			free(j_p);
			return(NULL);
		}
		else hit_eof = ret;
		offset += STR;
	}
	if (L_SZ > 0 && !hit_eof) {
		ret = add_accesses(sock, f_p, j_p, atype, offset, L_SZ);
		if (ret < 0) /* error in add_accesses */ {
			ERR("build_job: add_accesses failed for l_size");
			free(j_p);
			return(NULL);
		}
		else hit_eof = ret;
	}

	/* handle case of empty job due to hit_eof */
	if (hit_eof && alist_empty(j_p->al_p)) {
		ainfo_p a_p;
		/* add a A_NOP to make sure an ACK is sent */
		if (!(a_p = (ainfo_p) malloc(sizeof(ainfo)))) return(NULL);
		a_p->type = A_NOP;
		a_p->sock = -1;

		if (a_add_end(j_p->al_p, a_p) < 0) /* error adding to list */ {
			ERR("build_job: a_add_end failed adding A_NOP\n");
			return(NULL);
		}
	}
	return(j_p);
} /* end of build_job() */

#undef F_SZ
#undef G_SZ
#undef G_CT
#undef L_SZ
#undef STR
#undef OFF

/* add_accesses()
 *
 * returns -1 on error, 0 on completion, and 1 if hit EOF in a read
 */
static int add_accesses(int sock, finfo_p f_p, jinfo_p j_p, int8_t type,
	int64_t offset, int64_t size)
{
	int64_t sz, pl, df, fl, el;
	ainfo_p a_p = 0;

	LOG4("add_accesses: s = %d, t = %d, o = %Ld, sz = %Ld\n", sock, type,
		offset, size);
	NEXTPL(f_p, offset, fl); /* fl is set by macro */
	while ((df = fl - offset) < size) {
		pl = FLTOPL(f_p, fl);
		sz = size - df;
		if (pl+sz > f_p->fsize && type==A_READ) /* check op. for append */ {
			/* update the file size info */
			if (update_fsize(f_p) < 0) return(-1);
			if (pl+sz > f_p->fsize) sz = f_p->fsize - pl; /* stop @ EOF */
			if (sz <= 0) {
				/* if this op. won't append and really at EOF, then we're done */
				LOG("  add_accesses: hit EOF\n");
				return(1);
			}
		}

		/* get space for the access; add to end of list; fill it in */
		if ((a_p = (ainfo_p) malloc(sizeof(ainfo))) == NULL) return(-1);
		if (a_add_end(j_p->al_p, a_p) < 0) /* error adding to list */ {
			ERR("add_accesses: a_add_end failed\n");
			return(-1);
		}
		a_p->type        = type;
		a_p->sock        = sock;
		a_p->u.rw.off    = pl;
		a_p->u.rw.file_p = f_p;

		el = TOPEND(f_p, pl);
		if (sz <= el || f_p->p_stat.pcount == 1) /* all in 1 block */ {
			j_p->size     += sz;
			a_p->u.rw.size = sz;
			j_p->lastb     = sz + pl;
			LOG1("  add_accesses: added last access for this region (sz = %Ld)\n",
				sz);
			LOG("add_accesses: done.\n");
			return(0);
		}
		else /* frag extends beyond this stripe */ {
			j_p->size     += el;
			a_p->u.rw.size = el;
			LOG1("  add_accesses: added access for this region (el = %Ld)\n", el);
		}

		/* prepare for next iteration */
		fl    += el;
		size  -= fl - offset;
		offset = fl;
		NEXTPL(f_p, offset, fl);
	}
	if (a_p) j_p->lastb = a_p->u.rw.size + a_p->u.rw.off;
	LOG("add_accesses: done.\n");
	return(0);
} /* end of add_accesses() */


/* do_job()
 * note: a sock value of -1 currently bypasses the check to see if the 
 * top access corresponds to a certain socket
 *
 * returns -1 on error, 0 on partial or full success
 */
int do_job(int sock, jinfo_p j_p)
{
	ainfo_p a_p;
	int oldtype;

	if (timing_on) {
		/* record time   */
		if (jobstat[j_p->jobnum].st_serv.tv_sec < 0)
			gettimeofday(&(jobstat[j_p->jobnum].st_serv), NULL);
	}	
	/* find first access in job for socket (if at top of list) */
	if (!(a_p = a_get_start(j_p->al_p))) {
		ERR("do_job: something bad happened finding top access\n");
		return(-1);
	}
	if (a_p->type != A_NOP && a_p->sock != sock && sock >= 0) /* not for us */
		return(0);

	oldtype = a_p->type;

	/* while there are accesses for this socket and we don't block */
	while (a_p) {
		/* do the access (or part of it) */
		switch (do_access(j_p->al_p, a_p, &(j_p->nospc))) {
			case 0 /* partial success (NOPs also return this) */:
				return(0);
			case 1 /* completed access */:
				/* finished this access; set up for the next one */
				if ((a_p = a_sock_search(j_p->al_p, sock)) != NULL) {
					/* not done w/sock */
					if (oldtype != a_p->type) switch(a_p->type) {
						case A_READ:
						case A_ACK:
							delsock(sock, &socks);
							addsockwr(sock, &socks);
							break;
						case A_WRITE:
							delsock(sock, &socks);
							addsockrd(sock, &socks);
							break;
						default:
							break;
					}
					/* check to see if next access is for this socket */
					if (a_get_start(j_p->al_p) != a_p) return(0);
				}
				else /* finished w/socket (no more accesses) */ {
					delsock(sock, &socks);
					/* watch for next request */
					if (is_active(sock)) addsockrd(sock, &socks);
					if (!j_p || alist_empty(j_p->al_p)) /* that's the end of this job */ {
						LOG("do_job: finished job - removing it\n");
						if (timing_on) gettimeofday(&(jobstat[j_p->jobnum].end_serv), NULL);
						if (j_rem(jobs_p, sock) < 0) {
							PERROR("do_job: j_rem");
						}	
						return(0);
					}
					/* otherwise we need to clear this socket out of the job's set */
					dfd_clr(sock, &j_p->socks);
					if (sock + 1 >= j_p->max_nr) /* need to reduce max_nr */ {
						int i;
						for (i=sock; i >= 0; i--) {
							if (dfd_isset(i, &j_p->socks)) break;
						}
						j_p->max_nr = i+1;
					}
				}
				break;
			default /* includes the -1 error value from do_access() */:
				return(-1);
		} /* end of switch */
	} /* end of while(a_p) */
	return(0);
} /* end of do_job() */



/* DO_ACCESS - performs necessary operations to complete one access,
 * pointed to by a_p, which is part of the access list pointed to by 
 * al_p.  A_READ and A_WRITE operations may be only partially completed.
 * In the case of completion, the associated ainfo structure is removed
 * from the alist using a_ptr_rem().
 *
 * Note: When encountered, A_NOP accesses are removed and 0 is returned.
 *
 * 64 BIT NOTES: We trap all "big" values of sizes here and reduce them
 * to MAXINT; this should allow the underlying code to function even in
 * the presence of the new, larger values...but I'm sure I missed
 * something somewhere...
 *
 * NOSPC NOTES: If the nospc flag passed in is non-zero, then data for
 * write accesses should be thrown away, as we have run out of space to
 * store it.  Likewise if we detect that there is no more space, we will
 * set this flag (this is done by do_write() in this case).
 *
 * Returns 1 on completion, 0 on partial success, or -1 in case of error.
 */
static inline int do_access(alist_p al_p, ainfo_p a_p, int8_t *nospc) {
	finfo_p f_p;
	int smallsize=0;
	int64_t comp;

	/* try to perform the access (read/write/ack/whatever)
	 */
	switch (a_p->type) {
		case A_READ /* read from file */:
			smallsize = MIN((int64_t)MAXINT, a_p->u.rw.size);
#ifndef LARGE_FILE_SUPPORT
			if (a_p->u.rw.off + a_p->u.rw.size > (int64_t) MAXINT) {
				ERR("do_access: off + size too big!\n");
				errno = EINVAL;
				return(-1);
			}
#endif

#ifdef __USE_SENDFILE__
			/* SENDFILE CODE */
			if (a_p->u.rw.off < (int64_t)MAXINT) {
				/* size is small enough to use sendfile() */
				comp = nbsendfile(a_p->sock, a_p->u.rw.file_p->fd,
				(int32_t) a_p->u.rw.off, smallsize);

				if (comp < 0) /* error */ {
					PERROR("do_access: do_write");
					return(-1);
				}
				if (!(a_p->u.rw.size -= comp)) /* done with access */ {
					LOG("done!\n");
					a_ptr_rem(al_p, a_p);
					return(1);
				}
				/* else partially completed */
				a_p->u.rw.off  += comp;
				return(0);
			}
#endif

			if (mmap_page(a_p->u.rw.file_p, a_p->u.rw.off, smallsize)) {
				ERR("do_access: something bad happened mapping file\n");
				return(-1);
			}
			/* NOTE: yes we want to fall through... */
		case A_WRITE /* write to file */:
			LOG4("do_job: read/write: t = %d, s = %d, o = %Ld, sz = %Ld\n",
				a_p->type, a_p->sock, a_p->u.rw.off, a_p->u.rw.size);
			f_p = a_p->u.rw.file_p;

			/* NOTE: smallsize IS SET UP ABOVE FOR READ CASE */
			if (a_p->type == A_READ) comp = nbsend(a_p->sock, f_p->mm.mloc
				+ a_p->u.rw.off - f_p->mm.moff, smallsize);
			else /* A_WRITE */ {
				if (*nospc) /* receive and throw away data */ {
					char buf[65536];
					comp = nbrecv(a_p->sock, buf, MIN(65536, a_p->u.rw.size));
				}
				else {
					comp = do_write(a_p->sock, a_p->u.rw.off, a_p->u.rw.size,
						a_p->u.rw.file_p->fd, nospc);
				}
			}
			if (comp < 0) /* error */ {
				PERROR("do_access: do_write");
				return(-1);
			}
			if (!(a_p->u.rw.size -= comp)) /* done with access */ {
				LOG("done!\n");
				a_ptr_rem(al_p, a_p);
				return(1);
			}
			/* else partially completed */
			a_p->u.rw.off  += comp;
			return(0);

		case A_ACK /* send ack to app */:
			LOG3("do_job: ack: s = %d, sz = %Ld, ds = %Ld\n", a_p->sock,
				a_p->u.ack.size, ((iack_p)a_p->u.ack.ack_p)->dsize);
			if (*nospc) {
				((iack_p)a_p->u.ack.cur_p)->eno = ENOSPC;
				((iack_p)a_p->u.ack.cur_p)->dsize = 0; /* sort of worst case */
				((iack_p)a_p->u.ack.cur_p)->status = -1;
			}
			comp = nbsend(a_p->sock, a_p->u.ack.cur_p, a_p->u.ack.size);
			if (comp < 0) {
				PERROR("do_access: nbsend");
				return(-1);
			}
			if (!(a_p->u.ack.size -= comp)) {
				LOG("done!\n");
				free(a_p->u.ack.ack_p);
				a_ptr_rem(al_p, a_p);
				return(1);
			}
			/* else partially completed */
			a_p->u.ack.cur_p += comp;
			return(0);

		case A_SOCKOPT /* set socket option */:
			comp = setsockopt(a_p->sock, a_p->u.sockopt.level,
				a_p->u.sockopt.name, &(a_p->u.sockopt.val),
				sizeof(a_p->u.sockopt.val));
			a_ptr_rem(al_p, a_p);
			return(comp ? -1 : 1);

		case A_NOP /* no operation */:
			a_ptr_rem(al_p, a_p);
			return(0);

		default:
			ERR1("do_access: unsupported access type (%d)\n", a_p->type);
			a_ptr_rem(al_p, a_p);
			return(-1);
	} /* end of switch */
	return(-1); /* we should never hit this */
} /* end of do_access() */




/* DO_RW_REQ() - builds jobs for read and write requests
 */
int do_rw_req(int sock, ireq_p r_p, iack_p a_p) {
	int i;
	ainfo_p nopa_p;
	finfo_p f_p;
	jinfo_p j_p=0;

	/* find the open file info */
	if ((f_p = f_search(open_p, r_p->req.rw.f_ino, r_p->req.rw.cap)) == NULL) {
		errno = ENOENT;
		return(-1);
	}
	/* check file->socket association */
	/* THIS IS IN NEED OF IMPROVEMENT!! */
	if (f_p->sock == -1 && f_p->grp.sched == -1) f_p->sock = sock;

	for (i = 0; i < MAX(1,f_p->grp.nr_conn); i++) /* foreach job to build */ {
		jinfo_p tmpj_p;
		ainfo_p tmpacc_p;

		tmpj_p = build_job(sock, r_p, f_p);

		if (!tmpj_p) /* start a new job */ {
			ERR("do_rw: build_job failed\n");
			return(-1);
		}
		tmpj_p->jobnum = stat_ind; 
		if (timing_on) {			/* Record start of wait */
			if (stat_ind < STATNUM-1)	stat_ind++; 
			gettimeofday(&(jobstat[tmpj_p->jobnum].st_wait), NULL);
			jobstat[tmpj_p->jobnum].numaccess = 
				count_access(tmpj_p, &(jobstat[tmpj_p->jobnum].offset));
			jobstat[tmpj_p->jobnum].fd 		= tmpj_p->file_p->fd;
			jobstat[tmpj_p->jobnum].type		= tmpj_p->type;
		}	

		if (alist_empty(tmpj_p->al_p)) /* nothing in job */ {
			LOG("do_rw_req: empty job - removing\n");
			j_free(tmpj_p);
			tmpj_p = NULL;
		}
		else /* there is data being sent to this socket */ {
			/* wrap accesses with TCP options before ACK is added */
			switch(tmpj_p->type) {
				case IOD_RW_WRITE:
					if (j_wrap_tcpopt(sock, tmpj_p, TCP_NODELAY, 0, 1) < 0)
						return(-1);
					break;
			}

			/* set up ack access */
			if (!(tmpacc_p = ack_a_new())) return(-1);
			
			tmpacc_p->sock = sock;
			*(iack_p)tmpacc_p->u.ack.ack_p = *a_p;
			((iack_p)tmpacc_p->u.ack.ack_p)->type   = r_p->req.rw.rw_type;
			((iack_p)tmpacc_p->u.ack.ack_p)->dsize  = tmpj_p->size;
			((iack_p)tmpacc_p->u.ack.ack_p)->status = 0;

			/* add ack and make sure socket is being checked */
			switch(tmpj_p->type) {
				case IOD_RW_READ:
					/* if type is a read, add to front */
					if (a_add_start(tmpj_p->al_p, tmpacc_p) < 0) {
						ERR("do_rw_req: a_add_start failed\n");
						return(-1);
					}
					addsockwr(sock, &socks);
					LOG1("added %d for writing\n", sock);
					break;
				case IOD_RW_WRITE:
					/* otherwise add to the back */
					if (a_add_end(tmpj_p->al_p, tmpacc_p) < 0) {
						ERR("do_rw_req: a_add_start failed\n");
						return(-1);
					}
					addsockrd(sock, &socks);
					LOG1("added %d for reading\n", sock);
					break;
				default:
					ERR1("do_rw: unsupported job type (%d)\n", tmpj_p->type);
					break;
			}

			/* wrap accesses with TCP options after ACK is added */
			switch(tmpj_p->type) {
				case IOD_RW_READ:
#ifdef TCP_CORK
					if (j_wrap_tcpopt(sock, tmpj_p, TCP_CORK, 1, 0) < 0)
						return(-1);
#endif
					break;
			}

			if (j_p) /* second or later iteration */ {
				j_merge(j_p, tmpj_p); /* merge tmpj_p into j_p */
			}
			else j_p = tmpj_p; /* save the current job */
		} /* end of "there is something in this job" */
	} /* end of building jobs for request */

	/* remove all NOPs from job; bug fix */
	while ((nopa_p = a_type_search(j_p->al_p, A_NOP)) != NULL) {
		a_ptr_rem(j_p->al_p, nopa_p);
	}

	/* add job to job list */
	if (j_add(jobs_p, j_p) < 0) {
		ERR("do_rw: j_add failed\n");
		return(-1);
	}
	return(0);
} /* end of do_rw_req() */

/* DO_WRITE() - receives data from an app. and stores in a file
 *
 */
static int64_t do_write(int sock, int64_t loc, int64_t size, int fd,
	int8_t *nospc)
{
	int64_t rsize, wsize, comp;
	int64_t total = 0;
	int myerr;
	static char buf[IOD_WRITE_BUFFER_SIZE]; 

#ifndef LARGE_FILE_SUPPORT
			if (loc + size > (int64_t) MAXINT) {
				ERR("do_write: loc + size too big!\n");
				errno = EINVAL;
				return(-1);
			}
#endif
	LOG1("	 seeking to %Ld\n", loc);
#ifndef LARGE_FILE_SUPPORT
	if (lseek(fd, loc, SEEK_SET) == -1) {
		myerr = errno;
		PERROR("do_write: lseek");
		errno = myerr;
		return(-1);
	}
#else
	if (llseek(fd, loc, SEEK_SET) == -1) {
		myerr = errno;
		PERROR("do_write: llseek");
		errno = myerr;
		return(-1);
	}
#endif

	while (size > 0) {
		rsize = MIN(size, (int64_t) IOD_WRITE_BUFFER_SIZE);
		LOG2("	 trying to get %Ld bytes from %d; ", rsize, sock);
		if ((wsize = nbrecv(sock, buf, rsize)) == -1) 
		{
			PERROR("do_write: nbrecv");
			return(-1);
		}
		LOG1("	 got %Ld bytes; ", wsize);
		if (!wsize) return(total);
		comp = write(fd, buf, wsize);
		if (comp != wsize) {
			/* We're going to assume we ran out of space here */
			*nospc = 1;
			ERR("do_write: ran out of space, will ignore remaining data\n");
			return(wsize);
		}
		LOG1("	 wrote %Ld bytes\n", comp);
		if (wsize < rsize) /* received less from socket than asked for */
			return(total + comp);
		size -= comp;
		total += comp; /* keep this around for easy return value */
	}
	return(total);
} /* end of do_write() */


/* ACK_A_NEW() - returns a pointer to an access with memory allocated
 * for the access and the ack it is designated to send.
 *
 * returns pointer on success, NULL on failure
 *
 * Some things that should be filled in by caller:
 *   in access: sock
 *   in ack: everything!
 */
ainfo_p ack_a_new(void) {
	ainfo_p tmpacc_p;
	iack_p  tmpack_p;

	if (!(tmpack_p = (iack_p) malloc(sizeof(iack)))) {
		PERROR("ack_a_new: malloc");
		return(0);
	}
	tmpack_p->type   = -1;
	tmpack_p->dsize  = -1;
	tmpack_p->status = 0;

	/* build an access to point to ack */
	if (!(tmpacc_p = (ainfo_p) malloc(sizeof(ainfo)))) {
		PERROR("ack_a_new: malloc");
		return(0);
	}
	tmpacc_p->type = A_ACK;
	tmpacc_p->sock = -1;
	tmpacc_p->u.ack.ack_p = tmpacc_p->u.ack.cur_p = tmpack_p;
	tmpacc_p->u.ack.size = sizeof(iack);
	return(tmpacc_p);
}


/* DISK_MERGE_ALIST() - Merges two alists, sorting the accesses in
 * such a way that leading acks are kept at the front of the new list,
 * trailing acks are kept at the end of the new list, and read/write
 * accesses are ordered sequentially with respect to offset only.
 *
 * After completion, al1_p points to the new list, while al2_p is no
 * longer valid.
 *
 * NOTE: This function is dependent on the access list implementation!!!
 * ALSO NOTE: This function won't handle A_LISTs at all!!!
 *
 * llist_rem is used because it doesn't free the item memory, while
 * a_ptr_rem does.
 *
 * Returns 0 on success, -1 on failure.
 */
int disk_merge_alist(alist_p al1_p, alist_p al2_p)
{
	alist_p new_p;
	ainfo_p i_p, i2_p;

	new_p = alist_new();

	/* pull leading ACKs from both lists */
	while ((i_p = a_get_start(al1_p)) && i_p->type == A_ACK) {
		a_add_end(new_p, i_p);
		llist_rem(al1_p, (void *) i_p, a_ptr_cmp);
	}
	while ((i_p = a_get_start(al2_p)) && i_p->type == A_ACK) {
		a_add_end(new_p, i_p);
		llist_rem(al2_p, (void *) i_p, a_ptr_cmp);
	}

	/* pull RWs from both lists, sorting as we go */
	while ((i_p = a_get_start(al1_p)) && (i2_p = a_get_start(al2_p)))
	{
		if (i_p->type == A_ACK && i2_p->type == A_ACK) break;

		if (i_p->type == A_ACK) /* got all RWs from l1 */ {
			while (i2_p && i2_p->type != A_ACK) {
				a_add_end(new_p, i2_p);
				llist_rem(al2_p, (void *) i2_p, a_ptr_cmp);
				i2_p = a_get_start(al2_p);
			}
		}

		else if (i2_p->type == A_ACK) /* got all RWs from l2 */ {
			while (i_p && i_p->type != A_ACK) {
				a_add_end(new_p, i_p);
				llist_rem(al1_p, (void *) i_p, a_ptr_cmp);
				i_p = a_get_start(al1_p);
			}
		}
		else /* ASSUMING REQUEST IS RW; need to compare */ {
			if (i_p->u.rw.off < i2_p->u.rw.off) {
				a_add_end(new_p, i_p);
				llist_rem(al1_p, (void *) i_p, a_ptr_cmp);
			}
			else {
				a_add_end(new_p, i2_p);
				llist_rem(al2_p, (void *) i2_p, a_ptr_cmp);
			}
		}
	}

	/* pull trailing accesses (any type) from both lists */
	while ((i_p = a_get_start(al1_p))) {
		a_add_end(new_p, i_p);
		llist_rem(al1_p, (void *) i_p, a_ptr_cmp);
	}
	while ((i_p = a_get_start(al2_p))) {
		a_add_end(new_p, i_p);
		llist_rem(al2_p, (void *) i_p, a_ptr_cmp);
	}

	/* swap around the lists */
	alist_cleanup(al2_p);
	al2_p = al1_p->next;
	al1_p->next = new_p->next;
	new_p->next = al2_p;
	alist_cleanup(new_p);

	return(0);
}

/* INTER_MERGE_ALIST() - merges two alists, interleaving the accesses
 *
 * After completion, al1_p points to the new list, while al2_p is no
 * longer valid.
 *
 * NOTE: This function is dependent on the access list implementation!!!
 * ALSO NOTE: This function won't handle A_LISTs at all!!!
 *
 * llist_rem is used because it doesn't free the item memory, while
 * a_ptr_rem does.
 *
 * Returns 0 on success, -1 on failure.
 */
int inter_merge_alist(alist_p al1_p, alist_p al2_p)
{
	alist_p new_p;
	ainfo_p i_p, i2_p;

	new_p = alist_new();

	while ((i_p = a_get_start(al1_p)) && (i2_p = a_get_start(al2_p))) {
		a_add_end(new_p, i_p);
		a_add_end(new_p, i2_p);
		llist_rem(al1_p, (void *) i_p, a_ptr_cmp); /* remove from list
		                                            * w/out freeing memory
	                                               */
		llist_rem(al2_p, (void *) i2_p, a_ptr_cmp);
	}

	/* only one list has any accesses left */
	while ((i_p = a_get_start(al1_p)) != NULL) {
		a_add_end(new_p, i_p);
		llist_rem(al1_p, (void *) i_p, a_ptr_cmp);
	}
	while ((i_p = a_get_start(al2_p)) != NULL) {
		a_add_end(new_p, i_p);
		llist_rem(al2_p, (void *) i_p, a_ptr_cmp);
	}
	
	/* swap around the lists - want al1_p to have new list */
	alist_cleanup(al2_p);
	al2_p = al1_p->next; /* using al2_p as a temp. var. from now on */
	al1_p->next = new_p->next;
	new_p->next = al2_p;
	alist_cleanup(new_p);

	return(0);
}


/* J_MERGE() - merges two jobs, taking the job info from the first
 *
 */
int j_merge(jinfo_p j1_p, jinfo_p j2_p)
{
	int i;

	inter_merge_alist(j1_p->al_p, j2_p->al_p);

	/* add sizes */
	if (j2_p->size > 0) j1_p->size += j2_p->size;

	/* update socket list */
	for (i=0; i < j2_p->max_nr; i++) {
		if (dfd_isset(i, &j2_p->socks)) dfd_set(i, &j1_p->socks);
	}
	j1_p->max_nr = MAX(j1_p->max_nr, j2_p->max_nr);

	/* assume files are the same; take larger lastb */
	j1_p->lastb = MAX(j1_p->lastb, j2_p->lastb);

	j2_p->al_p = NULL; /* no longer valid */
	j_free(j2_p);
	return(0);
}

/* j_wrap_tcpopt()
 *
 * Used to wrap a job in accesses which will perform a sockopt().
 *
 * Returns 0 on success, -1 on failure.
 */
static int j_wrap_tcpopt(int s, jinfo_p j_p, int name, int startval, int endval)
{
	ainfo_p tmpacc_p;

	if (!(tmpacc_p = (ainfo_p) malloc(sizeof(ainfo)))) return(-1);
	tmpacc_p->type = A_SOCKOPT;
	tmpacc_p->sock = s;
	tmpacc_p->u.sockopt.level = SOL_TCP;
	tmpacc_p->u.sockopt.name  = name;
	tmpacc_p->u.sockopt.val   = startval;
	
	if (a_add_start(j_p->al_p, tmpacc_p) < 0) {
		ERR("j_wrap_tcpopt: a_add_start failed\n");
		return(-1);
	}

	if (!(tmpacc_p = (ainfo_p) malloc(sizeof(ainfo)))) return(-1);
	tmpacc_p->type = A_SOCKOPT;
	tmpacc_p->sock = s;
	tmpacc_p->u.sockopt.level = SOL_TCP;
	tmpacc_p->u.sockopt.name  = name;
	tmpacc_p->u.sockopt.val   = endval;
	if (a_add_end(j_p->al_p, tmpacc_p) < 0) {
		ERR("j_wrap_tcpopt: a_add_start failed\n");
		return(-1);
	}
	return(0);
}

