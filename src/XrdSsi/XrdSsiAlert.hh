#ifndef _XRDSSIALERT_H
#define _XRDSSIALERT_H
/******************************************************************************/
/*                                                                            */
/*                        X r d S s i A l e r t . h h                         */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSsi/XrdSsiRequest.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdSsiAlert : public XrdOucEICB
{
public:

XrdSsiAlert            *next;

static XrdSsiAlert     *Alloc(XrdSsiRespInfoMsg &aMsg);

       void             Recycle();

       int              SetInfo(XrdOucErrInfo &eInfo, char* aMsg, int aLen);

static void             SetMax(int maxval) {fMax = maxval;}

// OucEICB methods
//
        void           Done(int &Result, XrdOucErrInfo *cbInfo,
                            const char *path=0);

        int            Same(unsigned long long arg1, unsigned long long arg2)
                           {return 0;}

                        XrdSsiAlert() {}
                       ~XrdSsiAlert() {}
private:

static XrdSysMutex      aMutex;
static XrdSsiAlert     *free;
static int              fNum;
static int              fMax;

static const int        fmaxDflt = 100;

XrdSsiRespInfoMsg      *theMsg;
};
#endif
