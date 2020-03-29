#ifndef __SecsssCon__
#define __SecsssCon__
/******************************************************************************/
/*                                                                            */
/*                       X r d S e c s s s C o n . h h                        */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include <set>
#include <string>

//-----------------------------------------------------------------------------
//! The XrdSecsssCon class provides a mechanism to track and cleanup contacts
//! (i.e. connections). It contains a defined method to add a contact and
//! an abstract method to cleanup contacts when the XrdSecEntity object is
//! unregistered from the XrdSecsssID object.
//-----------------------------------------------------------------------------

class XrdSecEntity;

class XrdSecsssCon
{
public:

//-----------------------------------------------------------------------------
//! Cleanup connections established by the passed entity.
//!
//! @param  Contacts Reference to a set of connections created by the entity.
//!                  Each entry in the form of 'user[:pswd]@host:port'.
//! @param  Entity   Reference to the entity object responsible for the contacts.
//!
//! @note 1) This object is passed to the XrdSecsssID constructor.
//!       2) It is expected that the callee will disconnect each connection.
//!       3) Upon return the Contacts and Entity objects are deleted.
//-----------------------------------------------------------------------------

virtual void Cleanup(const std::set<std::string> &Contacts,
                     const XrdSecEntity          &Entity) = 0;

//-----------------------------------------------------------------------------
//! Add a contact for the indicated loginid entity.
//!
//! @param  lgnid  - The loginid used to to register an Entity via XrdSecsssID.
//! @param  hostID - The hostID (i.e. lgnid[:pswd]@host:port).
//!
//! @return true   - Contact added.
//! @return false  - Contact not added as the lgnid is not registered.
//-----------------------------------------------------------------------------

        bool Contact(const std::string &lgnid, const std::string &hostID);

             XrdSecsssCon() {}
virtual     ~XrdSecsssCon() {}
};
#endif
