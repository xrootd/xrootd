#include "XrdVersion.hh"
#include "XrdCeph/XrdCephPosix.h"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdCeph/XrdCephXAttr.hh"

XrdSysError CephXattrEroute(0);
XrdOucTrace CephXattrTrace(&CephXattrEroute);

extern "C"
{
  XrdSysXAttr*
  XrdSysGetXAttrObject(XrdSysError  *errP,
                       const char   *config_fn,
                       const char   *parms)
  {
    // Do the herald thing
    CephXattrEroute.SetPrefix("cephxattr_");
    CephXattrEroute.logger(errP->logger());
    CephXattrEroute.Say("++++++ CERN/IT-DSS XrdCephXattr");
    // set parameters
    try {
      ceph_posix_set_defaults(parms);
    } catch (std::exception e) {
      CephXattrEroute.Say("CephXattr loading failed with exception. Check the syntax of parameters : ", parms);
      return 0;
    }
    return new CephXAttr();
  }
}

CephXAttr::CephXAttr() {}

CephXAttr::~CephXAttr() {}

int CephXAttr::Del(const char *Aname, const char *Path, int fd) {
  try {
    return ceph_posix_removexattr(0, Path, Aname);
  } catch (std::exception e) {
    CephXattrEroute.Say("Del : invalid syntax in file parameters", Path);
    return -EINVAL;
  }
}

void CephXAttr::Free(AList *aPL) {
  ceph_posix_freexattrlist(aPL);
}

int CephXAttr::Get(const char *Aname, void *Aval, int Avsz,
                   const char *Path,  int fd) {
  if (fd >= 0) {
    return ceph_posix_fgetxattr(fd, Aname, Aval, Avsz);
  } else {
    try {
      return ceph_posix_getxattr(0, Path, Aname, Aval, Avsz);
    } catch (std::exception e) {
      CephXattrEroute.Say("Get : invalid syntax in file parameters", Path);
      return -EINVAL;
    }
  }
}

int CephXAttr::List(AList **aPL, const char *Path, int fd, int getSz) {
  if (fd > 0) {
    return ceph_posix_flistxattrs(fd, aPL, getSz);
  } else {
    try {
      return ceph_posix_listxattrs(0, Path, aPL, getSz);
    } catch (std::exception e) {
      CephXattrEroute.Say("List : invalid syntax in file parameters", Path);
      return -EINVAL;
    }
  }
}

int CephXAttr::Set(const char *Aname, const void *Aval, int Avsz,
                   const char *Path,  int fd,  int isNew) {
  if (fd >= 0) {
    return ceph_posix_fsetxattr(fd, Aname, Aval, Avsz, 0);
  } else {
    try {
      return ceph_posix_setxattr(0, Path, Aname, Aval, Avsz, 0);
    } catch (std::exception e) {
      CephXattrEroute.Say("Set : invalid syntax in file parameters", Path);
      return -EINVAL;
    }
  }
}

XrdVERSIONINFO(XrdSysGetXAttrObject, CephXattr);
