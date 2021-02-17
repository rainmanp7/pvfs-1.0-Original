/*
 * sockio.c copyright (c) 1994 Clemson University, all rights reserved.
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
 * 
 * Contact:  Rob Ross  	rbross@parl.clemson.edu
 *           Walt Ligon walt@parl.clemson.edu
 * 
 */

static char sockio_c[] = "$Header: /projects/cvsroot/pvfs/shared/sockio.c,v 1.8 2000/10/30 14:51:58 rbross Exp $";

/* UNIX INCLUDE FILES */
#include <errno.h>
#include <stdio.h>
#include <string.h>	/* bzero and bcopy prototypes */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <debug.h>

#include <sys/uio.h>

/* PVFS INCLUDE FILES */
#include <sockio.h>
#include <pvfs_config.h>

extern int errno;

/* FUNCTIONS */
int new_sock() {
	static int p_num = -1; /* set to tcp protocol # on first call */
	struct protoent *pep;

   if (p_num == -1) {
		if ((pep = getprotobyname("tcp")) == NULL) {
			perror("Kernel does not support tcp");
			return(-1);
		}
		p_num = pep->p_proto;
	}
	return(socket(AF_INET, SOCK_STREAM, p_num));
}

int bind_sock(int sockd, int service)
{
	struct sockaddr_in saddr;

	bzero((char *)&saddr,sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons((u_short)service);
	saddr.sin_addr.s_addr = INADDR_ANY;
bind_sock_restart:
	if (bind(sockd,(struct sockaddr *)&saddr,sizeof(saddr)) < 0) {
		if (errno == EINTR) goto bind_sock_restart;
		return(-1);
	}
	return(sockd);
}

int connect_sock(int sockd, char *name, int service)
{
	struct sockaddr saddr;

	if (init_sock(&saddr, name, service) != 0) return(-1);
connect_sock_restart:
	if (connect(sockd,(struct sockaddr *)&saddr,sizeof(saddr)) < 0) {
		if (errno == EINTR) goto connect_sock_restart;
		return(-1);
	}
	return(sockd);
}

int init_sock(struct sockaddr *saddrp, char *name, int service)
{
	struct hostent *hep;

	bzero((char *)saddrp,sizeof(struct sockaddr_in));
	if (name == NULL) {
		if ((hep = gethostbyname("localhost")) == NULL) {
			return(-1);
		}
	}
	else if ((hep = gethostbyname(name)) == NULL) {
		return(-1);
	}
	((struct sockaddr_in *) saddrp)->sin_family = AF_INET;
	((struct sockaddr_in *)saddrp)->sin_port = htons((u_short)service);
	bcopy(hep->h_addr, (char *)&(((struct sockaddr_in *)saddrp)->sin_addr),
		hep->h_length);
	return(0);
}

/* blocking receive */
/* Returns -1 if it cannot get all len bytes
 * and the # of bytes received otherwise
 */
int brecv(int s, void *buf, int len)
{
	int oldfl, ret, comp = len;
	oldfl = fcntl(s, F_GETFL, 0);
	if (oldfl & O_NONBLOCK) fcntl(s, F_SETFL, oldfl & (~O_NONBLOCK));

	while (comp) {
brecv_restart:
		if ((ret = recv(s, (char *) buf, comp, 0)) < 0) {
			if (errno == EINTR) goto brecv_restart;
			return(-1);
		}
		if (!ret) {
			/* Note: this indicates a closed socket.  However, I don't
			 * like this behavior, so we're going to return -1 w/an EPIPE
			 * instead.
			 */
			errno = EPIPE;
			return(-1);
		}
		comp -= ret;
		buf += ret;
	}
	return(len - comp);
}

/* nonblocking receive */
int nbrecv(int s, void *buf, int len)
{
	int oldfl, ret, comp = len;

	oldfl = fcntl(s, F_GETFL, 0);
	if (!(oldfl & O_NONBLOCK)) fcntl(s, F_SETFL, oldfl | O_NONBLOCK);

	while (comp) {
nbrecv_restart:
		ret = recv(s, buf, comp, 0);
		if (!ret) /* socket closed */ {
			errno = EPIPE;
			return(-1);
		}
		if (ret == -1 && errno == EWOULDBLOCK) {
			LOG("nbrecv: would block\n");
			return(len - comp); /* return amount completed */
		}
		if (ret == -1 && errno == EINTR) {
			goto nbrecv_restart;
		}
		else if (ret == -1) {
			PERROR("nbrecv: recv");
			return(-1);
		}
		LOG3("nbrecv: s = %d, expect = %d, actual = %d\n", s, comp, ret);
		comp -= ret;
		buf += ret;
	}
	return(len - comp);
}

/* blocking send */
int bsend(int s, void *buf, int len)
{
	int oldfl, ret, comp = len;
	oldfl = fcntl(s, F_GETFL, 0);
	if (oldfl & O_NONBLOCK) fcntl(s, F_SETFL, oldfl & (~O_NONBLOCK));

	while (comp) {
bsend_restart:
		if ((ret = send(s, (char *) buf, comp, 0)) < 0) {
			if (errno == EINTR) goto bsend_restart;
			return(-1);
		}
		comp -= ret;
		buf += ret;
	}
	return(len - comp);
}

/* blocking vector send */
int bsendv(int s, const struct iovec *vec, int cnt)
{
	int tot, comp, ret, oldfl;
#ifdef BSENDV_NO_WRITEV
	struct iovec *cur = (struct iovec *)vec;
	char *buf;
#else
#endif

	oldfl = fcntl(s, F_GETFL, 0);
	if (oldfl & O_NONBLOCK) fcntl(s, F_SETFL, oldfl & (~O_NONBLOCK));

#ifdef BSENDV_NO_WRITEV
	for (tot=0; cnt--; cur++) {
		buf = (char *) cur->iov_base;
		comp = cur->iov_len;
		while (comp) {
bsendv_restart:
			if ((ret = send(s, buf, comp, 0)) < 0) {
				if (errno == EINTR) goto bsendv_restart;
				return(-1);
			}
			comp -= ret;
			buf += ret;
		}
		tot += cur->iov_len;
	}
	return(tot);
#else

	for(comp=0,tot=0; comp < cnt; comp++) tot += vec[comp].iov_len;

	if ((ret = writev(s, vec, cnt)) < 0) {
bsendv_restart:
		if (errno == EINTR) goto bsendv_restart;
		return(-1);
	}
	if (ret < tot) {
		PERROR("writev() wrote less than the full amount!!!\n");
	}
	return(ret);
#endif
}

/* nonblocking send */
/* should always return 0 when nothing gets done! */
int nbsend(int s, void *buf, int len)
{
	int oldfl, ret, comp = len;
	oldfl = fcntl(s, F_GETFL, 0);
	if (!(oldfl & O_NONBLOCK)) fcntl(s, F_SETFL, oldfl | O_NONBLOCK);

	while (comp) {
nbsend_restart:
		ret = send(s, (char *) buf, comp, 0);
		if (ret == 0 || (ret == -1 && errno == EWOULDBLOCK))
			return(len - comp); /* return amount completed */
		if (ret == -1 && errno == EINTR) {
			goto nbsend_restart;
		}
		else if (ret == -1) return(-1);
		comp -= ret;
		buf += ret;
	}
	return(len - comp);
}

#ifdef __USE_SENDFILE__
/* NBSENDFILE() - nonblocking (on the socket) send from file
 *
 * Here we are going to take advantage of the sendfile() call provided
 * in the linux 2.2 kernel to send from an open file directly (ie. w/out
 * explicitly reading into user space memory or memory mapping).
 *
 * We are going to set the non-block flag on the socket, but leave the
 * file as is.
 *
 * Boy, that type on the offset for sockfile() sure is lame, isn't it?
 * That's going to cause us some headaches when we want to do 64-bit
 * I/O...
 *
 * Returns -1 on error, amount of data written to socket on success.
 */
int nbsendfile(int s, int f, int off, int len)
{
	int oldfl, ret, comp = len, myoff;

	oldfl = fcntl(s, F_GETFL, 0);
	if (!(oldfl & O_NONBLOCK)) fcntl(s, F_SETFL, oldfl | O_NONBLOCK);

	while (comp) {
nbsendfile_restart:
		myoff = off;
		ret = sendfile(s, f, &myoff, comp);
		if (ret == 0 || (ret == -1 && errno == EWOULDBLOCK))
			return(len - comp); /* return amount completed */
		if (ret == -1 && errno == EINTR) {
			goto nbsendfile_restart;
		}
		else if (ret == -1) return(-1);
		comp -= ret;
		off += ret;
	}
	return(len - comp);
}
#endif

/* routines to get and set socket options */
int get_sockopt(int s, int optname)
{
	int val, len = sizeof(val);
	if (getsockopt(s, SOL_SOCKET, optname, &val, &len) == -1)
		return(-1);
	else return(val);
}

int set_tcpopt(int s, int optname, int val)
{
	if (setsockopt(s, SOL_TCP, optname, &val, sizeof(val)) == -1)
		return(-1);
	else return(val);
}

int set_sockopt(int s, int optname, int val)
{
	if (setsockopt(s, SOL_SOCKET, optname, &val, sizeof(val)) == -1)
		return(-1);
	else return(val);
}

int get_socktime(int s, int optname)
{
	struct timeval val;
	int len = sizeof(val);
	if (getsockopt(s, SOL_SOCKET, optname, &val, &len) == -1)
		return(-1);
	else return(val.tv_sec * 1000000 + val.tv_usec);
}

int set_socktime(int s, int optname, int size)
{
	struct timeval val;
	val.tv_sec = size / 1000000;
	val.tv_usec = size % 1000000;
	if (setsockopt(s, SOL_SOCKET, optname, &val, sizeof(val)) == -1)
		return(-1);
	else return(size);
}

/* SOCKIO_DUMP_SOCKADDR() - dump info in a sockaddr structure
 *
 * Might or might not work for any given platform!
 */
int sockio_dump_sockaddr(struct sockaddr_in *ptr, FILE *fp)
{
	int i;
	unsigned long int tmp;
	struct hostent *hp;
	char abuf[]="xxx.xxx.xxx.xxx\0";

	fprintf(fp, "sin_family = %d\n", ptr->sin_family);
	fprintf(fp, "sin_port = %d (%d)\n", ptr->sin_port, ntohs(ptr->sin_port));
	/* print in_addr info */
	tmp = ptr->sin_addr.s_addr;
	sprintf(&abuf[0], "%d.%d.%d.%d",tmp & 0xff, (tmp >> 8) & 0xff,
		(tmp >> 16) & 0xff, (tmp >> 24) & 0xff);
	hp = gethostbyaddr((char *)&ptr->sin_addr.s_addr, sizeof(ptr->sin_addr),
		ptr->sin_family);
	fprintf(fp, "sin_addr = %lx (%s = %s)\n", ptr->sin_addr, abuf, 
		hp->h_name);
	for (i = 0; i < sizeof(struct sockaddr) - sizeof(short int) -
		sizeof(unsigned short int) - sizeof(struct in_addr); i++)
	{
		fprintf(fp, "%x", ptr->sin_zero[i]);
	}
	fprintf(fp, "\n");
} /* end of SOCKIO_DUMP_SOCKADDR() */
