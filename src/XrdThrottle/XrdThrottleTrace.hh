
#ifndef _XRDTHROTTLE_TRACE_H
#define _XRDTHROTTLE_TRACE_H

// Trace flags
//
#define TRACE_NONE      0x0000
#define TRACE_ALL       0x0fff
#define TRACE_BANDWIDTH 0x0001
#define TRACE_IOPS      0x0002
#define TRACE_IOLOAD    0x0004
#define TRACE_DEBUG     0x0008
#define TRACE_FILES     0x0010
#define TRACE_CONNS     0x0020

#ifndef NODEBUG

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdOuc/XrdOucTrace.hh"

#ifndef XRD_TRACE
#define XRD_TRACE m_trace->
#endif

#define TRACE(act, x) \
   if (XRD_TRACE What & TRACE_ ## act) \
      {XRD_TRACE Beg(TraceID);   std::cerr <<x; XRD_TRACE End();}

#define TRACEI(act, x) \
   if (XRD_TRACE What & TRACE_ ## act) \
      {XRD_TRACE Beg(TraceID,TRACELINK->ID); std::cerr <<x; \
       XRD_TRACE End();}

#define TRACING(x) XRD_TRACE What & x

#else

#define TRACE(act,x)
#define TRACEI(act,x)
#define TRACING(x) 0
#endif

#endif

