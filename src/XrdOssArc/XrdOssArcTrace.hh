#ifndef _XRDOSSARC_TRACE_H
#define _XRDOSSARC_TRACE_H
/******************************************************************************/
/*                                                                            */
/*                     X r d O s s A r c T r a c e . h h                      */
/*                                                                            */
/* (C) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysTrace.hh"

#define TRACE_All   0xffffffff
#define TRACE_Debug 0x00000001
#define TRACE_None  0x00000000

namespace XrdOssArcGlobals
{
extern XrdSysTrace ArcTrace;
}

#ifndef XRDOSSARC_TRACE
#define XRDOSSARC_TRACE XrdOssArcGlobals::ArcTrace.
#endif

#define TraceInfo(x,y) \
        const char *TraceEP = x;\
        const char *TraceID = y;

#define TRACE(act, x) \
   if (XRDOSSARC_TRACE What & TRACE_ ## act) \
      {SYSTRACE(XRDOSSARC_TRACE, TraceID, TraceEP, 0, x)}

#define TRACEI(act, x) \
   if (XRDOSSARC_TRACE What & TRACE_ ## act) \
      {SYSTRACE(XRDOSSARC_TRACE, TraceID, TraceEP, 0, x)}

#define TRACING(x) XRDOSSARC_TRACE What & x

#define DEBUG(x) TRACE(Debug, x)

#endif
