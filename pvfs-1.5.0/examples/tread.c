#include <stdio.h>
#include <malloc.h>
#include <fcntl.h>

int main(int argc, char **argv) {
	int off = 0, bsz = 65536, fd, i;
	char *fn;
	void *bptr;

	if (argc < 2 || argc > 4) {
		fprintf(stderr, "usage: %s <filename> <offset> <size>\n", argv[0]);
		exit(-1);
	}

	if (argc > 1) fn = argv[1];
	if (argc > 2) off = atoi(argv[2]);
	if (argc > 3) bsz = atoi(argv[3]);

	if ((fd = pvfs_open(fn, O_RDONLY)) < 0) {
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
	
	if ((i = pvfs_read(fd, bptr, bsz)) < 0) {
		perror("write");
		exit(-1);
	}
	if (i != bsz) fprintf(stderr,
		"warning: pvfs_read: received %d of %d bytes.\n", i, bsz);

	if (pvfs_close(fd) < 0) {
		perror("close");
		exit(-1);
	}
}
