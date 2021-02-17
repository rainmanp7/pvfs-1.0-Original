/* pvfs-test.c
 *
 * Usually the MPI versions can be compiled from this directory with
 * something like:
 *
 * mpicc -D__USE_MPI__ -I../include pvfs-test.c -L../lib -lminipvfs \
 *       -o pvfs-test
 *
 * This is derived from code given to me by Rajeev Thakur.  Dunno where
 * it originated.
 *
 * It's purpose is to produce aggregate bandwidth numbers for varying
 * block sizes, number of processors, an number of iterations.
 *
 * Compile time defines determine whether or not it will use mpi, while
 * all other options are selectable via command line.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <error.h>
#include <string.h>
#include <sys/time.h>
#include <pvfs.h>
#include <pvfs_proto.h>
#ifdef __USE_MPI__
#include <mpi.h>
#endif


/* DEFAULT VALUES FOR OPTIONS */
int64_t opt_block     = 1048576*16;
int     opt_iter      = 1;
int     opt_stripe    = -1;
int     opt_correct   = 0;
int     opt_vfs       = 0;
int     opt_read      = 1;
int     opt_write     = 1;
int     opt_files     = 1;
int     opt_unlink    = 0;
int     amode         = O_RDWR | O_CREAT;
char    opt_file[256] = "/foo/test.out\0";
char    opt_pvfstab[256] = "notset\0";
int     opt_pvfstab_set = 0;

/* function prototypes */
int parse_args(int argc, char **argv);
double Wtime(void);

extern fdesc_p pfds[];
extern int errno;
extern int debug_on;

/* globals needed for getopt */
extern char *optarg;
extern int optind, opterr;

int main(int argc, char **argv)
{
	char *buf, *tmp, *buf2, *tmp2, *check;
	int fd, i, j, mynod=0, nprocs=1, err, my_correct = 1, correct, myerrno;
	int mynod_mod = 0;
	double stim, etim;
	double write_tim = 0;
	double read_tim = 0;
	double read_bw, write_bw;
	double max_read_tim, max_write_tim;
	double min_read_tim, min_write_tim;
	double sum_read_tim, sum_write_tim;
	double ave_read_tim, ave_write_tim;
	double var_read_tim, var_write_tim;
	double sumsq_read_tim, sumsq_write_tim;
	double sq_read_tim, sq_write_tim;
	struct pvfs_filestat pstat = {0,-1,16384,0,0};
	int64_t iter_jump = 0;

	/* startup MPI and determine the rank of this process */
#ifdef __USE_MPI__
	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &mynod);
#endif

	/* parse the command line arguments */
	parse_args(argc, argv);
	pstat.ssize = opt_stripe;
	if (!opt_vfs) {
		/* tell pvfs that we want to pass in metadata and have tcp
		 * connections setup for us at pvfs_open
		 */
		amode = amode | O_META | O_CONN;
		if (mynod == 0)
			printf("# Using native pvfs calls.\n");
	}
	else {
		if (mynod == 0)
			printf("# Using vfs calls.\n");
	}

	if (opt_files > 1) {
		/* create multiple file names */
		char intbuf[10];

		if (opt_files > nprocs) opt_files = nprocs;
		snprintf(intbuf, 9, "%d",mynod % opt_files);
		strcat(opt_file, intbuf);
		mynod_mod = mynod / opt_files;

	}
	
	/* kindof a weird hack- if the location of the pvfstab file was 
	 * specified on the command line, then spit out this location into
	 * the appropriate environment variable:
	 */
	
	if	(opt_pvfstab_set) {
		if ((setenv("PVFSTAB_FILE", opt_pvfstab, 1)) < 0) {
			perror("setenv");
			goto die_jar_jar_die;
		}
	}
	
	/* this is how much of the file data is covered on each iteration of
	 * the test.  used to help determine the seek offset on each
	 * iteration
	 */
	iter_jump = nprocs * opt_block;
		
	/* setup a buffer of data to write */
	if (!(tmp = (char *) malloc(opt_block + 256))) {
		perror("malloc");
		goto die_jar_jar_die;
	}
	buf = tmp + 128 - (((long)tmp) % 128);  /* align buffer */

	if (opt_correct) {
		/* do the same buffer setup for verifiable data */
		if (!(tmp2 = (char *) malloc(opt_block + 256))) {
			perror("malloc2");
			goto die_jar_jar_die;
		}
		buf2 = tmp + 128 - (((long)tmp) % 128);
	}

	if (opt_write) {
		/* open the file for writing */
		if (opt_vfs) {
			if ((fd = open(opt_file, amode, 0644)) < 0) {
				fprintf(stderr, "node %d, open error: %s\n", mynod,
				strerror(errno));
				goto die_jar_jar_die;
			}
		}
		else {
			if ((fd = pvfs_open(opt_file, amode, 0644, &pstat, NULL)) < 0) {
				fprintf(stderr, "node %d, open error: %s\n", mynod,
				strerror(errno));
				goto die_jar_jar_die;
			}

		}

		/* repeat write operation number of times specified on the command line */
		for (j=0; j < opt_iter; j++) {

			/* seek to an appropriate position depending on the iteration and
			 * rank of the current process */
			if (opt_vfs)
				lseek(fd, (j*iter_jump)+(mynod_mod*opt_block), SEEK_SET);
			else
				pvfs_lseek64(fd,
				(j*iter_jump)+(mynod_mod*opt_block),
				SEEK_SET);

			if (opt_correct) /* fill in buffer for iteration */ {
				for (i=mynod+j, check=buf; i<opt_block; i++,check++) *check=(char)i;
			}

			/* discover the starting time of the operation */
#ifdef __USE_MPI__
			MPI_Barrier(MPI_COMM_WORLD);
			stim = MPI_Wtime();
#else
			stim = Wtime();
#endif

			/* write out the data */
			if (opt_vfs)
				err = write(fd, buf, opt_block);
			else
				err = pvfs_write(fd, buf, opt_block);

			myerrno = errno;

			/* discover the ending time of the operation */
#ifdef __USE_MPI__
			etim = MPI_Wtime();
#else
			etim = Wtime();
#endif
			write_tim += (etim - stim);

			if (err < 0)
				fprintf(stderr, "node %d, write error, loc = %Ld: %s\n",
				mynod, mynod_mod*opt_block, strerror(myerrno));
			
		} /* end of write loop */

		/* close the file */
		if (opt_vfs) {
			if (close(fd) < 0)
				fprintf(stderr, "node %d, close error after write\n", mynod);
		}
		else {
			if (pvfs_close(fd) < 0)
				fprintf(stderr, "node %d, close error after write\n", mynod);
		}
		 
		/* wait for everyone to synchronize at this point */
#ifdef __USE_MPI__
		MPI_Barrier(MPI_COMM_WORLD);
#endif
	} /* end of if (opt_write) */

	if (opt_read) {
		/* open the file to read the data back out */
		if (opt_vfs) {
			if ((fd = open(opt_file, amode, 0644)) < 0) {
				fprintf(stderr, "node %d, open error: %s\n", mynod,
				strerror(errno));
				goto die_jar_jar_die;
			}
		}
		else {
			if ((fd = pvfs_open(opt_file, amode, 0644, &pstat, NULL)) < 0) {
				fprintf(stderr, "node %d, open error: %s\n", mynod,
				strerror(errno));
				goto die_jar_jar_die;
			}
		}

		/* repeat the read operation the number of iterations specified */
		for (j=0; j < opt_iter; j++) {

			/* seek to the appropriate spot give the current iteration and
			 * rank within the MPI processes */
			if (opt_vfs)
				lseek(fd, (j*iter_jump)+(mynod_mod*opt_block), SEEK_SET);
			else
				pvfs_lseek64(fd, (j*iter_jump)+(mynod_mod*opt_block), SEEK_SET);

			/* discover the start time */
#ifdef __USE_MPI__
			MPI_Barrier(MPI_COMM_WORLD);
			stim = MPI_Wtime();
#else
			stim = Wtime();
#endif

			/* read in the file data; if testing for correctness, read into
			 * a second buffer so we can compare data
			 */
			if (opt_vfs) {
				if (!opt_correct)
					err = read(fd, buf, opt_block);
				else
					err = read(fd, buf2, opt_block);
			}
			else {
				if (!opt_correct)
					err = pvfs_read(fd, buf, opt_block);
				else
					err = pvfs_read(fd, buf2, opt_block);
			}
			myerrno = errno;

			/* discover the end time */
#ifdef __USE_MPI__
			etim = MPI_Wtime();
#else
			etim = Wtime();
#endif
			read_tim += (etim - stim);

			if (err < 0)
				fprintf(stderr, "node %d, read error, loc = %Ld: %s\n", mynod,
				mynod_mod*opt_block, strerror(myerrno));

			/* if the user wanted to check correctness, compare the write
			 * buffer to the read buffer
			 */
			if (opt_correct && memcmp(buf, buf2, opt_block)) {
				fprintf(stderr, "node %d, correctness test failed\n", mynod);
				my_correct = 0;
#ifdef __USE_MPI__
				MPI_Allreduce(&my_correct, &correct, 1, MPI_INT, MPI_MIN,
					MPI_COMM_WORLD);
#endif
			}
		} /* end of read loop */

		/* close the file */
		if (opt_vfs)
			close(fd);
		else
			pvfs_close(fd);
	} /* end of if (opt_read) */

#ifdef __USE_MPI__
	MPI_Allreduce(&read_tim, &max_read_tim, 1, MPI_DOUBLE, MPI_MAX,
		MPI_COMM_WORLD);
	MPI_Allreduce(&read_tim, &min_read_tim, 1, MPI_DOUBLE, MPI_MIN,
		MPI_COMM_WORLD);
	MPI_Allreduce(&read_tim, &sum_read_tim, 1, MPI_DOUBLE, MPI_SUM,
		MPI_COMM_WORLD);
	/* calculate our part of the summation used for variance */
	sq_read_tim = read_tim - (sum_read_tim / nprocs);
	sq_read_tim = sq_read_tim * sq_read_tim;
	MPI_Allreduce(&sq_read_tim, &sumsq_read_tim, 1, MPI_DOUBLE, MPI_SUM,
		MPI_COMM_WORLD);
#else
	max_read_tim = read_tim;
	min_read_tim = read_tim;
	sum_write_tim = read_tim;
#endif

#ifdef __USE_MPI__
	MPI_Allreduce(&write_tim, &max_write_tim, 1, MPI_DOUBLE, MPI_MAX,
		MPI_COMM_WORLD);
	MPI_Allreduce(&write_tim, &min_write_tim, 1, MPI_DOUBLE, MPI_MIN,
		MPI_COMM_WORLD);
	MPI_Allreduce(&write_tim, &sum_write_tim, 1, MPI_DOUBLE, MPI_SUM,
		MPI_COMM_WORLD);
	/* calculate our part of the summation used for variance */
	sq_write_tim = write_tim - (sum_write_tim / nprocs);
	sq_write_tim = sq_write_tim * sq_write_tim;
	MPI_Allreduce(&sq_write_tim, &sumsq_write_tim, 1, MPI_DOUBLE, MPI_SUM,
		MPI_COMM_WORLD);
#else
	max_write_tim = write_tim;
	min_write_tim = write_tim;
	sum_write_tim = write_tim;
#endif
	/* calculate the mean from the sum */
	ave_read_tim = sum_read_tim / nprocs; 
	ave_write_tim = sum_write_tim / nprocs; 

	/* calculate variance */
	if (nprocs > 1) {
		var_read_tim = sumsq_read_tim / (nprocs-1);
		var_write_tim = sumsq_write_tim / (nprocs-1);
	}
	else {
		var_read_tim = 0;
		var_write_tim = 0;
	}

	/* unlink the file(s) if asked to */
	if (opt_unlink != 0) {
		if (mynod < opt_files) {
			err = pvfs_unlink(opt_file);
			if (err < 0) {
				fprintf(stderr, "node %d, unlink error, file = %s: %s\n", mynod,
				opt_file, strerror(myerrno));
			}
		}
	}
	
	/* print out the results on one node */
	if (mynod == 0) {
	   read_bw = ((int64_t)(opt_block*nprocs*opt_iter))/
		(max_read_tim*1000000.0);
	   write_bw = ((int64_t)(opt_block*nprocs*opt_iter))/
		(max_write_tim*1000000.0);
		
		printf("nr_procs = %d, nr_iter = %d, blk_sz = %Ld, nr_files = %d\n",
		nprocs, opt_iter, opt_block, opt_files);
		
		printf("# total_size = %Ld\n", (opt_block*nprocs*opt_iter));
		
		if (opt_write)
			printf("# Write:  min_t = %f, max_t = %f, mean_t = %f, var_t = %f\n", 
			min_write_tim, max_write_tim, ave_write_tim, var_write_tim);
		if (opt_read)
			printf("# Read:  min_t = %f, max_t = %f, mean_t = %f, var_t = %f\n", 
			min_read_tim, max_read_tim, ave_read_tim, var_read_tim);
		
		if (opt_write)
			printf("Write bandwidth = %f Mbytes/sec\n", write_bw);
	   if (opt_read)
			printf("Read bandwidth = %f Mbytes/sec\n", read_bw);
		
		if (opt_correct)
			printf("Correctness test %s.\n", correct ? "passed" : "failed");
	}

die_jar_jar_die:
	free(tmp);
	if (opt_correct) free(tmp2);
#ifdef __USE_MPI__
	MPI_Finalize();
#endif
	return(0);
}

int parse_args(int argc, char **argv)
{
	int c;
	
	while ((c = getopt(argc, argv, "s:b:i:f:p:n:cvwru")) != EOF) {
		switch (c) {
			case 's': /* stripe */
				opt_stripe = atoi(optarg);
				break;
			case 'b': /* block size */
				opt_block = atoi(optarg);
				break;
			case 'n': /* number of files */
				opt_files = atoi(optarg);
				break;
			case 'i': /* iterations */
				opt_iter = atoi(optarg);
				break;
			case 'f': /* filename */
				strncpy(opt_file, optarg, 255);
				break;
			case 'p': /* pvfstab file */
				strncpy(opt_pvfstab, optarg, 255);
				opt_pvfstab_set = 1;
				break;
			case 'c': /* correctness */
				if (opt_write && opt_read) {
					opt_correct = 1;
				}
				else {
					fprintf(stderr, "%s: cannot test correctness without"
					" reading AND writing\n", argv[0]);
					exit(1);
				}
				break;
			case 'u': /* unlink after test */
				opt_unlink = 1;
				break;
			case 'v': /* use vfs interface */
				opt_vfs = 1;
				break;
			case 'r': /* read only */
				opt_write = 0;
				opt_read = 1;
				opt_correct = 0; /* can't check correctness without both */
				break;
			case 'w': /* write only */
				opt_write = 1;
				opt_read = 0;
				opt_correct = 0; /* can't check correctness without both */
				break;
			case '?': /* unknown */
			default:
				fprintf(stderr, "usage: %s [-b blksz] [-i iter] [-f file] "
				"[-p pvfstab] [-s stripe] [-n nrfiles] [-c] [-w] [-r] [-v]\n\n"
				"  -b blksz      access blocks of blksz bytes (default=16MB)\n"
				"  -i iter       perform iter accesses (default=1)\n"
				"  -n nrfiles    use N files for testing (default=1)\n"
				"  -f file       name of file to access\n"
				"  -p pvfstab    location of pvfstab file (default=/etc/pvfstab)\n"
				"  -s stripe     stripe size for output file if created\n"
				"  -c            test for correctness\n"
				"  -w            write only\n"
				"  -r            read only\n"
				"  -u            unlink file(s) after test\n"
				"  -v            use VFS calls instead of PVFS ones\n",
				argv[0]);
				exit(1);
				break;
		}
	}
	return(0);
}

/* Wtime() - returns current time in sec., in a double */
double Wtime()
{
	struct timeval t;
	
	gettimeofday(&t, NULL);
	return((double)t.tv_sec + (double)t.tv_usec / 1000000);
}





