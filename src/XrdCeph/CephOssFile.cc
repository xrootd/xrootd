#include <sys/types.h>
#include <unistd.h>

#include <XrdCeph/ceph_posix.h>
#include <XrdOuc/XrdOucEnv.hh>

#include "CephOssFile.hh"
#include "CephOss.hh"

CephOssFile::CephOssFile(CephOss *cephOss) : m_fd(-1), m_cephOss(cephOss) {}

int CephOssFile::Open(const char *path, int flags, mode_t mode, XrdOucEnv &env) {
  m_fd = ceph_posix_open(&env, path, flags, mode);
  return XrdOssOK;
}

int CephOssFile::Close(long long *retsz) {
  return ceph_posix_close(m_fd);
}

ssize_t CephOssFile::Read(off_t offset, size_t blen) {
  return XrdOssOK;
}

ssize_t CephOssFile::Read(void *buff, off_t offset, size_t blen) {
  int rc = ceph_posix_lseek(m_fd, offset, SEEK_SET);
  if (0 == rc) {
    return ceph_posix_read(m_fd, buff, blen);
  }
  return rc;
}

int CephOssFile::Read(XrdSfsAio *aoip) {
  return -ENOTSUP;
}

ssize_t CephOssFile::ReadRaw(void *buff, off_t offset, size_t blen) {
  return Read(buff, offset, blen);
}

int CephOssFile::Fstat(struct stat *buff) {
  return ceph_posix_fstat(m_fd, buff);
}

ssize_t CephOssFile::Write(const void *buff, off_t offset, size_t blen) {
  int rc = ceph_posix_lseek(m_fd, offset, SEEK_SET);
  if (0 == rc) {
    return ceph_posix_write(m_fd, buff, blen);
  }
  return rc;
}

int CephOssFile::Fsync() {
  return ceph_posix_fsync(m_fd);
}

int CephOssFile::Ftruncate(unsigned long long len) {
  return ceph_posix_ftruncate(m_fd, len);
}
