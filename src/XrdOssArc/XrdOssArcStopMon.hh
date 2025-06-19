#ifndef _XRDOSSARC_STOPMON_HH_
#define _XRDOSSARC_STOPMON_HH_
/******************************************************************************/
/*                                                                            */
/*                      X r d O s s S t o p M o n . h h                       */
/*                                                                            */
/* (c) 2025 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "Xrd/XrdJob.hh"

#include "XrdSys/XrdSysRAtomic.hh"
#include "XrdSys/XrdSysXSLock.hh"
  
/******************************************************************************/
/*                C l a s s   X r d O s s A r c S t o p M o n                 */
/******************************************************************************/
  
class XrdOssArcStopMon : public XrdJob
{
public:

// This method is only used by the parent class. Note that only the parent
// is allowed to get an exclusive lock.
//
virtual  void DoIt() override;

// Activate stop prevention
//
         void Activate() {if (!isActive.exchange(true)) xsLock.Lock(xs_Shared);}

// Deactivate stop prevention
//
         void Deactivate() {if (isActive.exchange(false))
                               xsLock.UnLock(xs_Shared);
                           }

// Parent constructor. Caller promises that apath will remain valid for
// the whole duration of process execution.
//
         XrdOssArcStopMon(const char* apath, int chkT, bool& aOK);

// Child constructor
//
         XrdOssArcStopMon(XrdOssArcStopMon* parent) : xsLock(parent->xsLock),
                          admPath(0), admDirFD(-1), isActive(true)
                         {xsLock.Lock(xs_Shared);}

// If you try to delete a fully constructed parent instance of this class,
// it will throw an exception as it is poentially actively inuse via child
// instances and destroying the parent may lead to undefined behaviour. This
// may only be deleted when it returns false during parent construction.
//
virtual ~XrdOssArcStopMon();

// This class cannot be copied nor assigned as it may caue undefined behaviour
//
         XrdOssArcStopMon(const XrdOssArcStopMon& other) = delete;
         XrdOssArcStopMon& operator= (const XrdOssArcStopMon&) = delete;
private:

XrdSysXSLock& xsLock;
const char*   admPath;
int           admDirFD;
int           chkInterval;
RAtomic_bool  isActive;
};
#endif
