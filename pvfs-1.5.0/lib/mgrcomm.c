/* mgrcomm.c copyright (c) 1998 Clemson University, all rights reserved.

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

 */

static char mgrcomm_c[] = "$Header: /projects/cvsroot/pvfs/lib/mgrcomm.c,v 1.9 2001/01/15 21:25:42 rbross Exp $";

#include <sys/uio.h>

#include <lib.h>
#include <sockio.h>

extern fdesc_p pfds[];
extern int errno;

/* MGRINFO structure - this is used to keep up with managers in use
 *
 * DESCRIPTION:
 *
 * The idea here is to try to keep a single connection open to each
 * manager that we are communicating with.  To do this, we're going to
 * keep a little table and do our matching by comparing the port and 
 * address that we would use to connect to the manager.  We're going
 * to hold a maximum of MAXMGRS connections around.  There is no reason
 * why this number can't be bigger.
 *
 * NOTES ON FORKING PROCESSES:
 *
 * Experience now shows me that while this does indeed speed up metadata
 * operations quite a bit, it also causes some problems with programs
 * that fork children to do PVFS I/O (when they have already talked to
 * the manager and established a connection).
 *
 * Keep in mind that this situation causes any number of other potential
 * problems as well, particularly when a PVFS file is opened before the
 * fork occurs.  This kind if situation could possibly be handled via
 * the use of semaphores around all I/O operations, but that isn't
 * implemented and isn't going to be in this version of PVFS.  The next
 * version should handle that sort of thing in addition to being thread
 * safe.
 * 
 * SOLUTION TO FORKING PROCESSES:
 *
 * For now I'm going to add a "pid" field to the manager structure.
 * This will keep up with the pid of the owner of the connection.  If
 * the pid in the structure doesn't match the pid of the process
 * attempting to use the connection, we will close the connection,
 * initialize our structure, and open a new connection.
 *
 * This should allow programs such as Bonnie, which do not pass on PVFS
 * FDs but do talk to the manager before spawning processes, to operate
 * correctly.  It will not help situations where PVFS FDs are passed on.
 * 
 */
#define MAXMGRS 5
static int mgrcnt = 0;
static pid_t mgrpid = 0;
static struct mgrinfoitem {
	unsigned short int port;
	unsigned int addr;
	int fd;
} mgrinfo[MAXMGRS];

/* PROTOTYPES */
static int addmgrinfo(struct sockaddr *saddr_p, int fd);
static int getmgrfd(struct sockaddr *saddr_p);
static int badmgrfd(int fd);
static int initmgrconn(pid_t pid);

/* SEND_MREQ() - send a request to the manager and receive an ack
 *
 * This function handles sending a request to the manager, including
 * opening a connection to the manager if one is not already
 * established.  If an error occurs during the process, the socket will
 * be closed.  If the acknowledgement from the manager indicates
 * success, an open socket will be returned to the calling process so
 * that the process can receive trailing data (if any) or keep the
 * socket open.
 *
 * In the event that communication fails on a previously opened socket,
 * the code will attempt to establish a new connection.  It will only retry
 * in this manner once.
 *
 * Note: CURRENTLY NOT USED IN PVFS_OPEN()
 *
 * PARAMETERS:
 * dir_p  - directory metadata
 * req_p  - pointer to request structure
 * data_p - pointer to data to send following request (may be NULL)
 * ack_p  - pointer to memory location in which to store ack from mgr
 */
int send_mreq_dmeta(dmeta_p dir_p, mreq_p req_p, void *data_p,
                    mack_p ack_p)
{
	struct sockaddr saddr;

	if (!req_p || !ack_p || !dir_p) {
		errno = EINVAL;
		return(-1);
	}

	/* build a sockaddr structure */
	if (init_sock(&saddr, dir_p->host, dir_p->port)) return(-1);

	/* call send_mreq_saddr() to do the work */
	return(send_mreq_saddr(&saddr, req_p, data_p, ack_p));
} /* end of send_mreq_dmeta() */


/* SEND_MREQ_SADDR() - same as SEND_MREQ_DMETA, only pulls connection
 * info from a sockaddr structure instead.
 */
int send_mreq_saddr(struct sockaddr *saddr_p, mreq_p req_p,
                    void *data_p, mack_p ack_p)
{
	int fd;
	char newconn=0;
	struct iovec vec[2];

	if (!req_p || !ack_p || !saddr_p) {
		errno = EINVAL;
		return(-1);
	}
make_new_conn:
	if ((fd = getmgrfd(saddr_p)) < 0) {
		if ((fd = new_sock()) < 0) return(-1);
mgrcomm_connect_retry:
		if (connect(fd, saddr_p, sizeof(struct sockaddr)) < 0) {
			if (errno == EINTR) goto mgrcomm_connect_retry;
			if (errno == EAGAIN) {
				sleep(2);
				goto mgrcomm_connect_retry;
			}
			close(fd);
			return(-1);
		}
		newconn=1;
	}
	/* send request */
	req_p->majik_nr = MGR_MAJIK_NR;
	if (req_p->dsize > 0 && data_p) {
		vec[0].iov_base = req_p;
		vec[0].iov_len = sizeof(mreq);
		vec[1].iov_base = data_p;
		vec[1].iov_len = req_p->dsize;
		if (bsendv(fd, vec, 2) < (int)(sizeof(mreq) + req_p->dsize)) {
			PERROR("send_mreq_saddr (sending req with bsendv)");
			close(fd);
			if (!newconn) {
				badmgrfd(fd);
				goto make_new_conn; /* try to reopen connection */
			}
			return(-1);
		}
	}
	else /* no data -- stick to the bsend */ {
		if (bsend(fd, req_p, sizeof(mreq)) < (int) sizeof(mreq)) {
			PERROR("send_mreq_saddr (sending req)");
			close(fd);
			if (!newconn) {
				badmgrfd(fd);
				goto make_new_conn; /* try to reopen connection */
			}
			return(-1);
		}
	}

	/* receive ack */
	if (brecv(fd, ack_p, sizeof(mack)) < (int) sizeof(mack)) {
		close(fd);
		if (!newconn) {
		    badmgrfd(fd);
		    goto make_new_conn; /* try to reopen connection */
		}
		return(-1);
	}
	if (ack_p->majik_nr != MGR_MAJIK_NR) /* serious error -- nonsense ack */ {
		close(fd);
		if (!newconn) badmgrfd(fd);
		errno = EIO;
		return(-1);
	}

	/* not going to close connection, must save the state! */
	if (newconn) addmgrinfo(saddr_p, fd);

	if (ack_p->status) /* status error -- don't close the connection...  */ {
		errno = ack_p->eno;
		return(-1);
	}
	return(fd);
} /* end of send_mreq_saddr() */

static int addmgrinfo(struct sockaddr *saddr_p, int fd)
{
	if (mgrcnt >= MAXMGRS) {
		ERR("addmgrinfo: MAXMGRS exceeded!\n");
		return(-1);
	}
	mgrinfo[mgrcnt].addr = ((struct sockaddr_in *) saddr_p)->sin_addr.s_addr;
	mgrinfo[mgrcnt].port = ((struct sockaddr_in *) saddr_p)->sin_port;
	mgrinfo[mgrcnt].fd = fd;
	mgrcnt++;

	/* update fd table */
	if (pfds[fd]) {
		ERR1("fd %d already in use?!?\n", fd);
	}
	pfds[fd] = (fdesc_p)malloc(sizeof(fdesc));
	pfds[fd]->fs = FS_RESV;
	pfds[fd]->part_p = NULL;
	pfds[fd]->dim_p  = NULL;
	pfds[fd]->fd.ref = -1;
	pfds[fd]->fd.grp_nr = -1;
	return(0);
}

static int getmgrfd(struct sockaddr *saddr_p)
{
	int i=0;
	pid_t mypid;

	if ((mypid = getpid()) != mgrpid) {
		/* process trying to use another's connection or first call */
		initmgrconn(mypid);
		return(-1);
	}
	for (; i < mgrcnt; i++) {
		if (((struct sockaddr_in *)saddr_p)->sin_addr.s_addr == mgrinfo[i].addr
		&& ((struct sockaddr_in *)saddr_p)->sin_port == mgrinfo[i].port)
		{
			return(mgrinfo[i].fd);
		}
	}
	return(-1);
}

static int initmgrconn(pid_t pid)
{
	int i=0;

	for (; i < mgrcnt; i++) close(mgrinfo[i].fd);

	mgrcnt = 0;
	mgrpid = pid;
	return(0);
}

static int badmgrfd(int fd)
{
	int i=0, found=-1;

	for (; i < mgrcnt; i++) {
		if (fd == mgrinfo[i].fd) found=i;
	}
	if (found < 0) return(-1);
	if (found != mgrcnt-1) {
		mgrinfo[found] = mgrinfo[mgrcnt-1];
	}
	mgrcnt--;

	/* update fd table */
	if (!pfds[fd] || pfds[fd]->fs != FS_RESV) {
		ERR1("fd %d was not reserved?!?\n", fd);
		return(-1);
	}
	free(pfds[fd]);
	pfds[fd] = NULL;
	return(0);
}








