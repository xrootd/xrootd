#ifndef __XRDSECMONITOR__
#define __XRDSECMONITOR__
/******************************************************************************/
/*                                                                            */
/*                      X r d S e c M o n i t o r . h h                       */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdSecMonitor
{
public:

enum WhatInfo {TokenInfo = 0};

//------------------------------------------------------------------------------
//! Include extra information in the monitoring stream to be associated with
//! the current mapped user. This object is pointed to via the XrdSecEntity
//! secMon member.
//! 
//! @param  infoT - the enum describing what information is being reported
//! @param  info  - a null terminate string with the information in cgi format
//! 
//! @return true  - Information reported.
//! @return false - Invalid infoT code or not enabled, call has been ignored.
//------------------------------------------------------------------------------

virtual bool      Report(WhatInfo infoT, const char *info) = 0;

                  XrdSecMonitor() {}
virtual          ~XrdSecMonitor() {}
};
#endif
