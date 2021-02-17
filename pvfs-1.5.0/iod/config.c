/*
 * iod_config.c copyright (c) 1998 Clemson University, all rights reserved.
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
 * 3) All other lines are of the format <option> <value>, where value
 *    is an arbitrary string ended by a carriage return or '#'
 * 4) In most cases only the first whitespace separated item in the
 *    value field is used for setting the option (eg. port).
 * 
 * SAMPLE CONFIG FILE:
 * 
 * #
 * # IOD configuration file
 * #
 * 
 * port 6969
 * user pvfs
 * rootdir /
 * datadir /var/pvfs_data
 * logdir /tmp
 * debug 0
 * 
 * END OF SAMPLE CONFIG FILE
 *
 * Separate functions are used for reading the configuration file and
 * setting up the resulting environment.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <pvfs_config.h>
#include <ctype.h>

#define INBUFSZ 1024
#define MAXOPTLEN 1024

static struct iod_config {
	unsigned short port;
	char user[MAXOPTLEN];
	char group[MAXOPTLEN];
	char rootdir[MAXOPTLEN];
	char datadir[MAXOPTLEN];
	char logdir[MAXOPTLEN];
	int debug;
} config = { IOD_REQ_PORT, IOD_USER, IOD_GROUP, IOD_ROOTDIR,
	IOD_DATADIR, IOD_LOGDIR, 0 };


int parse_config(char *fname)
{
	FILE *cfile;
	char inbuf[INBUFSZ], *option, *value;

	/* open file */
	if (!(cfile = fopen(fname, "r"))) {
		return(-1);
	}

	while (fgets(inbuf, INBUFSZ, cfile)) {
		/* comments get skipped here */
		if (*inbuf == '#') continue;

		/* blank lines get skipped here */
		if (!(option = strtok(inbuf, " \t\n"))) continue;

		if (!(value = strtok(NULL, "#\n"))) {
			fprintf(stderr, "parse_config: error tokenizing value\n");
			return(-1);
		}
		/* PORT (eg. "port 7000") */
		if (!strcasecmp("port", option)) {
			char *err;
			if (!(strtok(value, " \t\n#"))) {
				fprintf(stderr, "parse_config: error reducing string");
				continue;
			}
			while (isspace(*value)) value++;
			config.port = strtol(value, &err, 10);
			if (*err) /* bad character in value */ {
				fprintf(stderr, "warning: trailing character(s) in port\n");
			}
		}
		/* USER (eg. "user nobody") */
		else if (!strcasecmp("user", option)) {
			if (!(strtok(value, " \t\n#"))) {
				fprintf(stderr, "parse_config: error reducing string");
				continue;
			}
			while (isspace(*value)) value++;
			strncpy(config.user, value, MAXOPTLEN);
		}
		/* GROUP (eg. "group bin") */
		else if (!strcasecmp("group", option)) {
			if (!(strtok(value, " \t\n#"))) {
				fprintf(stderr, "parse_config: error reducing string");
				continue;
			}
			while (isspace(*value)) value++;
			strncpy(config.group, value, MAXOPTLEN);
		}
		/* ROOTDIR (eg. "rootdir /") */
		else if (!strcasecmp("rootdir", option)) {
			if (!(strtok(value, " \t\n#"))) {
				fprintf(stderr, "parse_config: error reducing string");
			}
			while (isspace(*value)) value++;
			strncpy(config.rootdir, value, MAXOPTLEN);
		}
		/* DATADIR (eg. "datadir /tmp") */
		else if (!strcasecmp("datadir", option)) {
			if (!(strtok(value, " \t\n#"))) {
				fprintf(stderr, "parse_config: error reducing string");
			}
			while (isspace(*value)) value++;
			strncpy(config.datadir, value, MAXOPTLEN);
		}
		/* LOGDIR (eg. "logdir /tmp") */
		else if (!strcasecmp("logdir", option)) {
			if (!(strtok(value, " \t\n#"))) {
				fprintf(stderr, "parse_config: error reducing string");
			}
			while (isspace(*value)) value++;
			strncpy(config.logdir, value, MAXOPTLEN);
		}
		/* DEBUG (eg. "debug 4") */
		else if (!strcasecmp("debug", option)) {
			char *err;
			if (!(strtok(value, " \t\n#"))) {
				fprintf(stderr, "parse_config: error reducing string");
			}
			while (isspace(*value)) value++;
			config.debug = strtol(value, &err, 10);
			if (*err) /* bad character in value */ {
				fprintf(stderr, "warning: trailing character(s) in debug\n");
			}
		}
		else {
			fprintf(stderr, "unknown option: %s\n", option);
		}
	}
	fclose(cfile);
	return(0);
} /* end of parse_config() */

int dump_config(FILE *fp)
{
	fprintf(fp, "# I/O DAEMON CONFIG FILE -- AUTOMATICALLY GENERATED\n");
	fprintf(fp, "port %d\n", config.port);
	fprintf(fp, "user %s\n", config.user);
	fprintf(fp, "group %s\n", config.group);
	fprintf(fp, "rootdir %s\n", config.rootdir);
	fprintf(fp, "datadir %s\n", config.datadir);
	fprintf(fp, "logdir %s\n", config.logdir);
	fprintf(fp, "debug %d\n", config.debug);
	return(0);
} /* end of dump_config() */

/* SET_CONFIG() - set environment using configuration parameters
 *
 * First we determine if we are running as root.
 *
 * Steps (if root):
 * 1) find gid, uid
 * 2) chroot() to rootdir
 * 3) Set current working directory to datadir
 * 4) set gid, uid
 *
 * Steps (if not root):
 * 1) Set current working directory to datadir
 *
 * Returns 0 on success, -1 on failure.
 */
int set_config(void)
{
	int uid=-1, gid=-1; 
	struct group *grp_p;
	struct passwd *pwd_p;
	char im_root;

	im_root = (!geteuid() || !getuid()) ? 1 : 0;

	if (im_root) {
		if (!(grp_p = getgrnam(config.group))) {
			fprintf(stderr, "set_config: getgrnam error\n");
			fprintf(stderr, "make sure the group id specified in iod.conf exists in /etc/group.\n");
			return(-1);
		}
		gid = grp_p->gr_gid;

		if (!(pwd_p = getpwnam(config.user))) {
			fprintf(stderr, "setconfig: getpwnam error\n");
			fprintf(stderr, "make sure the user id specified in iod.conf exists in /etc/passwd.\n");
			perror("set_config: getpwnam");
			return(-1);
		}
		uid = pwd_p->pw_uid;

		if (chroot(config.rootdir) < 0) {
			perror("set_config: chroot");
			return(-1);
		}
	}

	if (chdir(config.datadir) < 0) {
		perror("set_config: chdir");
		return(-1);
	}


	if (im_root) {
		if (setgid(gid) < 0) {
			perror("set_config: setgid");
			return(-1);
		}

		if (setuid(uid) < 0) {
			perror("set_config: setuid");
			return(-1);
		}
	}
	return(0);
} /* end of set_config() */

int get_config_port(void)
{
	return(config.port);
}

int get_config_debug(void)
{
	return(config.debug);
}

char *get_config_logdir(void)
{
	return(config.logdir);
}
