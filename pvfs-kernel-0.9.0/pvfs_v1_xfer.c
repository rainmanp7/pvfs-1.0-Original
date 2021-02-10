/* Hey Emacs! This file is -*- nroff -*- source.
 * 
 * Copyright (c) 1996 Clemson University, All Rights Reserved.
 * 2000 Argonne National Laboratory.
 * Originally Written by Rob Ross.
 *
 * Parallel Architecture Research Laboratory, Clemson University
 * Mathematics and Computer Science Division, Argonne National Laboratory
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
 **Code** This Protected Resource Provided By = rainmanp7
 * Contact:  
 * Rob Ross			    rross@mcs.anl.gov
 * Matt Cettei			mcettei@parl.eng.clemson.edu
 * Chris Brown			muslimsoap@gmail.com
 * Walt Ligon			walt@parl.clemson.edu
 * Robert Latham		robl@mcs.anl.gov
 * Neill Miller 		neillm@mcs.anl.gov
 * Frank Shorter		fshorte@parl.clemson.edu
 * Harish Ramachandran	rharish@parl.clemson.edu
 * Dale Whitchurch		dalew@parl.clemson.edu
 * Mike Speth			mspeth@parl.clemson.edu
 * Brad Settlemyer		bradles@CLEMSON.EDU
 */

static char pvfs_v1_xfer_c_hdr[] = "$Header: /projects/cvsroot/pvfs-kernel/pvfs_v1_xfer.c,v 1.44 2001/01/12 21:27:37 rbross Exp $";

/*
 * This file implements a library of calls which allow VFS-like access
 * to v1.xx PVFS file systems.  This is done by mapping the VFS-like
 * operations (as defined in ll_pvfs.h) into v1.xx operations, which is
 * not always a 1-to-1 mapping.
 *
 * We also build a list of open files in order to avoid using the
 * wrapper functions in the PVFS library.  This list is searched via
 * handle and pointer to manager information (sockaddr really).
 *
 * EXTERNALLY AVAILABLE OPERATIONS IMPLEMENTED IN THIS FILE:
 *
 * int pvfs_comm_init(void) - called before communication is started
 *    (before do_pvfs_op() is called)
 * int do_pvfs_op(mgr, up, down) - called to perform any valid PVFS
 *    VFS operation
 * void pvfs_comm_idle(void) - called occasionally to close idle files
 * void pvfs_comm_shutdown(void) - called to cleanly shut down
 *    communication before exiting
 *
 * A NOTE ON FILENAMES PASSED TO DO_PVFS_OP:
 * We needed to have a simple way to get the manager address and port
 * into the functions here, so we have embedded this information into
 * the file names passed in.  The format is <addr>:<port>/<filename>.
 * Examples:
 * foo:3000/pvfs
 * localhost:4000/pvfs/foo/bar
 * Names such as these are used throughout the do_XXX_op calls and in
 * the code that handles searches for open files.
 *
 * YOU SHOULD ENSURE THE NAME HAS A VALID FORMAT BEFORE PASSING IT IN!!!
 *
 * Three functions are used for working with these names, so if we want
 * to change the format later we can:
 *   static char *skip_to_filename(char *name);
 *   static int name_to_port(char *name);
 *   static void hostcpy(char *host, char *name);
 * The first one returns a pointer to the filename portion of the name.
 * The second returns an integer port number in host byte order.
 * The last one copies the hostname into a buffer of size PVFSHOSTLEN.
 * 
 */

/* TODO
 *
 * Make pvfs_mgr_init() take a pointer to a buffer and store its result
 * in there instead of malloc()ing the space.
 */

#include <config.h>

/* UNIX INCLUDES */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <unistd.h>

#ifdef LINUX
#include <linux/types.h>
#include <linux/dirent.h>
#endif

/* PVFS V2 INCLUDES */
#include <ll_pvfs.h>
#include <pvfs_v1_xfer.h>
#include <pvfs_kernel_config.h>

/* PVFS V1 INCLUDES */
#include <sockio.h>
#include <sockset.h>
#include <jlist.h>
#include <req.h>
#include <meta.h>
#include <desc.h>

enum {
	/* number of times we'll retry if too many files are open */
	PVFS_XFER_MAX_RETRY = 2
};

/* GLOBALS (for v1 lib. calls) */
extern jlist_p active_p;
extern sockset socks;
extern int pvfs_debug;

/* MGRCOMM ROUTINE USED IN HERE */
int send_mreq_saddr(struct sockaddr *saddr_p, mreq_p req_p,
	void *data_p, mack_p ack_p);

/* BUILD_JOBS ROUTINES USED IN HERE */
int build_rw_jobs(fdesc_p f_p, char *buf_p, int64_t size, int type);
int build_simple_jobs(fdesc_p f_p, ireq *req_p);

/* DO_JOB ROUTINE USED IN HERE */
int do_jobs(jlist_p jl_p, sockset_p ss_p, int msec);

/* JLIST ROUTINE USED IN HERE (AND NOT PROTOTYPED?) */
int jlist_empty(jlist_p jl_p);


/* OPEN FILE MISC. -- all this stuff is internal to this file too */
typedef llist_p pfl_t;

struct pf {
	pvfs_handle_t handle;
	char *name;
	time_t ltime; /* last time we used this open file */
	fdesc *fp;
};

struct pf_cmp {
	pvfs_handle_t handle;
	char *name;
};

pfl_t file_list = NULL;

static pfl_t pfl_new(void);
static struct pf *pfl_head(pfl_t pfl);
static int pf_add(pfl_t pfl, struct pf *p);
static void pfl_cleanup(pfl_t pfl);
static struct pf *pf_search(pfl_t pfl, pvfs_handle_t handle, char *name);
static void pfl_cleanup(pfl_t pfl);
static int pf_handle_cmp(void *cmp, void *pfp);
static int pf_ltime_cmp(void *time, void *pfp);
static int pf_ltime_olderthan(void *time, void *pfp);
static void pf_free(void *p);
static int pf_rem(pfl_t pfl, pvfs_handle_t handle, char *name);
static struct pf *pf_new(fdesc *fp, pvfs_handle_t handle, char *name);
static int pf_zerotime(void *pfp);

/* PROTOTYPES FOR INTERNAL FUNCTIONS */
static void init_mgr_req(mreq *rp, struct pvfs_upcall *op);
static void init_res(struct pvfs_downcall *resp, struct pvfs_upcall *op);
static void v1_fmeta_to_v2_meta(struct fmeta *fmeta, struct pvfs_meta *meta);
static void v1_fmeta_to_v2_phys(struct fmeta *fmeta, struct pvfs_phys *phys);
static int recv_dirents(int fd, pvfs_off_t *off, pvfs_size_t dsize,
	struct pvfs_dirent dlist[], pvfs_count_t *count);
static int open_pvfs_file(struct sockaddr *mgr, pvfs_char_t name[]);
static void close_pvfs_file(struct sockaddr *mgr, struct pf *pfp);
static int do_generic_op(struct sockaddr *mgr, struct pvfs_upcall *op,
	struct pvfs_downcall *resp);
static int do_create_op(struct sockaddr *mgr, struct pvfs_upcall *op,
	struct pvfs_downcall *resp);
static int do_hint_op(struct sockaddr *mgr, struct pvfs_upcall *op,
	struct pvfs_downcall *resp);
static int do_setmeta_op(struct sockaddr *mgr, struct pvfs_upcall *op,
	struct pvfs_downcall *resp);
static int do_getdents_op(struct sockaddr *mgr, struct pvfs_upcall *op,
	struct pvfs_downcall *resp);
static int do_rw_op(struct sockaddr *mgr, struct pvfs_upcall *op,
	struct pvfs_downcall *resp);
static int do_mkdir_post(struct sockaddr *mgr, struct pvfs_upcall *op,
	struct pvfs_downcall *resp);
static void ping_iods(fdesc *fp);
static fdesc *do_open_req(struct sockaddr *mgr, mreq *reqp, pvfs_char_t name[],
	int *errp);
static int create_pvfs_file(struct sockaddr *mgr, pvfs_char_t name[],
	struct pvfs_meta *meta, struct pvfs_phys *phys);
static void v2_meta_to_v1_fmeta(struct pvfs_meta *meta,
	struct fmeta *fmeta);
static void v2_phys_to_v1_fmeta(struct pvfs_phys *phys,
	struct fmeta *fmeta);
static void v1_ack_to_v2_statfs(struct mack *ack, struct pvfs_statfs *sfs);
static char *skip_to_filename(char *name);
static int name_to_port(char *name);
static void hostcpy(char *host, char *name);
static struct sockaddr *pvfs_mgr_init(pvfs_char_t host[], uint16_t port,
	pvfs_xferproto_t proto);
static int close_some_files(void);

/* EXPORTED FUNCTIONS */

/* pvfs_comm_init()
 *
 * Handle any initialization necessary...
 */
int pvfs_comm_init(void)
{
	return 0;
}

/* pvfs_mgr_init()
 *
 * Returns a pointer to a dynamically allocated region holding
 * connection information for a given manager.
 */
static struct sockaddr *pvfs_mgr_init(pvfs_char_t host[], uint16_t port,
pvfs_xferproto_t proto)
{
	struct sockaddr *sp;

	if (proto != PVFS_PROTO_TCP) return NULL;

	sp = (struct sockaddr *)malloc(sizeof(struct sockaddr));
	if (sp == NULL) return NULL;

	if (init_sock(sp, host, port) < 0) goto init_mgr_conn_error;
	return sp;

init_mgr_conn_error:
	free(sp);
	return NULL;
}

/* do_pvfs_op(mgr, op, resp)
 *
 * Exported function for performing PVFS operations.
 *
 * We leave the error in the downcall as is, and pass back any errors
 * returned from the underlying functions instead.  The upper layer can
 * do what they want with that error value.
 *
 * Returns 0 on success, -errno on failure.
 */
int do_pvfs_op(struct pvfs_upcall *op, struct pvfs_downcall *resp)
{
	int error = 0, port, retry = 0;
	struct sockaddr *mgr = NULL;
	char host[PVFSHOSTLEN];

	if (strlen(op->v1.fhname) == 0) return -EINVAL;

	port = name_to_port(op->v1.fhname);
	hostcpy(host, op->v1.fhname);
	if ((mgr = pvfs_mgr_init(host, port, PVFS_PROTO_TCP)) == NULL) 
		return -ENXIO;

do_pvfs_op_retry:
	switch(op->type) {
		case SETMETA_OP:
			error = do_setmeta_op(mgr, op, resp);
			break;
		case READ_OP:
		case WRITE_OP:
			error = do_rw_op(mgr, op, resp);
			break;
		case GETDENTS_OP:
			error = do_getdents_op(mgr, op, resp);
			break;
		case CREATE_OP:
			error = do_create_op(mgr, op, resp);
			break;
		case HINT_OP:
			error = do_hint_op(mgr, op, resp);
			break;
		default:
			error = do_generic_op(mgr, op, resp);
	}
	if (error == ENFILE || error == EMFILE) {
		/* need to close some files */
		PDEBUG(D_LLEV, "do_pvfs_op: closing files and retrying\n");
		close_some_files();
		if (retry++ < PVFS_XFER_MAX_RETRY) goto do_pvfs_op_retry;
	}
	free(mgr);
	return error;
}

/* pvfs_comm_idle()
 *
 * This incarnation of this function uses a standard "two strikes and
 * you're out" sort of deal.  First we look for any files with the ltime
 * set to 0.  These get removed.  Then we set all the remaining files
 * ltimes to 0.
 *
 * It might be best to add some sort of check so that we don't remove
 * files from the list "too often".  Perhaps this should be a parameter
 * to the function?
 *
 * We're going to ignore errors in here.
 */
void pvfs_comm_idle(void)
{
	int port;
	struct sockaddr *mgr = NULL;
	struct pf *old;
	time_t time = 0;
	char host[PVFSHOSTLEN];

	/* search out and remove all the old (zero'd) entries */
	while ((old = (struct pf *) llist_search(file_list,
	(void *) &time, pf_ltime_cmp)) != NULL)
	{
		pf_rem(file_list, old->handle, old->name); /* takes out of list */

		/* close the file */
		port = name_to_port(old->name);
		hostcpy(host, old->name);
		if ((mgr = pvfs_mgr_init(host, port, PVFS_PROTO_TCP)) == NULL) 
			return;
		close_pvfs_file(mgr, old);
		free(mgr);
		
		/* free the file structure */
		pf_free(old);
	}

	/* zero everyone else */
	llist_doall(file_list, pf_zerotime);
}

/* close_some_files()
 *
 * Returns -errno on error, number of files closed on success (can be 0).
 */
static int close_some_files(void)
{
	int i = 0, j, port;
	struct sockaddr *mgr = NULL;
	struct pf *old;
	time_t t = 0;
	char host[PVFSHOSTLEN];

	/* first we'll look for files that are already marked for removal */
	while ((old = (struct pf *) llist_search(file_list,
	(void *) &t, pf_ltime_cmp)) != NULL)
	{
		i++;

		pf_rem(file_list, old->handle, old->name); /* takes out of list */

		/* close the file */
		port = name_to_port(old->name);
		hostcpy(host, old->name);
		if ((mgr = pvfs_mgr_init(host, port, PVFS_PROTO_TCP)) == NULL) 
			return -errno;
		close_pvfs_file(mgr, old);
		free(mgr);
		
		/* free the file structure */
		pf_free(old);
	}

	/* if we got anything, let's go ahead and return */
	if (i > 0) return i;

	/* next we'll look for files of decreasing age */
	for (j = 20; j > 0; j-= 5) {
		t = time(NULL) - j;
		while ((old = (struct pf *) llist_search(file_list,
		(void *) &t, pf_ltime_olderthan)) != NULL)
		{
			i++;

			pf_rem(file_list, old->handle, old->name); /* takes out of list */
			PDEBUG(D_FILE, "closing %s\n", old->name);

			/* close the file */
			port = name_to_port(old->name);
			hostcpy(host, old->name);
			if ((mgr = pvfs_mgr_init(host, port, PVFS_PROTO_TCP)) == NULL) 
				return -1;
			close_pvfs_file(mgr, old);
			free(mgr);
			
			/* free the file structure */
			pf_free(old);
		}
		if (i > 0) return i;
	}

	/* finally we give up */
	return 0;
}

/* pf_zerotime()
 *
 * Sets the time value in a file structure (pf) to zero/null.
 *
 * This needs to be a function so we can pass it to llist_doall().
 */
static int pf_zerotime(void *pfp)
{
	((struct pf *) pfp)->ltime = 0;
	return 0;
}

/* close_pvfs_file()
 *
 * Handles sending a close request to a manager.  All local data
 * management must be handled at a higher level.
 */
static void close_pvfs_file(struct sockaddr *mgr, struct pf *pfp)
{
	int i, open_iod_conn = 0;
	mreq req;
	mack ack;
	ireq iodreq;

	for (i=0; i < pfp->fp->fd.meta.p_stat.pcount; i++) {
		if (pfp->fp->fd.iod[i].sock < 0) continue;
		open_iod_conn = 1;
		break;
	}

	/* NOTE:
	 * This has hung at least once when no connections were open to begin
	 * with; haven't been able to replicate consistently, but it can happen.
	 * This test to see if we have any connections open isn't going to
	 * fix the bug; however, it might allow us to avoid it as the only
	 * time I've seen the bug occur was when there were no open
	 * connections.  It will also make closes fast for the case where
	 * no I/O was done.  Is that going to happen often?  I dunno.  
	 * Probably not. -- Rob
	 */
	if (open_iod_conn) {
		/* we send a close request to all the iods; again we're going to
		 * let any errors go.
		 */
		memset(&iodreq, 0, sizeof(iodreq));
		iodreq.majik_nr = IOD_MAJIK_NR;
		iodreq.type = IOD_CLOSE;
		iodreq.req.close.f_ino = pfp->fp->fd.meta.u_stat.st_ino;
		iodreq.req.close.cap = pfp->fp->fd.cap;
		iodreq.dsize = 0;
		if (active_p == NULL) active_p = jlist_new();
		initset(&socks); /* clears out socket set */

		if (build_simple_jobs(pfp->fp, &iodreq) < 0) {
			perror("error building simple job");
			return;
		}

		while (jlist_empty(active_p) == 0) {
			if (do_jobs(active_p, &socks, -1) < 0) {
				PDEBUG(D_LLEV, "non-fatal error in do_jobs\n");
				break;
			}
		}

		/* finally we explicitly close the iod sockets */
		for (i=0; i < pfp->fp->fd.meta.p_stat.pcount; i++) {
			if (pfp->fp->fd.iod[i].sock >= 0) {
				close(pfp->fp->fd.iod[i].sock);
				pfp->fp->fd.iod[i].sock = -1;
			}
		}
	}

	/* initialize request to manager */
	memset(&req, 0, sizeof(req));
	req.majik_nr = MGR_MAJIK_NR;
	req.uid = 0; /* root */
	req.gid = 0; /* root */
	req.type = MGR_CLOSE;
	req.req.close.meta.fs_ino = pfp->fp->fd.meta.fs_ino;
	req.req.close.meta.u_stat.st_ino = pfp->fp->fd.meta.u_stat.st_ino;
	req.dsize = 0;

	/* send_mreq_saddr() handles receiving ack and checking error value,
	 * but we really don't care so much in this case.  We're going to let
	 * any errors just slide.
	 */
	send_mreq_saddr(mgr, &req, NULL, &ack);
	
	return;
}

/* pvfs_comm_shutdown()
 *
 * Clean up after ourselves.
 */
void pvfs_comm_shutdown(void)
{
	struct pf *head;

	/* remove all entries, handling each one individually */
	while ((head = pfl_head(file_list)) != NULL) {
		pf_rem(file_list, head->handle, head->name); /* takes out of list */
		pf_free(head); /* frees memory */
	}

	pfl_cleanup(file_list);
	file_list = NULL;
	return;
}

/* INTERNAL FUNCTIONS FOR PVFS OPERATIONS */

/* do_generic_op(mgr, op, resp)
 *
 * Handles the "easy" cases:
 *   GETMETA_OP, LOOKUP_OP, REMOVE_OP, RENAME_OP, MKDIR_OP, RMDIR_OP
 */
static int do_generic_op(struct sockaddr *mgr, struct pvfs_upcall *op,
struct pvfs_downcall *resp)
{
	int error = 0;
	mreq req;
	mack ack;
	char *fn = NULL;

	init_mgr_req(&req, op);
	init_res(resp, op);

	switch(op->type) {
		case GETMETA_OP:
			req.type = MGR_STAT;
			fn = skip_to_filename(op->v1.fhname);
			req.dsize = strlen(fn); /* don't count terminator */
			break;
		case LOOKUP_OP:
			fn = skip_to_filename(op->u.lookup.name);
			req.dsize = strlen(fn); /* don't count terminator */
			req.type = MGR_STAT;
			break;
		case REMOVE_OP:
			fn = skip_to_filename(op->v1.fhname);
			req.dsize = strlen(fn); /* don't count terminator */
			req.type = MGR_UNLINK;
			break;
		case RENAME_OP:
			req.type = MGR_RENAME;
			/* here we have to actually send BOTH names out to the manager.
			 * This is done with a single buffer using a null terminator
			 * between the two strings to indicate where one ends and the
			 * next begins.
			 */
			req.dsize = strlen(skip_to_filename(op->v1.fhname)) +
			strlen(skip_to_filename(op->u.rename.new_name)) + 2;
			if ((fn = (char *)malloc(req.dsize)) == NULL) return -ENOMEM;
			strcpy(fn, skip_to_filename(op->v1.fhname));
			strcpy(fn + strlen(skip_to_filename(op->v1.fhname)) + 1,
			skip_to_filename(op->u.rename.new_name));
			break;
		case MKDIR_OP:
			fn = skip_to_filename(op->u.mkdir.name);
			req.dsize = strlen(fn); /* don't count terminator */
			req.type = MGR_MKDIR;
			req.req.mkdir.mode = op->u.mkdir.meta.mode;
			break;
		case RMDIR_OP:
			fn = skip_to_filename(op->u.rmdir.name);
			req.dsize = strlen(fn); /* don't count terminator */
			req.type = MGR_RMDIR;
			break;
		case STATFS_OP:
			fn = skip_to_filename(op->v1.fhname);
			req.dsize = strlen(fn); /* don't count terminator */
			req.type = MGR_STATFS;
			break;
		default:
			error = -ENOSYS;
			goto do_generic_op_error;
	}
	/* note: send_mreq_saddr() is a mgrcomm.c call.  It handles opening
	 * connections when necessary and so on.  All we need to do is make
	 * sure that the address we pass to it is ready to go, which is
	 * handled at a higher layer.
	 *
	 * send_mreq_saddr() returns the integer value of the open file
	 * descriptor on success.  This is used to receive additional data
	 * when trailing data is indicated in the ack.  Otherwise it should
	 * be simply treated as an indicator that the call succeeded.  In
	 * case of failure -1 is returned.  IN ANY CASE, THE FD SHOULD NOT BE
	 * CLOSED.  See mgrcomm.c for more information.
	 */
	if (send_mreq_saddr(mgr, &req, fn, &ack) < 0) {
		error = -errno;
		goto do_generic_op_error;
	}

	switch(op->type) {
		case GETMETA_OP:
			v1_fmeta_to_v2_meta(&ack.ack.stat.meta, &(resp->u.getmeta.meta));
			v1_fmeta_to_v2_phys(&ack.ack.stat.meta, &(resp->u.getmeta.phys));
			break;
		case LOOKUP_OP:
			v1_fmeta_to_v2_meta(&ack.ack.stat.meta, &(resp->u.lookup.meta));
			break;
		case RENAME_OP:
			free(fn);
			break;
		case MKDIR_OP:
			/* call do_mkdir_post() to handle setting the owner of the dir */
			if ((error = do_mkdir_post(mgr, op, resp)) < 0)
				goto do_generic_op_error;
			break;
		case REMOVE_OP:
		case RMDIR_OP:
			/* all that is needed is the return value */
			break;
		case STATFS_OP:
			v1_ack_to_v2_statfs(&ack,&(resp->u.statfs.statfs));
			break;
		default:
			/* can't get here */
			error = -ENOSYS;
			goto do_generic_op_error;
	}
	resp->error = 0;
	return 0;

do_generic_op_error:
	resp->error = error;
	return error;
}

/* do_mkdir_post()
 *
 * Handles the ancillary operations necessary for correct mkdir
 * operation.  Currently this means that we call do_setmeta_op()
 * to set the owner and group appropriately.
 *
 * The mode is set already by the do_generic_op() call.
 */
static int do_mkdir_post(struct sockaddr *mgr, struct pvfs_upcall *op,
struct pvfs_downcall *resp)
{
	int error;
	struct pvfs_upcall smup;
	struct pvfs_downcall smdown;

	/* set up the upcall */
	memset(&smup, 0, sizeof(smup));
	smup.magic = op->magic;
	smup.seq = op->seq;
	smup.type = SETMETA_OP;
	smup.proc = op->proc;
	smup.u.setmeta.meta.handle = op->u.mkdir.meta.handle;
	smup.u.setmeta.meta.valid = V_UID | V_GID;
	smup.u.setmeta.meta.uid = op->u.mkdir.meta.uid;
	smup.u.setmeta.meta.gid = op->u.mkdir.meta.gid;
	strcpy(smup.v1.fhname, op->v1.fhname);
	
	error = do_setmeta_op(mgr, &smup, &smdown);
	return error;
}

/* do_hint_op(mgr, op, resp)
 *
 */
static int do_hint_op(struct sockaddr *mgr, struct pvfs_upcall *op,
struct pvfs_downcall *resp)
{
	struct pf *pfp;

	init_res(resp, op);

	switch (op->u.hint.hint) {
		case HINT_CLOSE:
			/* find the file in our list, close it, remove from list */
			pfp = pf_search(file_list, op->u.hint.handle, op->v1.fhname);
			if (pfp == NULL) return 0;
			pf_rem(file_list, pfp->handle, pfp->name);
			close_pvfs_file(mgr, pfp);
			pf_free(pfp);
			break;
		case HINT_OPEN:
			/* don't do anything; we'll open the file when I/O is started */
		default:
		break;
	}
	return 0;
}


/* do_create_op(mgr, op, resp)
 */
static int do_create_op(struct sockaddr *mgr, struct pvfs_upcall *op,
struct pvfs_downcall *resp)
{
	int error = 0;
	mreq req;

	init_mgr_req(&req, op);
	init_res(resp, op);

	if ((error = create_pvfs_file(mgr, op->u.create.name,
	&(op->u.create.meta), &(op->u.create.phys))) < 0) 
		goto do_create_op_error;

do_create_op_error:
	resp->error = error;
	return error;
}

/* do_setmeta_op(mgr, op, resp)
 *
 * Notes:
 * This is more complicated than it should be.  The problem here is that
 * we don't have a "setmeta" operation in PVFS v1.  We instead have
 * chown, chmod, and utime.
 *
 * First we perform a GETMETA request to obtain the current metadata and
 * also to grab the pvfs_phys which we must return (and the v1 calls
 * don't return this for ANY of the calls we would perform in here).
 * This has the added bonus of giving us the opportunity to avoid calls
 * which wouldn't result in any modification to the metadata (ie. we can
 * compare the existing values to the new ones).
 */
static int do_setmeta_op(struct sockaddr *mgr, struct pvfs_upcall *op,
struct pvfs_downcall *resp)
{
	int error = 0;
	char *fn;
	mreq req;
	mack ack;

	struct pvfs_upcall gmup;
	struct pvfs_downcall gmres;

	/* perform a GETMETA first */
	memset(&gmup, 0, sizeof(gmup));
	gmup.magic = op->magic;
	gmup.seq = op->seq;
	gmup.type = GETMETA_OP;
	gmup.proc = op->proc;
	gmup.u.getmeta.handle = op->u.setmeta.meta.handle;
	strcpy(gmup.v1.fhname, op->v1.fhname);

	error = do_generic_op(mgr, &gmup, &gmres);
	if (error < 0) goto do_setmeta_op_error;

	/* we initialize the result and copy in the final values. */
	init_res(resp, op);
	resp->u.setmeta.meta = gmres.u.getmeta.meta;
	resp->u.setmeta.phys = gmres.u.getmeta.phys;

	fn = skip_to_filename(op->v1.fhname);

	if (op->u.setmeta.meta.valid & V_MODE
	&& op->u.setmeta.meta.mode != gmres.u.getmeta.meta.mode)
	{
		init_mgr_req(&req, op);
		req.type = MGR_CHMOD;
		req.dsize = strlen(fn); /* don't count terminator */
		req.req.chmod.mode = op->u.setmeta.meta.mode;

		if (send_mreq_saddr(mgr, &req, fn, &ack) < 0) {
			error = -errno;
			goto do_setmeta_op_error;
		}

		/* save new value in the response */
		resp->u.setmeta.meta.mode = op->u.setmeta.meta.mode;
	}

	/* this is a little convoluted, but it keeps us from doing separate
	 * requests for owner and group changes.
	 */
	if (op->u.setmeta.meta.valid & (V_UID | V_GID)) {
		int changing_something = 0;

		init_mgr_req(&req, op);
		req.type = MGR_CHOWN;
		req.dsize = strlen(fn);
		if (op->u.setmeta.meta.valid & V_UID 
		&& op->u.setmeta.meta.uid != gmres.u.getmeta.meta.uid) {
			changing_something = 1;
			req.req.chown.owner = op->u.setmeta.meta.uid;
			resp->u.getmeta.meta.uid = op->u.setmeta.meta.uid;
		}
		else
			req.req.chown.owner = -1;

		if (op->u.setmeta.meta.valid & V_GID
		&& op->u.setmeta.meta.gid != gmres.u.getmeta.meta.gid) {
			changing_something = 1;
			req.req.chown.group = op->u.setmeta.meta.gid;
			resp->u.getmeta.meta.gid = op->u.setmeta.meta.gid;
		}
		else
			req.req.chown.group = -1;

		if (changing_something) {
			if (send_mreq_saddr(mgr, &req, fn, &ack) < 0) {
				error = -errno;
				goto do_setmeta_op_error;
			}
		}

		/* save new values in the response */
		if (op->u.setmeta.meta.valid & V_UID)
			resp->u.setmeta.meta.uid = op->u.setmeta.meta.uid;
		if (op->u.setmeta.meta.valid & V_GID)
			resp->u.setmeta.meta.gid = op->u.setmeta.meta.gid;
	}

	/* this one is almost always going to change (I think), so we're
	 * going to just do it for now.
	 */
	if (op->u.setmeta.meta.valid & V_TIMES) {
		init_mgr_req(&req, op);
		req.type = MGR_UTIME;
		req.dsize = strlen(fn);
		req.req.utime.buf.actime = op->u.setmeta.meta.atime;
		req.req.utime.buf.modtime = op->u.setmeta.meta.mtime;

		if (send_mreq_saddr(mgr, &req, fn, &ack) < 0) {
			error = -errno;
			goto do_setmeta_op_error;
		}

		resp->u.setmeta.meta.atime = op->u.setmeta.meta.atime;
		resp->u.setmeta.meta.ctime = op->u.setmeta.meta.atime;
		resp->u.setmeta.meta.mtime = op->u.setmeta.meta.mtime;
	}

	if (op->u.setmeta.meta.valid & V_SIZE
	&& op->u.setmeta.meta.size != gmres.u.getmeta.meta.size)
	{
		/* Hmm...is this a truncate? */
		init_mgr_req(&req, op);
		req.type = MGR_TRUNCATE;
		req.dsize = strlen(fn);
		req.req.truncate.length = op->u.setmeta.meta.size;

		if (send_mreq_saddr(mgr, &req, fn, &ack) < 0) {
			error = -errno;
			goto do_setmeta_op_error;
		}

		resp->u.setmeta.meta.size = op->u.setmeta.meta.size;
	}

	if (gmres.u.getmeta.phys.blksize > 0) {
		resp->u.setmeta.meta.blksize = gmres.u.getmeta.phys.blksize;
	}
	else {
		resp->u.setmeta.meta.blksize = 512;
		resp->u.setmeta.phys.blksize = 512;
	}
	resp->u.setmeta.meta.blocks =
		(resp->u.setmeta.meta.size / resp->u.setmeta.meta.blksize) +
		((resp->u.setmeta.meta.size % resp->u.setmeta.meta.blksize) ? 1 : 0);
	resp->u.setmeta.meta.valid =
		V_MODE|V_UID|V_GID|V_SIZE|V_TIMES|V_BLKSIZE|V_BLOCKS;
	resp->error = 0;
	return 0;

do_setmeta_op_error:
	resp->error = error;
	return error;
}

/* do_getdents_op(mgr, op, resp)
 *
 * Send a PVFS getdents message, return the results in a buffer defined
 * by the caller.
 *
 * NOTES:
 * PVFS getdents operations now include a count instead of a size,
 * indicating the number of records instead of the size of the region to
 * receive into.  Since our new dirent records are of a fixed size, this
 * will be fine in that we know how big a buffer we will need.
 *
 * However, we kinda have to guess what size buffer we will need to
 * receive the information from the manager; the v1 implmentation of
 * getdents() is rather primitive.
 *
 * This call relies on the op->xfer.ptr and op->xfer.size values being
 * set to describe a buffer region in which to put the resulting data.
 * It does not check the validity of the op->xfer.ptr value.
 *
 * Returns 0 on success, -errno on failure.  resp->xfer.ptr and
 * resp->xfer.size are set to describe the resulting buffer on success.
 */
static int do_getdents_op(struct sockaddr *mgr, struct pvfs_upcall *op,
struct pvfs_downcall *resp)
{
	int error = 0, sock = -1;
	pvfs_size_t res_dsize, min_size;
	mreq req;
	mack ack;
	char *fn;
	void *darray = NULL;

	init_mgr_req(&req, op);
	fn = skip_to_filename(op->v1.fhname);
	req.dsize = strlen(fn); /* don't count terminator */
	init_res(resp, op);

	req.type = MGR_GETDENTS;
	req.req.getdents.offset = op->u.getdents.off;
	res_dsize = op->u.getdents.count * sizeof(struct pvfs_dirent);
	req.req.getdents.length = res_dsize;

	req.dsize = strlen(fn); /* don't count terminator */
	if ((sock = send_mreq_saddr(mgr, &req, fn, &ack)) < 0) {
		error = -errno;
		goto do_getdents_op_error;
	}

	if (ack.dsize == 0) /* EOF */ {
		resp->u.getdents.eof = 1;
	}
	else /* read the trailing dirent data */ {
		pvfs_count_t count = op->u.getdents.count;
		pvfs_off_t off = op->u.getdents.off;

		if ((darray = (void *)malloc(res_dsize)) == NULL) {
			error = -ENOMEM;
			goto do_getdents_op_error;
		}

		if ((error = recv_dirents(sock, &off, ack.dsize, darray, &count)) < 0)
			goto do_getdents_op_error;

		/* transfer-specific elements, passed back from recv_dirents() */
		resp->u.getdents.off = off;
		resp->u.getdents.count = count;
		if (count == 0) {
			/* if we didn't get any entries, go ahead and free the space */
			free(darray);
			darray = NULL;
			res_dsize = 0;
		}
		else if (op->u.getdents.count > count) {
			/* if we got fewer entries than we expected, realloc() and
			 * save the extra
			 */
			res_dsize = count * sizeof(struct pvfs_dirent);
			darray = realloc(darray, res_dsize);
		}

		/* copy entries into buffer specified in the upcall */
		min_size = (res_dsize > op->xfer.size) ? res_dsize : op->xfer.size;
		memcpy(op->xfer.ptr, darray, min_size);
		resp->xfer.ptr = op->xfer.ptr;
		resp->xfer.size = min_size;
		free(darray);
	}

	resp->error = 0;
	return 0;

do_getdents_op_error:
	resp->error = error;
	free(darray);
	return error;
}

/* do_rw_op(mgr, op, resp)
 *
 * NOTES:
 * I/O daemons in v1 associate every instance of an open file with some
 * socket.  That is, for every file structure they have around, they
 * have a socket associated with that file.  There can, however, be more
 * than one file associated with a given socket -- that isn't a problem.
 *
 * Our goal here will be to use the same sockets over again when a file
 * is opened more than once.  That's not really likely to happen though,
 * unless someone is running multiple application tasks on the same
 * machine (which could eventually be commonplace).  So we're going to
 * end up with lots of connections around.
 *
 * In the long run (ie. v2) we would like to have one or more sets of
 * connections to the I/O daemons that we use for any communication
 * instead of this one set per file nonsense.  For now it is easier to
 * stick with the one per file method.  We'll try to encapsulate things
 * better next time...
 *
 * Returns -errno on failure, 0 on success.
 */
static int do_rw_op(struct sockaddr *mgr, struct pvfs_upcall *op,
struct pvfs_downcall *resp)
{
	int i, type, error = 0;
	pvfs_size_t size = 0;
	struct pf *pfp;
	fdesc *fp;

	if (op->u.rw.io.type != IO_CONTIG) {
		error = -ENOSYS;
		goto do_rw_op_error;
	}
	
	/* make sure the file is open and ready for access */
	if ((pfp = pf_search(file_list, op->u.rw.handle, op->v1.fhname)) == NULL) {
		if ((error = open_pvfs_file(mgr, op->v1.fhname)) < 0)
			goto do_rw_op_error;

		/* NOTE: It SHOULD be the case that we never hit this error;
		 * however, we are in fact having this problem.  Until I can
		 * discover why this test is failing when the open succeeds, this
		 * workaround will be here to keep the pvfsd from segfaulting.
		 */
		if ((pfp = pf_search(file_list, op->u.rw.handle, op->v1.fhname)) == NULL)
		{
			PERROR("NULL returned from pf_search after successful open\n");
			goto do_rw_op_error;
		}
	}
	fp = pfp->fp;

	fp->fd.off = op->u.rw.io.u.contig.off;

	if (op->u.rw.io.u.contig.size != op->xfer.size)  {
		error = -EINVAL;
		goto do_rw_op_error;
	}

	init_res(resp, op);
	resp->xfer.ptr = op->xfer.ptr;

	/* build jobs */
	switch(op->type) {
		case READ_OP:
			type = IOD_RW_READ;
			break;
		case WRITE_OP:
			type = IOD_RW_WRITE;
			break;
		default:
			error = -EINVAL;
			goto do_rw_op_error;
	}

	for(i=0; i < fp->fd.meta.p_stat.pcount; i++) {
		fp->fd.iod[i].ack.dsize = 0;
		fp->fd.iod[i].ack.status = 0;
	}

	if (build_rw_jobs(fp, op->xfer.ptr, op->xfer.size, type) < 0) {
		error = -errno;
		goto do_rw_op_error;
	}
	
	/* keep working until jobs are done */
	while (jlist_empty(active_p) == 0) {
		if (do_jobs(active_p, &socks, -1) < 0) {
			error = errno;
			PERROR("do_jobs failed, errno = %d\n", error);
			error = -errno;
			goto do_rw_op_error;
		}
	}
	/* check all the return values */
	for (i=0; i < fp->fd.meta.p_stat.pcount; i++) {
		if (fp->fd.iod[i].ack.status) {
			error = -(fp->fd.iod[i].ack.eno);
			goto do_rw_op_error;
		}
		size += fp->fd.iod[i].ack.dsize;
	}

	if(size > op->xfer.size){
		PERROR("v1_xfer write size mismatch!\n");
		PERROR("returned size reported: %ld\n", (long)size);
		PERROR("returned size set to: %ld\n", (long)op->xfer.size);
		size = op->xfer.size;
	}
	resp->xfer.size = size;
	resp->u.rw.size = size;

do_rw_op_error:
	resp->error = error;
	return error;
}

/*** INTERNAL, SUPPORT FUNCTIONS BELOW HERE ***/

/* create_pvfs_file(mgr, name, meta, phys)
 *
 * Create a new file with the given physical distribution and metadata.
 *
 * NOTE:
 * Actually only certain values of the metadata are used, in particular
 * the mode and so on.  I'm not going to talk too much about that here,
 * because I don't remember off the top of my head exactly what values
 * in there are actually used.  If you really want to know, you should
 * look at the manager code.
 *
 * This function takes the same name format as the do_XXX_op calls, as
 * it needs the manager information for adding to the open file list.
 *
 * Returns 0 on success error value (< 0) on failure.
 */
static int create_pvfs_file(struct sockaddr *mgr, pvfs_char_t name[],
struct pvfs_meta *meta, struct pvfs_phys *phys)
{
	int error = 0;
	mreq req;
	fdesc *fp = NULL;

	/* initialize request to manager */
	memset(&req, 0, sizeof(req));
	req.majik_nr = MGR_MAJIK_NR;
	req.uid = 0; /* root */
	req.gid = 0; /* root */
	req.type = MGR_OPEN;
	req.req.open.flag = O_RDWR | O_CREAT | O_EXCL;
	req.req.open.meta.fs_ino = 0; /* probably needs to be set... */
	v2_meta_to_v1_fmeta(meta, &(req.req.open.meta));
	v2_phys_to_v1_fmeta(phys, &(req.req.open.meta));
	req.dsize = 0; /* set in do_open_req() */

	/* send the request, save the open file information */
	if ((fp = do_open_req(mgr, &req, name, &error)) == NULL)
		return error;

	/* open the IOD connections */
	ping_iods(fp);

	return 0;
}

/* open_pvfs_file(mgr, name)
 *
 * Opens the PVFS file named "name" held by the manager referenced by
 * "mgr".  Only operates on regular files, not directories.
 *
 * Finally we connect to the IODs.  This is the point where the eepro
 * driver craps out <smile>.
 *
 * Returns error value (negative) on failure, 0 on success.
 */
static int open_pvfs_file(struct sockaddr *mgr, pvfs_char_t name[])
{
	int error = 0;
	mreq req;
	fdesc *fp = NULL;

	/* initialize request to manager */
	memset(&req, 0, sizeof(req));
	req.majik_nr = MGR_MAJIK_NR;
	req.uid = 0; /* root */
	req.gid = 0; /* root */
	req.type = MGR_OPEN;
	req.req.open.flag = O_RDWR;
	req.req.open.meta.fs_ino = 0; /* probably needs to be set... */
	req.dsize = 0; /* set in do_open_req() */

	/* send the request, save the open file information */
	if ((fp = do_open_req(mgr, &req, name, &error)) == NULL)
		return error;

	/* open the IOD connections */
	ping_iods(fp);

	return 0;
}

/* do_open_req()
 *
 * Used by create_pvfs_file() and open_pvfs_file() to communicate with
 * the manager and allocate any necessary memory.
 *
 * This function also takes the long name format, as it is used when
 * storing the open file information.
 *
 * First we send an open request to the manager.  This results in the
 * IOD addresses being returned to us (on success).
 *
 * Next we store up all this information so we can connect to these
 * guys again later.  We have to store it in the fdesc format because
 * that's what we'll be passing into build_rw_jobs() inside of
 * do_rw_op().  This is a little sad, but it's the easiest thing to do.
 *
 * Returns a pointer to the fdesc structure on success, NULL on failure.
 * In the event of failure, the error value is returned in the region
 * pointed to by errp.
 *
 */
static fdesc *do_open_req(struct sockaddr *mgr, mreq *reqp, pvfs_char_t name[], int *errp)
{
	int error = 0, sock, ct, i;
	fdesc *fp;
	mack ack;
	pvfs_handle_t handle;
	struct pf *p = NULL;
	char *fn;

   /* send off the request to the manager */
	fn = skip_to_filename(name);
	reqp->dsize = strlen(fn); /* have to fix data length to match name */
	if ((sock = send_mreq_saddr(mgr, reqp, fn, &ack)) < 0) {
		*errp = -errno;
		return NULL;
	}
	/* check the dsize, pcount, and sanity */
	if (ack.dsize == 0) /* badness */ {
		*errp = -EBADMSG;
		return NULL;
   }
	if ((ct = ack.ack.open.meta.p_stat.pcount) < 1) {
		*errp = -EINVAL;
		return NULL;
   }
	if (ack.dsize != sizeof(iod_info) * ct) {
		PERROR("size mismatch receiving iod info\n");
		*errp = -EINVAL;
		return NULL;
	}

	/* receive and fill in the file descriptor structure */
	if ((fp = (fdesc *)malloc(sizeof(*fp) + sizeof(iod_info)*(ct-1))) == NULL)
   {
		*errp = -errno;
		return NULL;
   }
	memset(fp, 0, sizeof(*fp) + sizeof(iod_info)*(ct-1));
	
	if (brecv(sock, fp->fd.iod, ack.dsize) < ack.dsize) {
		error = -errno;
		goto open_pvfs_file_error;
	}

	for (i = 0; i < ct; i++) fp->fd.iod[i].sock = -1;

	fp->fd.meta = ack.ack.open.meta;
	fp->fd.cap = ack.ack.open.cap;
	fp->fd.off = 0;
	fp->fs = FS_PVFS;
	handle = ack.ack.open.meta.u_stat.st_ino;

	/* store fdesc in our file list for use later */
	if (file_list == NULL) file_list = pfl_new();
	if (file_list == NULL) goto open_pvfs_file_error;
	if ((p = pf_new(fp, handle, name)) == NULL) goto open_pvfs_file_error;
	if (pf_add(file_list, p) < 0) goto open_pvfs_file_error;

	return 0;

open_pvfs_file_error:
	if (p != NULL) {
		pf_free(p);
		fp = NULL;
	}
	free(fp);
	*errp = error;
	return NULL;
}

/* ping_iods()
 *
 * Sends a NOOP message to all the iods for an open file.  Used by the
 * open routines to initiate connections between the application and the
 * iods at file open time.  This makes I/O performance more consistent.
 */
static void ping_iods(fdesc *fp)
{
	ireq iodreq;

	memset(&iodreq, 0, sizeof(iodreq));
	iodreq.majik_nr = IOD_MAJIK_NR;
	iodreq.type = IOD_NOOP;
	iodreq.dsize = 0;
	if (active_p == NULL) active_p = jlist_new();
	initset(&socks); /* clears out socket set */

	if (build_simple_jobs(fp, &iodreq) < 0) {
		perror("error building simple job");
		return;
	}

	while (jlist_empty(active_p) == 0) {
		if (do_jobs(active_p, &socks, -1) < 0) {
			PDEBUG(D_LLEV, "non-fatal error in do_jobs\n");
			break;
		}
	}
	
	return;
}


/* init_mgr_req()
 *
 * initializes fields in the manager request
 */
static void init_mgr_req(mreq *rp, struct pvfs_upcall *op)
{
	memset(rp, 0, sizeof(*rp));
	rp->majik_nr = MGR_MAJIK_NR;
	rp->uid = op->proc.uid;
	rp->gid = op->proc.gid;
	return;
}

/* init_res() 
 *
 * initializes the fields in a struct pvfs_downcall
 */
static void init_res(struct pvfs_downcall *resp, struct pvfs_upcall *op)
{
	memset(resp, 0, sizeof(*resp));
	resp->magic = PVFS_DOWNCALL_MAGIC;
	resp->seq = op->seq;
	resp->type = op->type;
	return;
}

/* v1_fmeta_to_v2_meta()
 *
 * Copy values from a v1 fmeta structure into a v2 meta structure.
 */
static void v1_fmeta_to_v2_meta(struct fmeta *fmeta, struct pvfs_meta *meta)
{
	meta->valid = V_MODE|V_UID|V_GID|V_SIZE|V_TIMES|V_BLKSIZE|V_BLOCKS;


	meta->handle = fmeta->u_stat.st_ino;
	meta->mode = fmeta->u_stat.st_mode;
	meta->uid = fmeta->u_stat.st_uid;
	meta->gid = fmeta->u_stat.st_gid;
	meta->size = fmeta->u_stat.st_size;
	meta->atime = fmeta->u_stat.st_atime;
	meta->mtime = fmeta->u_stat.st_mtime;
	meta->ctime = fmeta->u_stat.st_ctime;
	meta->blksize = fmeta->p_stat.ssize;
	if (fmeta->p_stat.ssize) {
		meta->blocks = fmeta->u_stat.st_size / fmeta->p_stat.ssize + 
			(fmeta->u_stat.st_size % fmeta->p_stat.ssize ? 1 : 0);
	}
	else meta->blocks = 0;

	return;
}

/* v2_meta_to_v1_fmeta()
 *
 * Copy values from a v2 pvfs_meta structure into a v1 fmeta structure.
 */
static void v2_meta_to_v1_fmeta(struct pvfs_meta *meta, struct fmeta *fmeta)
{
	memset(fmeta, 0, sizeof(*fmeta));
	fmeta->u_stat.st_ino = meta->handle;
	if (meta->valid & V_MODE) fmeta->u_stat.st_mode = meta->mode;
	if (meta->valid & V_UID) fmeta->u_stat.st_uid = meta->uid;
	if (meta->valid & V_GID) fmeta->u_stat.st_gid = meta->gid;
	if (meta->valid & V_SIZE) fmeta->u_stat.st_size = meta->size;
	if (meta->valid & V_TIMES) {
		fmeta->u_stat.st_atime = meta->atime;
		fmeta->u_stat.st_mtime = meta->mtime;
		fmeta->u_stat.st_ctime = meta->ctime;
	}
#if 0
	if (meta->valid & V_SIZE) fmeta->p_stat.ssize = meta->blksize;
#endif

	return;
}

/* v1_fmeta_to_v2_phys()
 *
 * Copy values from a v1 fmeta structure into a v2 pvfs_phys structure.
 */
static void v1_fmeta_to_v2_phys(struct fmeta *fmeta, struct pvfs_phys *phys)
{
	phys->blksize = fmeta->p_stat.ssize;
	phys->nodect = fmeta->p_stat.pcount;
	phys->dist = DIST_RROBIN;
	return;
}

/* v2_phys_to_v1_fmeta()
 *
 * Copy values from a v2 pvfs_phys structure into a v1 fmeta structure.
 */
static void v2_phys_to_v1_fmeta(struct pvfs_phys *phys, struct fmeta *fmeta)
{
	if (phys->blksize > 0)
		fmeta->p_stat.ssize = phys->blksize;
	else
		fmeta->p_stat.ssize = -1;
	if (phys->nodect > 0) 
		fmeta->p_stat.pcount = phys->nodect;
	else
		fmeta->p_stat.pcount = -1;

	/* for now we won't try to do anything with the iod lists */
	fmeta->p_stat.base = -1;
	fmeta->p_stat.bsize = -1;
	return;
}

/* v1_ack_to_v2_statfs()
 *
 * Copy values from v1 ack structure into a v2 pvfs_statfs structure
 */
static void v1_ack_to_v2_statfs(struct mack *ack, struct pvfs_statfs *sfs)
{
	sfs->bsize = PVFS_OPT_IO_SIZE;
	sfs->blocks = ack->ack.statfs.tot_bytes / (pvfs_size_t) PVFS_OPT_IO_SIZE;
	sfs->bfree = ack->ack.statfs.free_bytes / (pvfs_size_t) PVFS_OPT_IO_SIZE;
	sfs->bavail = sfs->bfree;
	sfs->files = ack->ack.statfs.tot_files;
	sfs->ffree = ack->ack.statfs.free_files;
	sfs->namelen = ack->ack.statfs.namelen;
	return;
}

/* recv_dirents(fd, off, dsize, dlist, countp)
 *
 * PARAMETERS:
 * fd - file descriptor for manager connection with waiting getdents
 *   results
 * offp - pointer to offset passed in by application; start of first
 *   record; this is set to the offset of the next record on return
 * dsize - trailing data size, size of dirents being returned
 * dlist - pointer to array of pvfs_dirent structures to receive data
 * countp - pointer to number of pvfs_dirent structures in the dlist
 *   array; this is set to the number actually read on return
 *
 * NOTES:
 * It's important to check the bounds on the incoming data in case we
 * received a partial data structure in the dirent array.  To do this we
 * use the "unused" dirent to calculate the length to the end of the
 * d_reclen field in the dirent structure (from the beginning of one).
 *
 * This way we can check to see that we indeed have the d_reclen field
 * for a given dirent.  Once we know that we can be confident in
 * accessing the field to see if we have all of the name of the file
 * too.
 *
 * As an aside, this would be easier if we knew for a fact that the
 * server handed us back only whole, well formed records.  We should
 * require this in the v2 specification.  It shouldn't be a problem,
 * because we should be requesting records in terms of a count instead
 * of a byte length anyway.
 */
static int recv_dirents(int fd, pvfs_off_t *offp, pvfs_size_t dsize,
	struct pvfs_dirent dlist[], pvfs_count_t *countp)
{
	pvfs_off_t off;
	int error = 0, i;
	struct dirent *dp;
	char *vp, *start;
	struct dirent unused;
	int off_end_reclen;

	off = *offp;

	off_end_reclen = ((char *)&(unused.d_reclen) - (char *)&unused)
		+ sizeof(unused.d_reclen);
	
	/* we keep "start" pointing at the beginning of the array so we can
	 * free it later on.
	 */
	if ((start = vp = (char *)malloc(dsize)) == NULL) return -ENOMEM;

	if (brecv(fd, vp, dsize) < 0) return -errno;

	for (i=0; i < *countp; i++) {
		dp = (struct dirent *) vp;

		/* here we check to make sure we have the whole entry */
		if (vp + off_end_reclen > start + dsize) break;
		if (vp + dp->d_reclen > start + dsize) break;

		dlist->handle = dp->d_ino;
		/* NOTE: the offset in the pvfs_dirent is to the current entry,
		 * but the offset in the dirent is to the NEXT entry...
		 */
		dlist->off = off;
		dlist->len = dp->d_reclen;
		strncpy(dlist->name, dp->d_name, PVFSNAMELEN);
		off = dp->d_off;
		vp += dp->d_reclen;
		dlist++;
	}
	*offp = off;
	*countp = i;
	free(start);
	return error;
}

/* OPEN FILE STORAGE FUNCTIONS
 *
 * So what's the deal here?
 *
 * Well, basically we need a way to hold on to open file information.
 * So we build a list of open files which we can search by handle/name
 * combination.
 *
 * OPERATIONS:
 *
 * pfl_new() - called once, creates a list for us
 * pf_new() - hands back an initialized pf (pvfs file) structure
 * pf_add() - adds a pf structure into a pf list
 * pf_search() - looks for a pf matching a given file handle and name
 *
 * etc.
 */

static pfl_t pfl_new(void)
{
	return llist_new();
}

static struct pf *pf_new(fdesc *fp, pvfs_handle_t handle, char *name)
{
	struct pf *p;

	/* allocate space for the string along with the structure */
	p = (struct pf *)malloc(sizeof(*p) + strlen(name) + 1);
	if (p == NULL) return NULL;

	p->handle = handle;
	p->name = (char *) p + sizeof(*p);
	p->ltime = time(NULL);
	p->fp = fp;
	strcpy(p->name, name);
	return p;
}

static int pf_add(pfl_t pfl, struct pf *p)
{
	return llist_add(pfl, (void *) p);
}

/* pf_search() - Searches a list for a matching item using both the
 * handle and the full name (including manager and port).  Updates the
 * ltime field of the entry before returning.
 */
static struct pf *pf_search(pfl_t pfl, pvfs_handle_t handle, char *name)
{
	struct pf_cmp cmp;
	struct pf *ret;
	
	cmp.handle = handle;
	cmp.name = name;
	ret = (struct pf *) llist_search(pfl, (void *) &cmp, pf_handle_cmp);
	if (ret != NULL) ret->ltime = time(NULL);
	return ret;
}

static struct pf *pfl_head(pfl_t pfl)
{
	return (struct pf *) llist_head(pfl);
}

/* pf_handle_cmp(cmp, pfp)
 *
 * Compares both handle and manager name; Returns 0 on match, non-zero
 * if no match
 */
static int pf_handle_cmp(void *cmp, void *pfp)
{
	/* do a handle match first because it should be really quick */
	if (((struct pf_cmp *) cmp)->handle != ((struct pf *) pfp)->handle)
		return -1;
	
	/* if we get this far, compare the names using strcmp() */
	return strcmp(((struct pf_cmp *) cmp)->name, ((struct pf *) pfp)->name);
}

/* pf_ltime_olderthan(timep, pfp)
 *
 * Returns 0 if the time stored in the structure is less than (older
 * than) the time passed as comparison.
 */
static int pf_ltime_olderthan(void *time, void *pfp)
{
	if (((struct pf *) pfp)->ltime < *((time_t *) time)) return 0;
	return 1;
}

/* pf_ltime_cmp(timep, pfp)
 *
 * Returns the time value stored in the file structure minus the time
 * value pointed to by timep.  Thus the resulting value is positive if
 * the file has been touched since the time passed in and negative if
 * the file hasn't been touch since that time.
 */
static int pf_ltime_cmp(void *time, void *pfp)
{
	return (((struct pf *) pfp)->ltime - *((time_t *) time));
}

/* pf_rem(pfl, handle)
 *
 * Finds and removes an instance of an open file with matching handle
 * and manager.  Does not free the structure.
 *
 * Returns 0 on successful remove, -1 if no instance was found.
 */
static int pf_rem(pfl_t pfl, pvfs_handle_t handle, char *name)
{
	struct pf *p;
	struct pf_cmp cmp;
	
	cmp.handle = handle;
	cmp.name = name;

	p = (struct pf *) llist_rem(pfl, (void *) &cmp, pf_handle_cmp);
	if (p) return 0;
	else return -1;
}

/* pf_free(p)
 *
 * Frees data structures pointed to within a pf structure and then frees
 * the structure itself.
 */
static void pf_free(void *p)
{
	free(((struct pf *)p)->fp);
	free(p);
	return;
}

static void pfl_cleanup(pfl_t pfl)
{
	llist_free(pfl, pf_free);
	return;
}

/* PVFS NAME-RELATED FUNCTIONS BELOW */

static char *skip_to_filename(char *name)
{
	while (*(++name) != '/' && *name != '\0');

	return name;
}

static int name_to_port(char *name)
{
	char buf[10];

	while (*name != ':') name++;
	name++; /* point past the ':' */
	strncpy(buf, name, 9);
	buf[9] = '\0';
	name = buf;
	while (*name != '/' && *name != '\0') name++;
	*name = '\0';
	return atoi(buf);
}

static void hostcpy(char *host, char *name)
{
	int len;
	char *end;

	end = name;
	while (*end != ':') end++;

	/* we won't copy more than the space available, and we'll let the
	 * resolution code give us the error
	 */
	len = ((end - name) < PVFSHOSTLEN) ? (end - name) : PVFSHOSTLEN-1;
	strncpy(host, name, len);
	host[len] = '\0';
}

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */
