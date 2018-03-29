#ifndef _XROOTD_TRACE_H
#define _XROOTD_TRACE_H
/******************************************************************************/
/*                                                                            */
/*                     X r d X r o o t d T r a c e . h h                      */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

// Trace flags
//
#define TRACE_ALL       0x0fff
#define TRACE_DEBUG     0x0001
#define TRACE_EMSG      0x0002
#define TRACE_FS        0x0004
#define TRACE_LOGIN     0x0008
#define TRACE_MEM       0x0010
#define TRACE_REQ       0x0020
#define TRACE_REDIR     0x0040
#define TRACE_RSP       0x0080
#define TRACE_SCHED     0x0100
#define TRACE_STALL     0x0200
 
#ifndef NODEBUG

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdOuc/XrdOucTrace.hh"

#define TRACE(act, x) \
   if (XrdXrootdTrace->What & TRACE_ ## act) \
      {XrdXrootdTrace->Beg(TraceID);   cerr <<x; XrdXrootdTrace->End();}

#define TRACEI(act, x) \
   if (XrdXrootdTrace->What & TRACE_ ## act) \
      {XrdXrootdTrace->Beg(TraceID,TRACELINK->ID); cerr <<x; XrdXrootdTrace->End();}

#define TRACEP(act, x) \
   if (XrdXrootdTrace->What & TRACE_ ## act) \
      {XrdXrootdTrace->Beg(TraceID,TRACELINK->ID,Response.ID()); cerr <<x; \
       XrdXrootdTrace->End();}

#define TRACES(act, x) \
   if (XrdXrootdTrace->What & TRACE_ ## act) \
      {XrdXrootdTrace->Beg(TraceID,TRACELINK->ID,(const char *)trsid); cerr <<x; \
       XrdXrootdTrace->End();}

#define TRACING(x) XrdXrootdTrace->What & x

#else

#define TRACE(act,x)
#define TRACEI(act,x)
#define TRACEP(act,x)
#define TRACES(act,x)
#define TRACING(x) 0
#endif

#endif
