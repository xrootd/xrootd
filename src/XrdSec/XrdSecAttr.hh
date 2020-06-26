#ifndef __SEC_ATTR_H__
#define __SEC_ATTR_H__
/******************************************************************************/
/*                                                                            */
/*                         X r d S e c A t t r . h h                          */
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

/*! The XrdSecAttr object is used as the base class to add arbitrary
    extensions to the XrdSecEntity object. A derived class definition should
    have a unique signature which is used by XrdSecEntity::Add() to
    differentiate extentions. The easiest way to do this is to use a unique
    memory address specific to all instances of the derived class. For instance,

    class myAttr :: public XrdSecAttr
    { ....
     static const <type> mySig;

          myAttr(...) : XrdSecAttr((const void *)&mySig), ...
      ....
    };

   You use this signature to retrieve an instance of the extension added to
   a specific XrdSecEntity object. Since signatures are unique the derived
   class is well known and you can safely use a static cast to downcast the
   returned pointer to the proper class (see the XrdSecEntity::Get() method).
   To successfully downcast using static_cast your derived class must be
   fully defined and in the scope of the static_cast. Otherwise, you must
   use the more expensive dynamic_cast.

   Attribute objects are deleted when the associated XrdSecEntity instance
   is deleted. This happens when the client's server connection is closed.
*/

class  XrdSecEntity;

class  XrdSecAttr
{
public:
friend class XrdSecEntityAttr;

//------------------------------------------------------------------------------
//! Delete this object (may be over-ridden for custom action).
//------------------------------------------------------------------------------

virtual void Delete() {delete this;}

//------------------------------------------------------------------------------
//! Constructor.
//!
//! @param  dSig - the unique signature for all instances of the class that
//!                uses this class as its base (i.e. the derived class).
//------------------------------------------------------------------------------

         XrdSecAttr(const void *dSig) : Signature(dSig) {}

//------------------------------------------------------------------------------
//! Destructor (always externally done via Delete() method).
//------------------------------------------------------------------------------
protected:

virtual ~XrdSecAttr() {}

private:

const void *Signature;
};
#endif
