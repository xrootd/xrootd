#ifndef _XROOTD_TRACE_H
#define _XROOTD_TRACE_H
/******************************************************************************/
/*                                                                            */
/*                        x r o o t d _ T r a c e . h                         */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//        $Id$
 
#ifndef NODEBUG

#include <iostream.h>
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

#else

#define TRACE(act,x)
#define TRACEI(act,x)
#define TRACEP(act,x)
#define TRACES(act,x)
#define TRACING(x)
#endif

#endif
