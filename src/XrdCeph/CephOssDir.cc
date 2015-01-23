#include <XrdCeph/ceph_posix.h>
#include <XrdSys/XrdSysError.hh>
#include <XrdOuc/XrdOucTrace.hh>

#include "CephOssDir.hh"

extern XrdSysError CephEroute;

CephOssDir::CephOssDir(CephOss *cephOss) : m_dirp(0), m_cephOss(cephOss) {}

int CephOssDir::Opendir(const char *path, XrdOucEnv &env) {
  try {
    m_dirp = ceph_posix_opendir(&env, path);
    if (0 == m_dirp) {
      return -errno;
    }
    return XrdOssOK;
  } catch (std::exception e) {
    CephEroute.Say("opendir : invalid syntax in file parameters");
    return -EINVAL;
  }
}

int CephOssDir::Close(long long *retsz) {
  ceph_posix_closedir(m_dirp);
  return XrdOssOK;
}

int CephOssDir::Readdir(char *buff, int blen) {
  return ceph_posix_readdir(m_dirp, buff, blen);
}
