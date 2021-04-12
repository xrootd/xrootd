#ifndef _XRDOSSCSI_TRACE_H
#define _XRDOSSCSI_TRACE_H
/******************************************************************************/
/*                                                                            */
/*                    X r d O s s C s i T r a c e . h h                       */
/*                                                                            */
/* (C) Copyright 2020 CERN.                                                   */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* In applying this licence, CERN does not waive the privileges and           */
/* immunities granted to it by virtue of its status as an Intergovernmental   */
/* Organization or submit itself to any jurisdiction.                         */
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
#define TRACE_Warn      0x0001
#define TRACE_Info      0x0002
#define TRACE_Debug     0x0800

#ifndef NODEBUG

#include "XrdSys/XrdSysHeaders.hh"

#define QTRACE(act) OssCsiTrace.What & TRACE_ ## act

#define TRACE(act, x) \
        if (QTRACE(act)) \
           {OssCsiTrace.Beg(epname,tident); cerr <<x; OssCsiTrace.End();}

#define TRACEReturn(type, ecode, msg) \
               {TRACE(type, "err " <<ecode <<msg); return ecode;}

#define DEBUG(y) if (QTRACE(Debug)) \
                    {OssCsiTrace.Beg(epname); cerr <<y; OssCsiTrace.End();}

#define EPNAME(x) static const char *epname = x;

#else

#define DEBUG(x)
#define QTRACE(x) 0
#define TRACE(x, y)
#define TRACEReturn(type, ecode, msg) return ecode
#define EPNAME(x)

#endif
#endif
