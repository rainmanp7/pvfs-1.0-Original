/*
 * iod-ping.c copyright (c) 2000 Clemson University, all rights reserved.
 *
 * Written by Rob Ross.
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
 * Contact:  Rob Ross    rross@mcs.anl.gov
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sockio.h>
#include <pvfs.h>

int parse_args(int argc, char **argv);
int usage(int argc, char **argv);

extern char *optarg;
extern int optind;

static char host[256] = "localhost";
static int port_nr = IOD_REQ_PORT;

int main(int argc, char **argv) {
	int s, err;
	struct ireq req;
	struct iack ack;
	
	req.majik_nr = IOD_MAJIK_NR;
	req.type = IOD_NOOP;
	req.dsize = 0;

	s = new_sock();

	parse_args(argc, argv);
	if (optind < argc) {
		usage(argc, argv);
	}


	err = connect_sock(s, host, port_nr);
	if (err < 0) goto oops;

	err = bsend(s, &req, sizeof(req));
	if (err < 0) goto oops;

	err = brecv(s, &ack, sizeof(ack));
	if (err < 0) goto oops;

	printf("%s:%d is responding.\n", host, port_nr);
	exit(0);

oops:
	printf("%s:%d is down.\n", host, port_nr);
#if 0
	perror("iod-ping");
#endif
	exit(1);
}


int parse_args(int argc, char **argv)
{
	int c;

	while ((c = getopt(argc, argv, "h:p:")) != EOF) {
		switch(c) {
			case 'h':
				strncpy(host, optarg, 255);
				host[255] = 0;
				break;
			case 'p':
				port_nr = atoi(optarg);
				break;
			case '?':
				usage(argc, argv);
				break;
		}
	}
	return 1;
} /* end of PARSE_ARGS() */

int usage(int argc, char **argv)
{
	fprintf(stderr, "Usage: iod-ping [-h host] [-p port]\n");
	exit(1);
} /* end of USAGE() */

