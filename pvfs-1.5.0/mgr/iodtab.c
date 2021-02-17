/*
 * iodtab.c copyright (c) 1998 Clemson University, all rights reserved.
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
 * Contact:  Rob Ross    rbross@parl.eng.clemson.edu
 */

/*
 * These functions parse the config file for the I/O daemon and set
 * up the daemon's environment based on the specified configuration.
 * 
 * File format is simple:
 * 
 * 1) Blank lines are ignored
 * 2) Lines starting with '#' are ignored
 *
 * Separate functions are used for reading the configuration file and
 * setting up the resulting environment.
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iodtab.h>
#include <arpa/inet.h>
#include <pvfs_config.h>

#define INBUFSZ 1024

static struct iodtabinfo iods;

struct iodtabinfo *parse_iodtab(char *fname)
{
	FILE *cfile;
	char inbuf[INBUFSZ], *entry, *port;
	struct hostent *hep;

	/* open file */
	if (!(cfile = fopen(fname, "r"))) {
		perror("fopen");
		return(NULL);
	}

	iods.nodecount = 0;

	while (fgets(inbuf, INBUFSZ, cfile)) {
		if (iods.nodecount >= MAXIODS) {
			fprintf(stderr, "MAXIODS exceeded!\n");
			return(NULL);
		}
		/* standard comments get skipped here */
		if (*inbuf == '#') continue;

		/* blank lines get skipped here */
		if (!(entry = strtok(inbuf, "#\n"))) continue;

		for (port = entry; *port && *port != ':'; port++);
		if (*port == ':') /* port number present */ {
			char *err;
			int portnr;

			portnr = strtol(port+1, &err, 10);
			if (err == port+1) /* ack, bad port */ {
				fprintf(stderr, "parse_iodtab: bad port\n");
				return(NULL);
			}
			iods.iod[iods.nodecount].sin_port = htons(portnr);
		}
		else /* use default port number */ {
			iods.iod[iods.nodecount].sin_port = htons(IOD_REQ_PORT);
		}
		*port = 0;
		if (!inet_aton(entry, &iods.iod[iods.nodecount].sin_addr)) {
			if (!(hep=gethostbyname(entry)))
				bzero((char *)&iods.iod[iods.nodecount].sin_addr,
					sizeof(struct in_addr));
			else
				bcopy(hep->h_addr,(char *)&iods.iod[iods.nodecount].sin_addr,
					hep->h_length);
		}
		iods.iod[iods.nodecount].sin_family = AF_INET;
		iods.nodecount++;
	}
	fclose(cfile);
	return(&iods);
} /* end of parse_config() */

int dump_iodtab(FILE *fp)
{
	int i;
	char *outp;

	fprintf(fp, "# IODTAB FILE -- AUTOMATICALLY GENERATED\n");
	for (i=0; i < iods.nodecount; i++) {
		outp = inet_ntoa(iods.iod[i].sin_addr);
		fprintf(fp, "%s:%d\n", outp, ntohs(iods.iod[i].sin_port));
	}
	return(0);
} /* end of dump_config() */

