#ifndef _XRDXR_TRACE_H
#define _XRDXR_TRACE_H
/*****************************************************************************/
/*                                                                           */
/*                        X r d X r T r a c e . h h                          */
/*                                                                           */
/* (C) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*        Produced by Heinz Stockinger for Stanford University.              */
/*                                                                           */
/*****************************************************************************/

//         $Id$

#include "XrdOuc/XrdOucTrace.hh"

#ifndef NODEBUG

#include <iostream.h>

#define QTRACE(act) XrTrace.What & TRACE_ ## act

#define TRACE(act, x) \
        if (QTRACE(act)) \
           {XrTrace.Beg(epname,tident); std::cerr <<x; XrTrace.End();}

#define TRACEReturn(type, ecode, msg) \
               {TRACE(type, "err " <<ecode <<msg); return ecode;}

#define DEBUGX(y) if (QTRACE(Debug)) \
                    {XrTrace.Beg(epname); std::cerr <<y; XrTrace.End();}

// Trace flags
//
#define TRACE_All       0x0dff
#define TRACE_Login     0x0001
#define TRACE_Auth      0x0002
#define TRACE_Open      0x0004
#define TRACE_Read      0x0010
#define TRACE_Stat      0x0014
#define TRACE_Close     TRACE_Open
#define TRACE_Logout    0x0018
#define TRACE_Wait      0x0019
#define TRACE_Debug     0x8000

#else

#define DEBUGX(x, y)
#define TRACE(x, y)
#define TRACEReturn(type, ecode, msg) return ecode

#endif
#endif
