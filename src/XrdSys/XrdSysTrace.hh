#ifndef __XRDSYSTRACE_HH__
#define __XRDSYSTRACE_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d S y s T r a c e . h h                         */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <iostream>
#include <sys/uio.h>

class XrdSysLogger;

#include "XrdSys/XrdSysPthread.hh"

namespace Xrd
{
enum Fmt {dec=0, hex=2, hex1=3, oct=4, oct1=5};
}

#define SYSTRACE(obj, usr, epn, txt, dbg) \
        obj Beg(usr, epn, txt) <<dbg <<obj End();

class XrdSysTrace
{
public:

XrdSysTrace& Beg(const char *usr=0, const char *epn=0, const char *txt=0);

XrdSysTrace *End() {return this;}

void         SetLogger(XrdSysLogger *logp);

typedef void (*msgCB_t)(const char *tid, const char *msg, bool dbgmsg);

void         SetLogger(msgCB_t cbP);

inline bool  Tracing(int mask) {return (mask & What) != 0;}

       int   What;

XrdSysTrace& operator<<(bool        val);

XrdSysTrace& operator<<(      char  val);
XrdSysTrace& operator<<(const char *val);
   XrdSysTrace& operator<<(const std::string& val);

XrdSysTrace& operator<<(short       val);
XrdSysTrace& operator<<(int         val);
XrdSysTrace& operator<<(long        val);
XrdSysTrace& operator<<(long long   val);

XrdSysTrace& operator<<(unsigned short     val);
XrdSysTrace& operator<<(unsigned int       val);
XrdSysTrace& operator<<(unsigned long      val);
XrdSysTrace& operator<<(unsigned long long val);

XrdSysTrace& operator<<(float       val)
                       {return Insert(static_cast<long double>(val));}
XrdSysTrace& operator<<(double      val)
                       {return Insert(static_cast<long double>(val));}
XrdSysTrace& operator<<(long double val)
                       {return Insert(val);}

XrdSysTrace& operator<<(void* val);

XrdSysTrace& operator<<(Xrd::Fmt val) {doFmt = val; return *this;}

XrdSysTrace& operator<<(XrdSysTrace *stp);

             XrdSysTrace(const char *pfx, XrdSysLogger *logp=0, int tf=0)
                        : What(tf), logP(logp), iName(pfx), dPnt(0),
                          dFree(txtMax), vPnt(1), doFmt(Xrd::dec) {}
            ~XrdSysTrace() {}

private:

XrdSysTrace& Insert(long double val);

static const int iovMax =  16;
static const int pfxMax = 256;
static const int txtMax = 256;

static const int doOne  =0x01;

XrdSysMutex      myMutex;
XrdSysLogger    *logP;
const char      *iName;
short            dPnt;
short            dFree;
short            vPnt;
Xrd::Fmt         doFmt;
struct iovec     ioVec[iovMax];
char             pBuff[pfxMax];
char             dBuff[txtMax];
};
#endif
