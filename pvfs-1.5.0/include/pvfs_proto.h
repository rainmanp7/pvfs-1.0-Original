#ifndef PVFS_PROTO_H
#define PVFS_PROTO_H

int pvfs_access(char* pathname, int mode);
int pvfs_chmod(const char* pathname, mode_t mode);
int pvfs_chown(const char* pathname, uid_t owner, gid_t group);
int pvfs_close(int fd);
int pvfs_dup(int fd);
int pvfs_dup2(int old_fd, int new_fd);
int pvfs_fchmod(int fd, mode_t mode);
int pvfs_fchown(int fd, uid_t owner, gid_t group);
int pvfs_fcntl(int fd, int cmd, long arg);
int pvfs_fdatasync(int fd);
int pvfs_flock(int fd, int operation);
int pvfs_fstat(int fd, struct stat *buf);
int pvfs_fsync(int fd);
int pvfs_ftruncate(int fd, size_t length);
int pvfs_ftruncate64(int fd, int64_t length);
int pvfs_ioctl(int fd, int cmd, void *data);
int pvfs_lseek(int fd, int off, int whence);
int64_t pvfs_lseek64(int fd, int64_t off, int whence);
int64_t pvfs_llseek(int fd, int64_t off, int whence);
int pvfs_lstat(char* pathname, struct stat *buf);
int pvfs_mkdir(const char* pathname, int mode);
int pvfs_munmap(void *start, size_t length);
int pvfs_open(const char* pathname, int flag, mode_t mode, void *, void *);
int pvfs_open64(const char* pathname, int flag, mode_t mode, void *, void *);
int pvfs_creat(const char* pathname, mode_t mode);
int pvfs_ostat(char* pathname, struct stat *buf);
int pvfs_stat(char* pathname, struct stat *buf);
int pvfs_read(int fd, char *buf, size_t count);
int pvfs_readv(int fd, const struct iovec *vector, size_t count);
int pvfs_rmdir(const char* pathname);
int pvfs_truncate(const char *path, size_t length);
int pvfs_truncate64(const char *path, int64_t length);
int pvfs_unlink(const char* pathname);
int pvfs_utime(const char *filename, struct utimbuf *buf);
int pvfs_write(int fd, char *buf, size_t count);
int pvfs_writev(int fd, const struct iovec *vector, size_t count);
#endif
