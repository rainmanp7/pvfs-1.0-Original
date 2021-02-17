/*
 * iodtab.h  -  Copyright (C) 1997 Clemson University
 * 
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
 * 
 */				 
	
#ifndef IODTAB_H
#define IODTAB_H

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pvfs_config.h>

/* DEFINES */

#define TABFNAME "/.iodtab"

/* STRUCTURES AND TYPEDEFS */

typedef struct iodtabinfo iodtabinfo, *iodtabinfo_p;

struct iodtabinfo
{
	int nodecount;
	struct sockaddr_in iod[MAXIODS];
};

/* PROTOTYPES */

iodtabinfo_p parse_iodtab();

#endif
