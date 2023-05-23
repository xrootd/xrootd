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

#ifndef __XRD_CEPH_XATTR_HH__
#define __XRD_CEPH_XATTR_HH__

#include <XrdSys/XrdSysXAttr.hh>

//------------------------------------------------------------------------------
//! This class implements XrdSysXAttr interface for usage with a CEPH storage.
//! It should be loaded via the ofs.xattrlib directive.
//!
//! This plugin is able to use any pool of ceph with any userId.
//! There are several ways to provide the pool and userId to be used for a given
//! operation. Here is the ordered list of possibilities.
//! First one defined wins :
//!   - the path can be prepended with userId and pool. Syntax is :
//!       [[userId@]pool:]<actual path>
//!   - the XrdOucEnv parameter, when existing, can have 'cephUserId' and/or
//!     'cephPool' entries
//!   - the ofs.xattrlib directive can provide an argument with format :
//!       [userID@]pool
//!   - default are 'admin' and 'default' for userId and pool respectively
//!
//! Note that the definition of a default via the ofs.xattrlib directive may
//! clash with one used in a ofs.osslib directive. In case both directives
//! have a default and they are different, the behavior is not defined.
//! In case one of the two only has a default, it will be applied for both plugins.
//------------------------------------------------------------------------------

class XrdCephXAttr : public XrdSysXAttr {

public:

  //------------------------------------------------------------------------------
  //! Constructor
  //------------------------------------------------------------------------------
  XrdCephXAttr();

  //------------------------------------------------------------------------------
  //! Destructor
  //------------------------------------------------------------------------------
  virtual ~XrdCephXAttr();

  //------------------------------------------------------------------------------
  //! Remove an extended attribute.
  //!
  //! @param  Aname  -> The attribute name.
  //! @param  Path   -> Path of the file whose attribute is to be removed.
  //! @param  fd        If >=0 is the file descriptor of the opened subject file.
  //!
  //! @return =0     Attribute was successfully removed or did not exist.
  //! @return <0     Attribute was not removed, the return value is -errno that
  //!                describes the reason for the failure.
  //------------------------------------------------------------------------------
  virtual int Del(const char *Aname, const char *Path, int fd=-1);

  //------------------------------------------------------------------------------
  //! Release storage occupied by the Alist structure returned by List().
  //!
  //! @param  aPL    -> The first element of the AList structure.
  //------------------------------------------------------------------------------

  virtual void Free(AList *aPL);

  //------------------------------------------------------------------------------
  //! Get an attribute value and its size.
  //!
  //! @param  Aname  -> The attribute name.
  //! @param  Aval   -> Buffer to receive the attribute value.
  //! @param  Avsz      Length of the buffer in bytes. Only up to this number of
  //!                   bytes should be returned. However, should Avsz be zero
  //!                   the the size of the attribute value should be returned
  //!                   and the Aval argument should be ignored.
  //! @param  Path   -> Path of the file whose attribute is to be fetched.
  //! @param  fd     -> If >=0 is the file descriptor of the opened subject file.
  //!
  //! @return >0     The number of bytes placed in Aval. However, if avsz is zero
  //!                then the value is the actual size of the attribute value.
  //! @return =0     The attribute does not exist.
  //! @return <0     The attribute value could not be returned. The returned
  //!                value is -errno describing the reason.
  //------------------------------------------------------------------------------

  virtual int Get(const char *Aname, void *Aval, int Avsz,
                  const char *Path,  int fd=-1);

  //------------------------------------------------------------------------------
  //! Get all of the attributes associated with a file.
  //!
  //! @param  aPL    -> the pointer to hold the first element of AList. The
  //!                   storage occupied by the returned AList must be released
  //!                   by calling Free().
  //! @param  Path   -> Path of the file whose attributes are t be returned.
  //! @param  fd     -> If >=0 is the file descriptor of the opened subject file.
  //! @param  getSz     When != 0 then the size of the maximum attribute value
  //!                   should be returned. Otherwise, upon success 0 is returned.
  //!
  //! @return >0     Attributes were returned and aPL points to the first
  //!                attribute value.  The returned value is the largest size
  //!                of an attribute value encountered (getSz != 0).
  //! @return =0     Attributes were returned and aPL points to the first
  //!                attribute value (getSz == 0).
  //! @return <0     The attribute values could not be returned. The returned
  //!                value is -errno describing the reason.
  //------------------------------------------------------------------------------
  virtual int List(AList **aPL, const char *Path, int fd=-1, int getSz=0);

  //------------------------------------------------------------------------------
  //! Set an attribute.
  //!
  //! @param  Aname  -> The attribute name.
  //! @param  Aval   -> Buffer holding the attribute value.
  //! @param  Avsz      Length of the buffer in bytes. This is the length of the
  //!                   attribute value which may contain binary data.
  //! @param  Path   -> Path of the file whose attribute is to be set.
  //! @param  fd     -> If >=0 is the file descriptor of the opened subject file.
  //! @param  isNew     When !0 then the attribute must not exist (i.e. new).
  //!                   Otherwise, if it does exist, the value is replaced. In
  //!                   either case, if it does not exist it should be created.
  //!
  //! @return =0     The attribute was successfully set.
  //! @return <0     The attribute values could not be set. The returned
  //!                value is -errno describing the reason.
  //------------------------------------------------------------------------------
  virtual int Set(const char *Aname, const void *Aval, int Avsz,
                  const char *Path,  int fd=-1,  int isNew=0);

};

#endif /* __XRD_CEPH_XATTR_HH__ */
