/* mkdot.c copyright (c) 1997 Clemson Univeristy - all rights reserved */

/* Command line function to create dot files for directories in
 * PVFS.  Options include:
 *              -r  directory where dot file is needed
 *                  (must end in /)
 *              -u  user name
 *              -m  mode for new dir. (looks like 0ugo
 *							for user, group, other)
 * 				 -d  root directory filename
 * Will append root directory name to end of dotfile
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <netdb.h>

#include <pvfs.h>
#include <pvfs_proto.h>

extern char *optarg;
extern int  optind;
int atoo(char *);

int main(int argc, char **argv)
{
	int newopt, intuid, intgid, intmode, fd, err = 0;
	char *uid, *gid, *mode, *host, *port, *rd_path, *sd_path;
	char fname[256];
	FILE *fp;
	struct passwd *pw;
	struct group *gr;
	struct stat sbuf;
	int uf=0, gf=0, mf=0, hf=0, pf=0, rf=0, sf=0;

	while ((newopt = getopt(argc, argv, "u:g:m:h:p:r:s:"))!= EOF)
	{
		switch(newopt)
		{
			case 'u':
				uid = optarg;
				uf++;
		   	break;
			case 'g':
				gid = optarg;
				gf++;
		   	break;
			case 'm':
				mode = optarg;
				mf++;
		   	break;
			case 'h':
				host = optarg;
				hf++;
		   	break;
			case 'p':
				port = optarg;
				pf++;
		   	break;
			case 'r':
				rd_path = optarg;
				rf++;
		   	break;
			case 's':
				sd_path = optarg;
				sf++;
		   	break;
		}
	}
	if (!(uf&&gf&&mf&&hf&&pf&&rf&&sf)) {
		fprintf(stderr,"Usage: %s -u uid -g gid -m mode -h host"
			" -p port -r root-path -s subdir-path\n",argv[0]);
		exit(-1);
	}
	
	strcpy(fname, rd_path);
	strcat(fname, "/");
	strcat(fname, sd_path);
	strcat(fname, "/.pvfsdir");
	umask(0);
	/* check arguments */
	if ((intuid = atoi(uid)) == 0) {
		intuid = -1;
	}
	if ((pw = getpwuid((uid_t)intuid)) == NULL &&
		(pw = getpwnam(uid)) == NULL) {
	        fprintf(stderr,"UID (%s) does not seem to be valid\n", uid);
                err++;
        }
	intuid = pw->pw_uid;
	if ((intgid = atoi(gid)) == 0) {
		intgid = -1;
	}
	if ((gr = getgrgid((gid_t)intgid)) == NULL &&
		(gr = getgrnam(gid)) == NULL) {
	        fprintf(stderr,"GID (%s) does not seem to be valid\n", gid);
                err++;
        }
	intgid = gr->gr_gid;
	intmode = strtol(mode,(char **)0,8);
	if ((intmode & 0140000) != 0040000) {
		if ((intmode & 0170000) == 0) {
			intmode |= 0040000;
		} else {
	        	fprintf(stderr,"MODE (%s) does not seem to be valid\n",
				 mode);
                	err++;
		}
        }
	if (gethostbyname(host) == NULL) {
	        fprintf(stderr,"HOST (%s) does not seem to be valid\n", host);
                err++;
        }
	if (atoi(port) < 1024) {
	        fprintf(stderr,"PORT (%s) does not seem to be valid\n", port);
                err++;
        }
	if ((fd = open(fname, O_RDWR|O_CREAT|O_TRUNC, 0644)) <= 0) {
	        fprintf(stderr,"PATH (%s) does not seem to be valid\n", fname);
		perror("Could not open dot file");
                err++;
	}
	if (err)
		exit(0);
	/* write file */
	fstat(fd, &sbuf);	
	if ((fp = fdopen(fd, "a")) <= 0) {
		perror("Error creating stream");
		exit(0);
	}
	fprintf(fp,"%ld\n%d\n%d\n%o\n%s\n%s\n%s\n%s\n\n",sbuf.st_ino,intuid,
			intgid,intmode,port,host,rd_path,sd_path);
	fprintf(stderr,"File created.\n");

	return 0;
}	
