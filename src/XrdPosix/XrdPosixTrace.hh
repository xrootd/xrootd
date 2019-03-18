#ifndef _XRDPOSIX_TRACE_H
#define _XRDPOSIX_TRACE_H
/******************************************************************************/
/*                                                                            */
/*                      X r d P o s i x T r a c e . h h                       */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#define TRACE_Debug     0x0001

#ifndef NODEBUG

#include "XrdSys/XrdSysTrace.hh"

#define DMSG(x,y) SYSTRACE(XrdPosixGlobals::Trace. , 0, x, 0, y)

#define DEBUGON (XrdPosixGlobals::Trace.What & TRACE_Debug)

#define DEBUG(y) if (XrdPosixGlobals::Trace.What & TRACE_Debug) DMSG(epname,y)

#define EPNAME(x) static const char *epname = x

namespace XrdPosixGlobals
{
extern    XrdSysTrace Trace;
}

#else

#define DEBUG(y)
#define EPNAME(x)
#define DEBUGON false

#endif
#endif
