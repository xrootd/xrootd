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

#include "XrdCeph/XrdCephPosix.hh"
#include "XrdCeph/XrdCephOssDir.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"

extern XrdSysError XrdCephEroute;

XrdCephOssDir::XrdCephOssDir(XrdCephOss *cephOss) : m_dirp(0), m_cephOss(cephOss) {}

int XrdCephOssDir::Opendir(const char *path, XrdOucEnv &env) {
  try {
    m_dirp = ceph_posix_opendir(&env, path);
    if (0 == m_dirp) {
      return -errno;
    }
    return XrdOssOK;
  } catch (std::exception &e) {
    XrdCephEroute.Say("opendir : invalid syntax in file parameters");
    return -EINVAL;
  }
}

int XrdCephOssDir::Close(long long *retsz) {
  ceph_posix_closedir(m_dirp);
  return XrdOssOK;
}

int XrdCephOssDir::Readdir(char *buff, int blen) {
  return ceph_posix_readdir(m_dirp, buff, blen);
}
