/*
 * sockio.h copyright (c) 1994 Clemson University, all rights reserved.
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
 * Contact:  Rob Ross  	rbross@parl.eng.clemson.edu
 *           Matt Cettei mcettei@parl.eng.clemson.edu
 * 
 */

#ifndef SOCKIO_H
#define SOCKIO_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pvfs_config.h>

int new_sock(void);
int bind_sock(int, int);
int connect_sock(int, char *, int);
int init_sock(struct sockaddr *, char *, int);
int brecv(int s, void *buf, int len);
int nbrecv(int s, void *buf, int len);
int bsend(int s, void *buf, int len);
int bsendv(int s, const struct iovec *vec, int cnt);
int nbsend(int s, void *buf, int len);
int get_sockopt(int s, int optname);
int set_tcpopt(int s, int optname, int val);
int set_sockopt(int s, int optname, int size);
int set_socktime(int s, int optname, int size);
int sockio_dump_sockaddr(struct sockaddr_in *ptr, FILE *fp);
#ifdef __USE_SENDFILE__
int nbsendfile(int s, int f, int off, int len);
#endif

#define GET_RECVBUFSIZE(s) get_sockopt(s, SO_RCVBUF)
#define GET_SENDBUFSIZE(s) get_sockopt(s, SO_SNDBUF)

/* some OS's (ie. Linux 1.3.xx) can't handle buffer sizes of certain
 * sizes, and will hang up
 */
#ifdef BRAINDEADSOCKS
/* setting socket buffer sizes can do bad things */
#define SET_RECVBUFSIZE(s, size)
#define SET_SENDBUFSIZE(s, size)
#else
#define SET_RECVBUFSIZE(s, size) set_sockopt(s, SO_RCVBUF, size)
#define SET_SENDBUFSIZE(s, size) set_sockopt(s, SO_SNDBUF, size)
#endif

#define GET_MINSENDSIZE(s) get_sockopt(s, SO_SNDLOWAT)
#define GET_MINRECVSIZE(s) get_sockopt(s, SO_RCVLOWAT)
#define SET_MINSENDSIZE(s, size) set_sockopt(s, SO_SNDLOWAT, size)
#define SET_MINRECVSIZE(s, size) set_sockopt(s, SO_RCVLOWAT, size)

/* BLOCKING / NONBLOCKING MACROS */

#define SET_NONBLOCK(x_fd) fcntl((x_fd), F_SETFL, O_NONBLOCK | \
   fcntl((x_fd), F_GETFL, 0))

#define SET_NONBLOCK_AND_SIGIO(x_fd) \
do { \
	fcntl((x_fd), F_SETOWN, getpid()); \
	fcntl((x_fd), F_SETFL, FASYNC | O_NONBLOCK | fcntl((x_fd), F_GETFL, 0)); \
} while (0)

#define CLR_NONBLOCK(x_fd) fcntl((x_fd), F_SETFL, fcntl((x_fd), F_GETFL, 0) & \
   (~O_NONBLOCK))


#endif
