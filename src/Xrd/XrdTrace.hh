#ifndef _XRD_TRACE_H
#define _XRD_TRACE_H
/******************************************************************************/
/*                                                                            */
/*                           x r d _ T r a c e . h                            */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//          $Id$

#ifndef NODEBUG

#include <iostream.h>
#include "XrdOuc/XrdOucTrace.hh"

#ifndef XRD_TRACE
#define XRD_TRACE XrdTrace.
#endif

#define TRACE(act, x) \
   if (XRD_TRACE What & TRACE_ ## act) \
      {XRD_TRACE Beg(TraceID);   cerr <<x; XRD_TRACE End();}

#define TRACEI(act, x) \
   if (XRD_TRACE What & TRACE_ ## act) \
      {XRD_TRACE Beg(TraceID,TRACELINK->ID); cerr <<x; \
       XRD_TRACE End();}

#define TRACING(x) XRD_TRACE What & x

// Trace flags
//
#define TRACE_NONE      0x0000
#define TRACE_ALL       0x0fff
#define TRACE_DEBUG     0x0001
#define TRACE_CONN      0x0002
#define TRACE_MEM       0x0004
#define TRACE_NET       0x0008
#define TRACE_POLL      0x0010
#define TRACE_PROT      0x0020
#define TRACE_SCHED     0x0040

#else

#define TRACE(act,x)
#define TRACEI(act,x)
#define TRACING(x)
#endif

#endif
