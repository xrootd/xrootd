
/** @file  XrdHttpTrace.hh
 * @brief  Trace definitions
 * @author Fabrizio Furano
 * @date   Nov 2012
 * 
 * 
 * 
 */


#ifndef _XROOTD_TRACE_H
#define _XROOTD_TRACE_H


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


extern XrdOucTrace *XrdHttpTrace;
extern const char *XrdHttpTraceID;

#define TRACE(act, x) \
   if (XrdHttpTrace->What & TRACE_ ## act) \
      {XrdHttpTrace->Beg(XrdHttpTraceID);   cerr <<x; XrdHttpTrace->End();}

#define TRACEI(act, x) \
   if (XrdHttpTrace->What & TRACE_ ## act) \
      {XrdHttpTrace->Beg(XrdHttpTraceID,TRACELINK->ID); cerr <<x; XrdHttpTrace->End();}

#define TRACEP(act, x) \
   if (XrdHttpTrace->What & TRACE_ ## act) \
      {XrdHttpTrace->Beg(XrdHttpTraceID,TRACELINK->ID,Response.ID()); cerr <<x; \
       XrdHttpTrace->End();}

#define TRACES(act, x) \
   if (XrdHttpTrace->What & TRACE_ ## act) \
      {XrdHttpTrace->Beg(XrdHttpTraceID,TRACELINK->ID,(const char *)trsid); cerr <<x; \
       XrdHttpTrace->End();}

#define TRACING(x) XrdHttpTrace->What & x

#else

#define TRACE(act,x)
#define TRACEI(act,x)
#define TRACEP(act,x)
#define TRACES(act,x)
#define TRACING(x) 0
#endif

#endif
