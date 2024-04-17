#ifndef _XRDFRC_TRACE_H
#define _XRDFRC_TRACE_H
/******************************************************************************/
/*                                                                            */
/*                        X r d F r c T r a c e . h h                         */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
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

#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"

#define TRACE_ALL       0xffff
#define TRACE_Debug     0x0001

#ifndef NODEBUG

#include "XrdSys/XrdSysHeaders.hh"

#define QTRACE(act) Trace.What & TRACE_ ## act

#define DEBUGR(y) if (Trace.What & TRACE_Debug) \
                  {Trace.Beg(epname, Req.User); std::cerr <<y; Trace.End();}

#define DEBUG(y) if (Trace.What & TRACE_Debug) TRACEX(y)

#define TRACE(x,y) if (Trace.What & TRACE_ ## x) TRACEX(y)

#define TRACER(x,y) if (Trace.What & TRACE_ ## x) \
                       {Trace.Beg(epname, Req.User); std::cerr <<y; Trace.End();}

#define TRACEX(y) {Trace.Beg(0,epname); std::cerr <<y; Trace.End();}
#define EPNAME(x) static const char *epname = x;

#else

#define DEBUG(y)
#define TRACE(x, y)
#define EPNAME(x)

#endif

#define VMSG(a,...) if (Config.Verbose) Say.Emsg(a,__VA_ARGS__);

#define VSAY(a,...) if (Config.Verbose) Say.Say(a,__VA_ARGS__);

namespace XrdFrc
{
extern XrdSysError  Say;
extern XrdOucTrace  Trace;
}
#endif
