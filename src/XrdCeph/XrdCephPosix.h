/*
 * This interface provides wrapper methods for using ceph through a POSIX API.
 */

#ifndef _CEPH_POSIX_H
#define _CEPH_POSIX_H

#include <sys/types.h>
#include <stdarg.h>
#include <dirent.h>
#include <XrdOuc/XrdOucEnv.hh>
#include <XrdSys/XrdSysXAttr.hh>

void ceph_posix_set_defaults(const char* value);
void ceph_posix_disconnect_all();
void ceph_posix_set_logfunc(void (*logfunc) (char *, va_list argp));
int ceph_posix_open(XrdOucEnv* env, const char *pathname, int flags, mode_t mode);
int ceph_posix_close(int fd);
off_t ceph_posix_lseek(int fd, off_t offset, int whence);
off64_t ceph_posix_lseek64(int fd, off64_t offset, int whence);
ssize_t ceph_posix_write(int fd, const void *buf, size_t count);
ssize_t ceph_posix_read(int fd, void *buf, size_t count);
int ceph_posix_fstat(int fd, struct stat *buf);
int ceph_posix_stat(XrdOucEnv* env, const char *pathname, struct stat *buf);
int ceph_posix_fsync(int fd);
int ceph_posix_fcntl(int fd, int cmd, ... /* arg */ );
ssize_t ceph_posix_getxattr(XrdOucEnv* env, const char* path, const char* name,
                            void* value, size_t size);
ssize_t ceph_posix_fgetxattr(int fd, const char* name, void* value, size_t size);
ssize_t ceph_posix_setxattr(XrdOucEnv* env, const char* path, const char* name,
                            const void* value, size_t size, int flags);
int ceph_posix_fsetxattr(int fd, const char* name, const void* value, size_t size, int flags);
int ceph_posix_removexattr(XrdOucEnv* env, const char* path, const char* name);
int ceph_posix_fremovexattr(int fd, const char* name);
int ceph_posix_listxattrs(XrdOucEnv* env, const char* path, XrdSysXAttr::AList **aPL, int getSz);
int ceph_posix_flistxattrs(int fd, XrdSysXAttr::AList **aPL, int getSz);
void ceph_posix_freexattrlist(XrdSysXAttr::AList *aPL);
int ceph_posix_statfs(long long *totalSpace, long long *freeSpace);
int ceph_posix_truncate(XrdOucEnv* env, const char *pathname, unsigned long long size);
int ceph_posix_ftruncate(int fd, unsigned long long size);
int ceph_posix_unlink(XrdOucEnv* env, const char *pathname);
DIR* ceph_posix_opendir(XrdOucEnv* env, const char *pathname);
int ceph_posix_readdir(DIR* dirp, char *buff, int blen);
int ceph_posix_closedir(DIR *dirp);

#endif
