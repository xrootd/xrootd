#ifndef __XRDMONROLL__
#define __XRDMONROLL__
/******************************************************************************/
/*                                                                            */
/*                         X r d M o n R o l l . h h                          */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSys/XrdSysRAtomic.hh"

//-----------------------------------------------------------------------------
//! XrdMonRoll
//!
//! This class is used to register counter sets to be included in the summary
//! statistics that out reported as defined by the xrootd.report directive.
//! A pointer to an instance of this object is placed in the Xrd environment
//! with a key of "XrdMonRoll*".
//-----------------------------------------------------------------------------

class XrdMonitor;

class XrdMonRoll
{
public:

//-----------------------------------------------------------------------------
//! The setMember structure is used to define a set of counters that will be
//! registered with this class. These counters are report in the summary.
//!
//! @param varName - Is the name of the variable and becomes the xml tag or
//!                  of JSON key for the reported value.
//! @param varValu - Is the reference to the associated counter variable
//!                  holding the value.
//-----------------------------------------------------------------------------

struct setMember {const char* varName; RAtomic_uint& varValu;}; 

//-----------------------------------------------------------------------------
//! Register a set of counters to be reported.
//!
//! @param setType - Is the type of set being defined:
//!                  Misc     - counters for miscellaneous activities.
//!                  Protocol - counters for a protocol.
//! @param setName - Is the name of the set of counter variables. The name
//!                  must not already be registered. The name is reported in
//!                  stats xml tag or JSON stats key value.
//! @param setVec  - Is a vector of setMember items that define the set of 
//!                  variables for the summary report. The last element of
//!                  the vector must be initialized to {0, EOV}. The vector
//!                  must reside in allocated storage until execution ends.
//!
//! @return true when the set has been registered and false if the set is
//!         already registered (i.e. setName is in use).
//-----------------------------------------------------------------------------

static RAtomic_uint EOV; // Variable at the end of the setVec.

enum rollType {Misc, Protocol};

bool Register(rollType setType, const char* setName, setMember setVec[]);

             XrdMonRoll(XrdMonitor& xMon);
            ~XrdMonRoll() {}

private:
XrdMonitor& xrdMon;
void*       rsvd[3];
};
#endif
