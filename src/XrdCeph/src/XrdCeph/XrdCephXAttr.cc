//------------------------------------------------------------------------------
// Copyright (c) 2014-2015 by European Organization for Nuclear Research (CERN)
// Author: Sebastien Ponce <sebastien.ponce@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include "XrdVersion.hh"
#include "XrdCeph/XrdCephPosix.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdCeph/XrdCephXAttr.hh"

XrdSysError XrdCephXattrEroute(0);
XrdOucTrace XrdCephXattrTrace(&XrdCephXattrEroute);

extern "C"
{
  XrdSysXAttr*
  XrdSysGetXAttrObject(XrdSysError  *errP,
                       const char   *config_fn,
                       const char   *parms)
  {
    // Do the herald thing
    XrdCephXattrEroute.SetPrefix("cephxattr_");
    XrdCephXattrEroute.logger(errP->logger());
    XrdCephXattrEroute.Say("++++++ CERN/IT-DSS XrdCephXattr");
    // set parameters
    try {
      ceph_posix_set_defaults(parms);
    } catch (std::exception &e) {
      XrdCephXattrEroute.Say("CephXattr loading failed with exception. Check the syntax of parameters : ", parms);
      return 0;
    }
    return new XrdCephXAttr();
  }
}

XrdCephXAttr::XrdCephXAttr() {}

XrdCephXAttr::~XrdCephXAttr() {}

int XrdCephXAttr::Del(const char *Aname, const char *Path, int fd) {
  try {
    return ceph_posix_removexattr(0, Path, Aname);
  } catch (std::exception &e) {
    XrdCephXattrEroute.Say("Del : invalid syntax in file parameters", Path);
    return -EINVAL;
  }
}

void XrdCephXAttr::Free(AList *aPL) {
  ceph_posix_freexattrlist(aPL);
}

int XrdCephXAttr::Get(const char *Aname, void *Aval, int Avsz,
                   const char *Path,  int fd) {
  if (fd >= 0) {
    return ceph_posix_fgetxattr(fd, Aname, Aval, Avsz);
  } else {
    try {
      return ceph_posix_getxattr(0, Path, Aname, Aval, Avsz);
    } catch (std::exception &e) {
      XrdCephXattrEroute.Say("Get : invalid syntax in file parameters", Path);
      return -EINVAL;
    }
  }
}

int XrdCephXAttr::List(AList **aPL, const char *Path, int fd, int getSz) {
  if (fd > 0) {
    return ceph_posix_flistxattrs(fd, aPL, getSz);
  } else {
    try {
      return ceph_posix_listxattrs(0, Path, aPL, getSz);
    } catch (std::exception &e) {
      XrdCephXattrEroute.Say("List : invalid syntax in file parameters", Path);
      return -EINVAL;
    }
  }
}

int XrdCephXAttr::Set(const char *Aname, const void *Aval, int Avsz,
                      const char *Path,  int fd,  int isNew) {
  if (fd >= 0) {
    return ceph_posix_fsetxattr(fd, Aname, Aval, Avsz, 0);
  } else {
    try {
      return ceph_posix_setxattr(0, Path, Aname, Aval, Avsz, 0);
    } catch (std::exception &e) {
      XrdCephXattrEroute.Say("Set : invalid syntax in file parameters", Path);
      return -EINVAL;
    }
  }
}

XrdVERSIONINFO(XrdSysGetXAttrObject, XrdCephXAttr);
