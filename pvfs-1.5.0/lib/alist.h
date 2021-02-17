/*
 * alist.h copyright (c) 1996 Rob Ross, all rights reserved.
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
 */

/*
 * ALIST.H - access list header file FOR LIBRARY CODE
 *
 */

#ifndef ALIST_H
#define ALIST_H

#include <llist.h>

typedef llist_p alist_p;

typedef struct ainfo ainfo, *ainfo_p;

#define A_READ  0
#define A_WRITE 1
#define A_REQ   2
#define A_ACK   3

struct ainfo {
	int8_t type;
	int sock;
	union {
		struct {
			char *loc;
			int64_t size;
		} rw;
		struct {
			char *req_p;
			char *cur_p;
			int64_t size;
		} req;
		struct {
			int iod_nr;
			char *ack_p;
			char *cur_p;
			int64_t size;
		} ack;
		struct {
			alist_p al_p;
		} list;
	} u;
};

alist_p alist_new(void);
int a_add_start(alist_p, ainfo_p);
int a_add_end(alist_p, ainfo_p);
ainfo_p a_get_start(alist_p);
ainfo_p a_search(alist_p, int sock);
int a_sock_rem(alist_p, int sock);
int a_ptr_rem(alist_p, ainfo_p);
void a_cleanup(alist_p);
int a_dump(void *);
int alist_dump(alist_p al_p);
ainfo_p ainfo_new(void);

#endif
