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

#ifndef NODEBUG

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdOuc/XrdOucTrace.hh"

#ifndef XRD_TRACE
#define XRD_TRACE GetTrace()->
#endif

#define TRACE(act, x) \
   if (XRD_TRACE What >= TRACE_ ## act) \
      {XRD_TRACE Beg(m_traceID);   cerr <<x; XRD_TRACE End();}

#define TRACEIO(act, x) \
   if (XRD_TRACE What >= TRACE_ ## act) \
   {XRD_TRACE Beg(m_traceID);   cerr <<x << " " <<  m_io->Path(); XRD_TRACE End();}

#define TRACEF(act, x) \
   if (XRD_TRACE What >= TRACE_ ## act) \
   {XRD_TRACE Beg(m_traceID);   cerr <<x << " " <<  GetLocalPath(); XRD_TRACE End();}




#else

#define TRACE(act,x)
#endif

#endif
