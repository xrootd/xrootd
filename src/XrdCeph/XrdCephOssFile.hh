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

#ifndef __XRD_CEPH_OSS_FILE_HH__
#define __XRD_CEPH_OSS_FILE_HH__

#include "XrdOss/XrdOss.hh"
#include "XrdCeph/XrdCephOss.hh"

//------------------------------------------------------------------------------
//! This class implements XrdOssDF interface for usage with a CEPH storage.
//!
//! This plugin is able to use any pool of ceph with any userId.
//! There are several ways to provide the pool and userId to be used for a given
//! operation. Here is the ordered list of possibilities.
//! First one defined wins :
//!   - the path can be prepended with userId and pool. Syntax is :
//!       [[userId@]pool:]<actual path>
//!   - the XrdOucEnv parameter, when existing, can have 'cephUserId' and/or
//!     'cephPool' entries
//!   - the ofs.osslib directive can provide an argument with format :
//!       [userID@]pool
//!   - default are 'admin' and 'default' for userId and pool respectively
//!
//! Note that the definition of a default via the ofs.osslib directive may
//! clash with one used in a ofs.xattrlib directive. In case both directives
//! have a default and they are different, the behavior is not defined.
//! In case one of the two only has a default, it will be applied for both plugins.
//------------------------------------------------------------------------------

class XrdCephOssFile : public XrdOssDF {

public:

  XrdCephOssFile(XrdCephOss *cephoss);
  virtual ~XrdCephOssFile() {};
  virtual int Open(const char *path, int flags, mode_t mode, XrdOucEnv &env);
  virtual int Close(long long *retsz=0);
  virtual ssize_t Read(off_t offset, size_t blen);
  virtual ssize_t Read(void *buff, off_t offset, size_t blen);
  virtual int     Read(XrdSfsAio *aoip);
  virtual ssize_t ReadRaw(void *, off_t, size_t);
  virtual int Fstat(struct stat *buff);
  virtual ssize_t Write(const void *buff, off_t offset, size_t blen);
  virtual int Write(XrdSfsAio *aiop);
  virtual int Fsync(void);
  virtual int Ftruncate(unsigned long long);

private:

  int m_fd;
  XrdCephOss *m_cephOss;

};

#endif /* __XRD_CEPH_OSS_FILE_HH__ */
