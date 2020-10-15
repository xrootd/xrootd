#ifndef __XRDNETREGISTRY_HH__
#define __XRDNETREGISTRY_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d N e t R e g i s t r y . h h                      */
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

#include <string>

#include "XrdNet/XrdNetUtils.hh"

class XrdNetAddr;

class XrdNetRegistry
{
public:

static const char pfx = '%'; //!< Registry names must start with this character

//------------------------------------------------------------------------------
//! Return addresses associated with a registered name.
//!
//! @param  hSpec    Reference to address specification & must start with "pfx".
//! @param  aVec     Reference to the vector to contain addresses.
//! @param  ordn     Pointer to where the partition ordinal is to be stored.
//! @param  opts     Options on what to return (see XrdNetUtils).
//! @param  pNum     Port number argument (see XrdNetUtils), ignored for now.
//!
//! @return Success: 0 is returned. When ordn is not nil, the number of IPv4
//!                  entries (for order46) or IPv6 (for order64) entries that
//!                  appear in the front of the vector. If ordering is not
//!                  specified, the value is set to the size of the vector.
//!         Failure: the error message text describing the error and aVec is
//!                  cleared (i.e. has no elements).
//------------------------------------------------------------------------------
static
const char  *GetAddrs(const std::string       &hSpec,
                      std::vector<XrdNetAddr> &aVec, int *ordn=0,
                      XrdNetUtils::AddrOpts opts=XrdNetUtils::allIPMap,
                      int pNum=XrdNetUtils::PortInSpec);

//------------------------------------------------------------------------------
//! Register a pseuo-hostname to be associated with a list of hosts.
//!
//! @param  hName    The pseudo-hostname which must start with 'pfx'.
//! @param  hList    A list of "host:port" entries to register as hName.
//! @param  hLNum    The number of entries in hList.
//! @param  eText    When not null, the reason for the registration failure.
//! @param  rotate   When true, the returned host list will be rotated +1
//!                  relative to the previously returned list of hosts.
//!
//! @return True upon success and false otherwise.
//!
//! @note The expanded list is returned when GetAddrs() is called with hName.
//------------------------------------------------------------------------------

static bool Register(const char *hName, const char *hList[], int hLNum,
                     std::string *eText=0, bool rotate=false);

//------------------------------------------------------------------------------
//! Register a pseuo-hostname to be associated with a string of hosts.
//!
//! @param  hName    The pseudo-hostname which must start with 'pfx'.
//! @param  hList    A string of comma separated "host:port" entries to register.
//!                  If the name starts with 'pfx' then it assumed to be a
//!                  single name where hName is to become its alias. Alias rules
//!                  are: 1) hName must not exist, 2) the target must exist, and
//!                  3) if the target is an alias hName becomes an alias of
//!                  it's target (i.e. its parent).
//! @param  eText    When not null, the reason for the registration failure.
//! @param  rotate   When true, the returned host list will be rotated +1
//!                  relative to the previously returned list of hosts.
//!
//! @return True upon success and false otherwise.
//!
//! @note The expanded list is returned when GetAddrs() is called with hName.
//------------------------------------------------------------------------------

static bool Register(const char *hName, const char *hList,
                     std::string *eText=0, bool rotate=false);

            XrdNetRegistry() {}
           ~XrdNetRegistry() {}

private:

static bool Resolve(const char *hList, std::string *eText=0);
static bool SetAlias(const char *hAlias, const char *hName, std::string *eText=0);
};
#endif
