/* Hey Emacs! This file is -*- nroff -*- source.
 * 
 * Copyright (c) 1996 Clemson University, All Rights Reserved.
 * 2000 Argonne National Laboratory.
 * Originally Written by Rob Ross.
 *
 * Parallel Architecture Research Laboratory, Clemson University
 * Mathematics and Computer Science Division, Argonne National Laboratory
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
 **Code** This Protected Resource Provided By = rainmanp7
 * Contact:  
 * Rob Ross			    rross@mcs.anl.gov
 * Matt Cettei			mcettei@parl.eng.clemson.edu
 * Chris Brown			muslimsoap@gmail.com
 * Walt Ligon			walt@parl.clemson.edu
 * Robert Latham		robl@mcs.anl.gov
 * Neill Miller 		neillm@mcs.anl.gov
 * Frank Shorter		fshorte@parl.clemson.edu
 * Harish Ramachandran	rharish@parl.clemson.edu
 * Dale Whitchurch		dalew@parl.clemson.edu
 * Mike Speth			mspeth@parl.clemson.edu
 * Brad Settlemyer		bradles@CLEMSON.EDU
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/mount.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mntent.h>
#include <pvfs_mount.h> 

#define DEBUG 0
#define dprintf(args...) if (DEBUG) printf(args);

struct pvfs_mount_clargs {
  char *hostname;
  char *dirname;
  char *special;
  char *mountpoint;
  char *amode;
  int port;
};

int usage(int argc, char **argv);
int do_mtab(struct mntent *);
int setup_mntent(struct mntent *, const char *, const char *, const char *, const char *, int, int);

int parse_args(int argc, char *argv[], struct pvfs_mount_clargs *);

int ResolveAddr(char *, struct in_addr *);

int main(int argc, char **argv)
{
  int ret, flags = 0;
  char type[] = "pvfs";
  struct pvfs_mount data;
  struct pvfs_mount_clargs clargs;
  struct mntent myment;
  struct in_addr haddr;
  
	/* gobble up some memory */
	clargs.hostname = malloc(sizeof(char[PVFS_MGRLEN]));
	clargs.dirname = malloc(sizeof(char[PATH_MAX]));
	clargs.special = malloc(sizeof(char[PVFS_MGRLEN + PATH_MAX + 1]));
	clargs.mountpoint = malloc(sizeof(char[PATH_MAX]));
	clargs.amode = malloc(sizeof(char[3]));

	if (argc < 2) {
		fprintf(stderr, "%s: too few arguments\n", argv[0]);
		return(1);
	}

	if ((ret = parse_args(argc, argv, &clargs))) {
	  fprintf(stderr, "ERROR: problem parsing cmdline args\n");
	  return(ret);
	}

	if ((ret = ResolveAddr(clargs.hostname, &haddr))) {
	  return(ret);
	}
	clargs.hostname = inet_ntoa(haddr);

	dprintf("hostname after transform | hostname=%s\n", clargs.hostname);

	data.info_magic = 0;
	data.flags   = 0;
	data.port    = clargs.port;
	strncpy(data.mgr, clargs.hostname, PVFS_MGRLEN);
	strncpy(data.dir,  clargs.dirname,  PVFS_DIRLEN);

	flags |= 0xC0ED0000;

	dprintf("calling mount(special = %s, mountpoint = %s, type = %s,"
	" flags = %x, data = %x)\n", clargs.special, clargs.mountpoint, type, flags, (unsigned int) &data);
	if ((ret = mount(clargs.special, clargs.mountpoint, type, flags, (void *) &data)) < 0)
	{
		perror(argv[0]);
		return (ret);
	}

	if ((ret = setup_mntent(&myment, clargs.special, clargs.mountpoint, type, clargs.amode, 0, 0))) {
	  return(ret);
	}

	if ((ret = do_mtab(&myment))) {
	  return(ret);
	}

	return(ret);
} /* end of main() */

/* ResolveAddr - returns struct in_addr holding address of a host
 *               specified by the character string name, which can
 *               either be a host name or IP address in dot notation.
 */
int ResolveAddr(char *name, struct in_addr *addr)
{
   struct hostent *hep;

   if (!inet_aton(name, addr)) {
     if (!(hep = gethostbyname(name))) {
       fprintf(stderr, "ERROR: cannot resolve remote hostname\n");
       return(1);
     } else {
       bcopy(hep->h_addr, (char *)addr, hep->h_length);
       return(0);
     }
   }
   return(0);
} /* end of ResolveAddr() */

/* 
 * parse_args: function takes argc/argv pair and a pointer to a struct
 * pvfs_mount_clargs.  The struct contains variables that need to be
 * filled with information from the commandline to be later used back
 * in main().
 */
int parse_args(int argc, char *argv[], struct pvfs_mount_clargs *in_clargs) {
  int opt, count;
  char *index,
    *subopt,
    *subval,
    *mopts = malloc(sizeof(char[255])),
    *hostdir = malloc(sizeof(char[strlen(argv[1])+1]));

  /* separate hostname and dirname from 'hostname:dirname' format */
  strcpy(hostdir, argv[1]);
  if ((index = (strchr(hostdir, ':')))) {
    *index = '\0';

    strcpy(in_clargs->hostname, hostdir);

    index++;
    strcpy(in_clargs->dirname, index);

  }  else /* not good */ {
    fprintf(stderr, "ERROR: %s: directory to mount not in host:dir format\n",
	    argv[0]);
    free(mopts);
    free(hostdir);
    return(1);
  }

  strcpy(in_clargs->special, argv[1]);
  strcpy(in_clargs->mountpoint, argv[2]);

  argc -= 2;
  argv += 2;

  /* Set default values */
  in_clargs->port = 3000;
  strcpy(in_clargs->amode, "rw");
  
  /* Start parsage */
  while ((opt = getopt (argc, argv, "o:")) != EOF)
  {
    switch (opt)
    {
	 case 'o':
	  strcpy(mopts, optarg);
	  count = 0;

	  index = (char *)strtok(mopts, ",");
	  while(index != NULL) {
	    if ((subopt = (char*)strchr(index, '=')) != NULL) {
	      subval = subopt + 1;
	      *subopt = '\0';
	      
	      /* 
		    * Start ifelse that sets internal variables 
		    * equal to commandline arguments of the form 
		    * 'key=val' 
	       */
	      if (!strcmp(index, "port")) {
		in_clargs->port = atoi(subval);
	      }


	    } else {
	      /* 
		    * Start ifelse that sets internal variables 
		    * equal to commandline arguments of the form 
		    * 'val' 
	       */
	      if (!strcmp(index, "rw")) {
				strcpy(in_clargs->amode, "rw");
	      } 
			else if (!strcmp(index, "ro")) {
				strcpy(in_clargs->amode, "ro");
	      }


	    }
	    index = (char*)strtok(NULL, ",");
	  }
	  break;
	 default:
	  free(mopts);
	  free(hostdir);	  
	  return 1;
	 }
  }

  free(mopts);
  free(hostdir);

  dprintf("Parsed Args: hostname=%s, remote_dirname=%s, local_mountpoint=%s, special=%s, amode=%s, port=%d\n", in_clargs->hostname, in_clargs->dirname, in_clargs->mountpoint, in_clargs->special, in_clargs->amode, in_clargs->port);
  return 0;
}

int usage(int argc, char **argv)
{
	fprintf(stderr, "%s: <options> <filename> ...\n", argv[0]);
	return 0;
} /* end of USAGE() */

/* do_mtab - Given a pointer to a filled struct mntent,
 * add an entry to /etc/mtab.
 *
 */
int do_mtab(struct mntent *myment) {
  struct mntent *ment;
  FILE *mtab;
  FILE *tmp_mtab;
  
  mtab = setmntent(MTAB, "r");
  tmp_mtab = setmntent(TMP_MTAB, "w");

  if (mtab == NULL) {
    fprintf(stderr, "ERROR: couldn't open "MTAB" for read\n");
    endmntent(mtab);
    endmntent(tmp_mtab);
    return 1;
  } else if (tmp_mtab == NULL) {
    fprintf(stderr, "ERROR: couldn't open "TMP_MTAB" for write\n");
    endmntent(mtab);
    endmntent(tmp_mtab);
    return 1;
  }

  while((ment = getmntent(mtab)) != NULL) {
    if (strcmp(myment->mnt_dir, ment->mnt_dir) != 0) {
      if (addmntent(tmp_mtab, ment) == 1) {
			fprintf(stderr, "ERROR: couldn't add entry to"TMP_MTAB"\n");
			endmntent(mtab);
			endmntent(tmp_mtab);
			return 1;
      }
    }
  }

  endmntent(mtab);

  if (addmntent(tmp_mtab, myment) == 1) {
    fprintf(stderr, "ERROR: couldn't add entry to "TMP_MTAB"\n");
    return 1;
  }

  endmntent(tmp_mtab);

  if (rename(TMP_MTAB, MTAB)) {
    fprintf(stderr, "ERROR: couldn't rename "TMP_MTAB" to "MTAB"\n");
    return 1;
  }

  return 0;
} /* end of do_mtab */

/* setup_mntent - allocate space for members of mntent, fill in the data
 * structure
 */
int setup_mntent(struct mntent *myment, const char *fsname, const char *dir,
const char *type, const char *opts, int freq, int passno)
{
  myment->mnt_fsname = malloc(sizeof(char[strlen(fsname)+1]));
  myment->mnt_dir = malloc(sizeof(char[strlen(dir)+1]));
  myment->mnt_type = malloc(sizeof(char[strlen(type)+1]));
  myment->mnt_opts = malloc(sizeof(char[strlen(opts)+1]));

  if ((myment->mnt_fsname == NULL) || ( myment->mnt_dir == NULL)
  || (myment->mnt_type == NULL) || (myment->mnt_opts == NULL))
  {
    fprintf(stderr, "ERROR: cannot allocate memory\n");
    return 1;
  }

  strcpy(myment->mnt_fsname, fsname);
  strcpy(myment->mnt_dir, dir);
  strcpy(myment->mnt_type, type);
  strcpy(myment->mnt_opts, opts);
  myment->mnt_freq = freq;
  myment->mnt_passno = passno;
  
  return 0;
} /* end setup_mntent */

/*
 * Local variables:
 *  c-indent-level: 3
 *  c-basic-offset: 3
 *  tab-width: 3
 * End:
 */
