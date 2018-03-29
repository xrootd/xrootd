#ifndef _XRDOSS_TRACE_H
#define _XRDOSS_TRACE_H
/******************************************************************************/
/*                                                                            */
/*                        X r d O s s T r a c e . h h                         */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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

#include "XrdOuc/XrdOucTrace.hh"

// Trace flags
//
#define TRACE_ALL       0x0fff
#define TRACE_Opendir   0x0001
#define TRACE_Open      0x0002
#define TRACE_AIO       0x0004
#define TRACE_Debug     0x0800

#ifndef NODEBUG

#include "XrdSys/XrdSysHeaders.hh"

#define QTRACE(act) OssTrace.What & TRACE_ ## act

#define TRACE(act, x) \
        if (QTRACE(act)) \
           {OssTrace.Beg(epname,tident); cerr <<x; OssTrace.End();}

#define TRACEReturn(type, ecode, msg) \
               {TRACE(type, "err " <<ecode <<msg); return ecode;}

#define DEBUG(y) if (QTRACE(Debug)) \
                    {OssTrace.Beg(epname); cerr <<y; OssTrace.End();}

#define EPNAME(x) static const char *epname = x;

#else

#define DEBUG(x)
#define QTRACE(x) 0
#define TRACE(x, y)
#define TRACEReturn(type, ecode, msg) return ecode
#define EPNAME(x)

#endif
#endif
