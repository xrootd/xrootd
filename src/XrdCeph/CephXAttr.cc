#include <XrdVersion.hh>
#include <XrdCeph/ceph_posix.h>
#include "CephXAttr.hh"

extern "C"
{
  XrdSysXAttr*
  XrdSysGetXAttrObject(XrdSysError  *errP,
                       const char   *config_fn,
                       const char   *parms)
  {
    ceph_posix_set_defaults(parms);
    return new CephXAttr();
  }
}

CephXAttr::CephXAttr() {}
  
CephXAttr::~CephXAttr() {}

int CephXAttr::Del(const char *Aname, const char *Path, int fd) {
  return ceph_posix_removexattr(0, Path, Aname);
}

void CephXAttr::Free(AList *aPL) {
  ceph_posix_freexattrlist(aPL);
}

int CephXAttr::Get(const char *Aname, void *Aval, int Avsz,
                   const char *Path,  int fd) {
  if (fd >= 0) {
    return ceph_posix_fgetxattr(fd, Aname, Aval, Avsz);
  } else {
    return ceph_posix_getxattr(0, Path, Aname, Aval, Avsz);
  }
}

int CephXAttr::List(AList **aPL, const char *Path, int fd, int getSz) {
  if (fd > 0) {
    return ceph_posix_flistxattrs(fd, aPL, getSz);
  } else {
    return ceph_posix_listxattrs(0, Path, aPL, getSz);
  }
}

int CephXAttr::Set(const char *Aname, const void *Aval, int Avsz,
                   const char *Path,  int fd,  int isNew) {
  if (fd >= 0) {
    return ceph_posix_fsetxattr(fd, Aname, Aval, Avsz, 0);
  } else {
    return ceph_posix_setxattr(0, Path, Aname, Aval, Avsz, 0);
  }
}

XrdVERSIONINFO(XrdSysGetXAttrObject, CephXattr);
