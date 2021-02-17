#include <stdio.h>
#include <malloc.h>
#include <fcntl.h>
#include <pvfs.h>

int main(int argc, char **argv) {
	int off = 0, bsz = 65536, fd;
	char *fn;
	void *bptr;
	struct pvfs_filestat pstat = {0,-1,16384,0,0};

	if (argc < 2 || argc > 5) {
		fprintf(stderr, "usage: %s <filename> <offset> <size> <nr_nodes>\n",
			argv[0]);
		exit(-1);
	}

	if (argc > 1) fn = argv[1];
	if (argc > 2) off = atoi(argv[2]);
	if (argc > 3) bsz = atoi(argv[3]);
	if (argc > 4) pstat.pcount = atoi(argv[4]);

	if ((fd = pvfs_open(fn, O_RDWR|O_CREAT|O_TRUNC|O_META, 0666, &pstat)) < 0) {
		perror("open");
		exit(-1);
	}

	if (off && pvfs_lseek(fd, off, SEEK_SET) < 0) {
		perror("lseek");
		exit(-1);
	}

	if (!(bptr = malloc(bsz))) {
		perror("malloc");
		exit(-1);
	}
	
	if (pvfs_write(fd, bptr, bsz) < 0) {
		perror("write");
		exit(-1);
	}

	if (pvfs_close(fd) < 0) {
		perror("close");
		exit(-1);
	}
}
