#ifndef __XRDOUCGMAP_H__
#define __XRDOUCGMAP_H__
/******************************************************************************/
/*                                                                            */
/*                    X r d O u c G M a p . h h                               */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdSys/XrdSysXSLock.hh"

class XrdOucTrace;
class XrdSysError;
class XrdSecGMapEntry_t
{
public:
   XrdSecGMapEntry_t(const char *v, const char *u, int t) : val(v), user(u), type(t) { }
   XrdOucString  val;
   XrdOucString  user;
   int           type;
};

class XrdOucGMap
{
public:

//------------------------------------------------------------------------------
//! Map a distinguished name (dn) to a user name.
//!
//! @param  dn    -> Distinguished name.
//! @param  user  -> Buffer where the user name is to be placed.
//!                  It must end with a null byte.
//! @param  ulen  -> The length of the 'user' buffer.
//! @param  now   -> Current time (result of time(0)) or 0 if not available.
//!
//! @return Success: Zero.
//!         Failure: An errno number describing the failure; typically
//!                  -EFAULT       - No valid matching found.
//!                  -errno        - If problems reloading the file
//------------------------------------------------------------------------------

virtual int  dn2user(const char *dn, char *user, int ulen, time_t now = 0);


//------------------------------------------------------------------------------
//! Constructor
//!
//! @param  eDest -> The error object that must be used to print any errors or
//!                  other messages (see XrdSysError.hh).
//! @param  mapfn -> Path to the grid map file to be used. This pointer
//!                  may be null; the default path will be tested then.
//! @param  parms -> Argument string specified on the gridmap directive. It may
//!                  be null or point to a null string if no parms exist.
//!                  Curently supported parms:
//!                        dbg 	        to enable debug printouts
//!                        to=timeout   to set a timeout in secs on the validity
//!                                     of the information loaded from the file
//!                                     Default is 600 (10'); the reload is
//!                                     triggered by the first mapping request
//!                                     after the timeout is expired; the file is
//!                                     reloaded only if changed.
//!
//------------------------------------------------------------------------------
#define XrdOucGMapArgs XrdSysError       *eDest, \
                       const char        *mapfn, \
                       const char        *parms
             XrdOucGMap(XrdOucGMapArgs);

//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------

virtual     ~XrdOucGMap() {}

//------------------------------------------------------------------------------
//! Validity checker
//------------------------------------------------------------------------------

bool         isValid() const { return valid; }

private:
//------------------------------------------------------------------------------
//! Internal members
//------------------------------------------------------------------------------

bool		 				  valid;
XrdOucHash<XrdSecGMapEntry_t> mappings;
XrdOucString				  mf_name;
time_t						  mf_mtime;
time_t                        notafter;
long                          timeout;

XrdSysError                  *elogger;
XrdOucTrace                  *tracer;
bool                          dbg;

XrdSysXSLock			      xsl;

//------------------------------------------------------------------------------
//! Internal methods
//------------------------------------------------------------------------------

int           load(const char *mf, bool force = 0);
};

/******************************************************************************/
/*                    X r d O u c g e t G M a p                               */
/******************************************************************************/
  
//------------------------------------------------------------------------------
//! Obtain an instance of the XrdOucGMap object.
//!
//! This extern "C" function is called when a shared library plug-in containing
//! implementation of this class is loaded. It must exist in the shared library
//! and must be thread-safe.
//!
//! @param  eDest -> The error object that must be used to print any errors or
//!                  other messages (see XrdSysError.hh).
//! @param  mapfn -> Path to the grid map file to be used. This pointer
//!                  may be null; the default path will be tested then.
//! @param  parms -> Argument string specified on the gridmap directive. It may
//!                  be null or point to a null string if no parms exist.
//!
//! @return Success: A pointer to an instance of the XrdOucGMap object.
//!         Failure: A null pointer which causes initialization to fail.
//!
//! The GMap object is used frequently in the course of creating new physical
//! connections.
//! The algorithms used by this object *must* be efficient and speedy;
//! otherwise system performance will be severely degraded.
//------------------------------------------------------------------------------

extern "C" XrdOucGMap *XrdOucgetGMap(XrdOucGMapArgs);

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in. While not currently required, it is highly recommended to
//! avoid execution issues should the class definition change. Declare it as:
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdOucgetGMap,<name>);

    where <name> is a 1- to 15-character unquoted name identifying your plugin.
*/
#endif
