/*
 * alist.h copyright (c) 1997 Clemson University, all rights reserved.
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
 *           Matt Cettei mcettei@parl.eng.clemson.edu
 */

#ifndef ALIST_H
#define ALIST_H

#include <llist.h>
#include <flist.h>

/* access defines */
#define A_NOP 0
#define A_ACK 1
#define A_READ 2
#define A_WRITE 3
#define A_LIST 4
#define A_SOCKOPT 5

#define a_search(__al_p, __sock) a_sock_search((__al_p), (__sock))

typedef llist_p alist_p;

typedef struct ainfo ainfo, *ainfo_p;

struct ainfo {
	int8_t type;
	int sock;
	union {
		struct {
			int64_t off;
			int64_t size;
			finfo_p file_p; /* pointer to open file info */
		} rw;
		struct {
			void *ack_p;
			void *cur_p;
			int size;
		} ack;
		struct {
			alist_p al_p;
		} list;
		struct {
			int level;
			int name;
			int val;
		} sockopt;
	} u;
};

alist_p alist_new(void);
int alist_empty(alist_p);
int a_add_start(alist_p, ainfo_p);
int a_add_end(alist_p, ainfo_p);
ainfo_p a_sock_search(alist_p, int sock);
ainfo_p a_type_search(alist_p, int type);
ainfo_p a_get_start(alist_p al_p);
int a_sock_rem(alist_p, int sock);
int a_ptr_rem(alist_p, ainfo_p);
int a_ptr_cmp(void *key, void *a_p);
void alist_cleanup(alist_p);
int a_dump(void *);
int alist_dump(alist_p al_p);
int ainfo_new(ainfo_p);

#endif
