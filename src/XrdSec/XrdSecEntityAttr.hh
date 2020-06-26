#ifndef __SEC_ENTITYATTR_H__
#define __SEC_ENTITYATTR_H__
/******************************************************************************/
/*                                                                            */
/*                   X r d S e c E n t i t y A t t r . h h                    */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

//------------------------------------------------------------------------------
//! This object is a non-const extension of the XrdSecEntity object. It is
//! used as the interface to XrdSecEntity attributes. Normally, a const
//! pointer is used for the XrdSecEntity object as nothing changes in the
//! entity. However, attributes may be added and deleted from the entity
//! changing the logical view of the entity. This provides a non-const
//! mechanism to this without the need to recast the XrdSecEntity pointer.
//------------------------------------------------------------------------------

#include <sys/types.h>

#include <string>
#include <vector>

class XrdSecAttr;
class XrdSecEntityAttrCB;
class XrdSecEntityXtra;
  
/******************************************************************************/
/*                      X r d S e c E n t i t y A t t r                       */
/******************************************************************************/
  
class XrdSecEntityAttr
{
public:
friend class XrdSecEntity;

//------------------------------------------------------------------------------
//! Add an attribute object to this entity.
//!
//! @param  attr    - Reference to the attribute object.
//!
//! @return True, the object was added.
//! @return False, the object was not added because such an object exists.
//------------------------------------------------------------------------------

         bool    Add(XrdSecAttr &attr);

//------------------------------------------------------------------------------
//! Add a key-value attribute to this entity. If one exists it is replaced.
//!
//! @param  key     - Reference to the key.
//! @param  val     - Reference to the value.
//! @param  replace - When true, any existing key-value is replaced. Otherwise,
//!                   the add is not performed.
//!
//! @return True, the key-value was added or replaced.
//! @return False, the key already exists so he value was not added.
//------------------------------------------------------------------------------

         bool    Add(const std::string &key,
                     const std::string &val, bool replace=false);

//------------------------------------------------------------------------------
//! Get an attribute object associated with this entity.
//!
//! @param  sigkey  - A unique attribute object signature key.
//!
//! @return Upon success a pointer to the attribute object is returned.
//!         Otherwise, a nil pointer is returned.
//------------------------------------------------------------------------------

XrdSecAttr      *Get(const void *sigkey);

//------------------------------------------------------------------------------
//! Get an attribute key value associated with this entity.
//!
//! @param  key     - The reference to the key.
//! @param  val     - The reference to the string object to receive the value.
//!
//! @return Upon success true is returned. If the key does not exist, false
//!         is returned and the val object remains unchanged.
//------------------------------------------------------------------------------

         bool    Get(const std::string &key, std::string &val);

//------------------------------------------------------------------------------
//! Get all the keys for associated attribytes.
//!
//! @return A vector containing all of the keys.
//------------------------------------------------------------------------------

std::vector<std::string> Keys();

//------------------------------------------------------------------------------
//! List key-value pairs via iterative callback on passed ovject.
//!
//! @param  attrCB  - Reference to the callback object to receive list entries.
//------------------------------------------------------------------------------

         void    List(XrdSecEntityAttrCB &attrCB);

//------------------------------------------------------------------------------
//! Constructor and Destructor.
//!
//! @param  xtra    - Pointer to the data for the implementation.
//------------------------------------------------------------------------------

         XrdSecEntityAttr(XrdSecEntityXtra *xtra) : entXtra(xtra) {}

        ~XrdSecEntityAttr() {}

private:

XrdSecEntityXtra *entXtra;
};

/******************************************************************************/
/*                    X r d S e c E n t i t y A t t r C B                     */
/******************************************************************************/

// The XrdSecEntityAttrCB class defines the callback object passed to the
// XrdSecEntity::List() method to iteratively obtain the key-value attribute
// pairs associated with the entity. The XrdSecEntityAttrCB::Attr() method is
// called for each key-value pair. The end of the list is indicated by calling
// Attr() with nil key-value pointers. The Attr() method should not call
// the XrdSecEntity::Add() or XrdSecEntity::Get() methods; otherwise, a
// deadlock will occur.
//
class XrdSecEntityAttrCB
{
public:

//------------------------------------------------------------------------------
//! Acceppt a key-value attribute pair from the XrdSecEntity::List() method.
//!
//! @param  key   - The key, if nil this is the end of the list.
//! @param  val   - The associated value, if nil this is the end of the list.
//!
//! @return One of the Action enum values. The return value is ignored when
//!         the end of the list indicator is returned.
//------------------------------------------------------------------------------

enum     Action {Delete = -1, //!< Delete the key-value and proceed to next one
                 Stop   =  0, //!< Stop the iteration
                 Next   =  1  //!< Proceed to the next key-value pair
                };

virtual  Action Attr(const char *key, const char *val) = 0;

//------------------------------------------------------------------------------
//! Constructor and Destructor.
//------------------------------------------------------------------------------

         XrdSecEntityAttrCB() {}
virtual ~XrdSecEntityAttrCB() {}
};
#endif
