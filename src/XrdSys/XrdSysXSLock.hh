#ifndef __SYS_XSLOCK_HH__
#define __SYS_XSLOCK_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d S y s X S L o c k . h h                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cerrno>
#include "XrdSys/XrdSysPthread.hh"

// These are valid usage options
//
enum XrdSysXS_Type {xs_None = 0, xs_Shared = 1, xs_Exclusive = 2};

// This class implements the shared lock. Any number of readers are allowed
// by requesting a shared lock. Only one exclusive writer is allowed by
// requesting an exclusive lock. Up/downgrading is not supported.
//
class XrdSysXSLock
{
public:

void        Lock(const XrdSysXS_Type usage);

void      UnLock(const XrdSysXS_Type usage=xs_None);

          XrdSysXSLock() : cur_usage(xs_None), cur_count(0), exc_wait(0),
                           shr_wait(0), toggle(0), WantShr(0), WantExc(0) {}
         ~XrdSysXSLock();

private:

XrdSysXS_Type cur_usage;
int           cur_count;
int           exc_wait;
int           shr_wait;
int           toggle;

XrdSysMutex       LockContext;
XrdSysSemaphore   WantShr;
XrdSysSemaphore   WantExc;
};
#endif
