#include <XrdCeph/ceph_posix.h>

#include "CephOssDir.hh"

CephOssDir::CephOssDir(CephOss *cephOss) : m_dirp(0), m_cephOss(cephOss) {}

int CephOssDir::Opendir(const char *path, XrdOucEnv &env) {
  if (strlen(path) != 1 || path[0] != '/') {
    return -ENOENT;
  }
  m_dirp = ceph_posix_opendir(m_cephOss->getPoolFromEnv(&env));
  if (0 == m_dirp) {
    return -errno;
  }
  return XrdOssOK;
}

int CephOssDir::Close(long long *retsz) {
  ceph_posix_closedir(m_dirp);
  return XrdOssOK;
}

int CephOssDir::Readdir(char *buff, int blen) {
  return ceph_posix_readdir(m_dirp, buff, blen);
}
