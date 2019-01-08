#ifndef _XRDFILECACHE_TRACE_H
#define _XRDFILECACHE_TRACE_H

// Trace flags
//
#define TRACE_None     0
#define TRACE_Error    1
#define TRACE_Warning  2
#define TRACE_Info     3
#define TRACE_Debug    4
#define TRACE_Dump     5

#define TRACE_STR_None     ""
#define TRACE_STR_Error    "error "
#define TRACE_STR_Warning  "warning "
#define TRACE_STR_Info     "info "
#define TRACE_STR_Debug    "debug "
#define TRACE_STR_Dump     "dump "

#define TRACE_STR_0        ""
#define TRACE_STR_1        "error "
#define TRACE_STR_2        "warning "
#define TRACE_STR_3        "info "
#define TRACE_STR_4        "debug "
#define TRACE_STR_5        "dump "

#ifndef NODEBUG

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysTrace.hh"

#ifndef XRD_TRACE
#define XRD_TRACE GetTrace()->
#endif

#define ERRNO_AND_ERRSTR ", errno=" << errno << ", err=" << strerror(errno)

#define TRACE(act, x) \
   if (XRD_TRACE What >= TRACE_ ## act) \
   {XRD_TRACE Beg(0, m_traceID) << TRACE_STR_ ## act  << x; XRD_TRACE End(); }

#define TRACE_INT(act, x) \
   if (XRD_TRACE What >= act) \
   {static const char* t_what[]={"","error ","warning ","info ","debug ","dump "};\
    XRD_TRACE Beg(0, m_traceID) << t_what[act] << x; XRD_TRACE End(); }

#define TRACE_TEST(act, x) \
   XRD_TRACE Beg("", m_traceID) << TRACE_STR_ ## act  << x; XRD_TRACE End(); 

#define TRACE_PC(act, pre_code, x) \
   if (XRD_TRACE What >= TRACE_ ## act) \
   {pre_code; XRD_TRACE Beg(0, m_traceID) << TRACE_STR_ ## act  <<x; XRD_TRACE End(); }

#define TRACEIO(act, x) \
   if (XRD_TRACE What >= TRACE_ ## act) \
   {XRD_TRACE Beg(0, m_traceID) << TRACE_STR_ ## act <<x << " " <<  GetPath(); XRD_TRACE End(); }

#define TRACEF(act, x) \
   if (XRD_TRACE What >= TRACE_ ## act) \
   {XRD_TRACE Beg(0, m_traceID) << TRACE_STR_ ## act << x << " " <<  GetLocalPath(); XRD_TRACE End(); }

#else

#define ERRNO_AND_ERRSTR
#define TRACE(act,x)
#define TRACE_PC(act, pre_code, x)
#define TRACEIO(act, x)
#define TRACEF(act, x)

#endif

#endif

