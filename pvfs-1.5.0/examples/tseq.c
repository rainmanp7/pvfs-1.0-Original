/*
/* foo.c copyright (c) 1996 Rob Ross, all rights reserved.
/*/

/*
/* FOO.C - 
/*
/*/

/*
 * $Log: tseq.c,v $
 * Revision 1.3  2000/11/22 16:06:05  rbross
 * Trudged through all the code and took out everything that was #if 0'd
 * and didn't need to be around any more.
 *
 * Revision 1.2  2000/10/30 14:50:59  rbross
 * Took out all the glibc files, since we're not using them any more.
 * Removed instances of syscall() all over the place.
 *
 * Revision 1.1.1.1  1999/08/10 17:11:31  rbross
 * Original 1.3.3 sources
 *
 * Revision 1.1  1997/11/25 18:17:08  rbross
 * Initial revision
 *
/*/

static char header[] = "$Header: /projects/cvsroot/pvfs/examples/tseq.c,v 1.3 2000/11/22 16:06:05 rbross Exp $";

#include <stdio.h>
#include <fcntl.h>
#include <pvfs.h>
#include <malloc.h>

main (int argc, char *argv[])
{
	int s, opt, opt2, i, fd, size, offset, seekmode, readsize, writesize;
	char buf[256];
	char *writebuf;
	char *readbuf;
	struct stat statbuf;
	struct pvfs_filestat pstat;
	
for (;;) {
	printf("req type: \n\tchmod  (0)\n\tchown  (1)\n\tclose  (2)\n\tstat   (3)"
		"\n\tmount  (4)\n\topen   (5)\n\tunlink (6)\n\twrite  (7)\n\tread   (8)"
		"\n\tlseek  (9)\n\tcreat  (10)\n\tQUIT   (11)\n");
	scanf("%d", &opt);
	getchar();
		
	if (opt == 11) exit(0);
	if ((opt != 2)&&(opt != 7)&&(opt != 8)&&(opt != 9)) /* grab a filename */ {
		printf("\n\nfilename: ");
		gets(buf);
	} else {
		printf("\n\nFD: ");
		scanf("%d",&fd);
		getchar();
	}	

	switch(opt)  {
		case 0:						/* chmod */
			if (pvfs_chmod(buf,  0400) < 0) {
				perror("chmod");
			}
			break;
		case 1:						/* chown */
			if (pvfs_chown(buf,  0, 5000) < 0) {
				perror("chown");
			}
			break;
		case 2:						/* close */
			printf("fd is: %d\n",fd);
			if (pvfs_close(fd) < 0) {
				perror("close");
			}	
			printf("File closed\n");
			break;
		case 3:						/* stat */
			if ((i = pvfs_ostat(buf, &statbuf)) < 0) {
				perror("stat");
			}
			printf("MODE: %o\n",statbuf.st_mode);
			printf("Size: %d\n",statbuf.st_size);
			break;
		case 4:						/* mount */
			if (pvfs_mount(0, buf,  0, 0, 0) < 0) {
				perror("mount");
			}
			break;
		case 5:						/* open */
			printf("Flag: %d\n", O_RDWR|O_CREAT);
			if ((i = pvfs_open(buf, O_RDWR | O_CREAT, 0666)) < 0) {
				perror("open");
			}
			printf("FD is: %d\n", i);	
			break;
		case 6:						/* unlink */
			if ((i = pvfs_unlink(buf)) < 0) {
				perror("unlink");
			}
			printf("File unlinked\n");
			break;
		case 7:						/* write */
			printf("\nSize to write: ");
			scanf("%d",&writesize);
			getchar();
			writebuf = (char *)malloc(writesize);
			printf("fd is: %d\n",fd);
			if ((i = pvfs_write(fd, writebuf, writesize)) < 0) {
				perror("write");
			}
			printf("write ret = %d\n", i);
			free(writebuf);
			break;
		case 8:						/* read */
			printf("\nSize to read: ");
			scanf("%d",&readsize);
			getchar();
			readbuf = (char *)malloc(readsize * sizeof(char));
			if ((i = pvfs_read(fd, readbuf, readsize)) < 0) {
				perror("read");
			}
			printf("read ret = %d\n", i);
			free(readbuf);
			break;
		case 9:						/* lseek */
			printf("\nOffset: ");
			scanf("%d",&offset);
			getchar();
			printf("\n\tSEEK_CUR  (0)\n\tSEEK_SET  (1)\n\tSEEK_END"
				"  (2)\n");	
			scanf("%d",&opt2);
			getchar();
			switch(opt2){
				case 1:
					seekmode = SEEK_SET;
					break;
				case 2:
					seekmode = SEEK_END;
					break;
				default:
					seekmode = SEEK_CUR;
					break;
			}		
			if ((i = pvfs_lseek(fd, offset, seekmode)) < 0) {
				perror("lseek");
			}
			printf("lseek ret = %d\n", i);
			break;
		case 10:						/* creat */
			printf("\nBase: ");
			scanf("%d",&(pstat.base));
			getchar();
			printf("\nPcount: ");
			scanf("%d",&(pstat.pcount));
			getchar();
			printf("\nStripe size: ");
			scanf("%d",&(pstat.ssize));
			getchar();
			pstat.bsize = 0;
			pstat.soff  = 0;
			if ((i = pvfs_creat(buf, 0666, &pstat)) < 0) {
				perror("creat");
			}	
			printf("FD is: %d\n", i);	
			break;
	}
  }
}
