#ifndef __XRDSSISESSREAL_HH__
#define __XRDSSISESSREAL_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d S s i S e s s R e a l . h h                      */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>
  
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdSsi/XrdSsiResponder.hh"
#include "XrdSsi/XrdSsiService.hh"
#include "XrdSsi/XrdSsiSession.hh"
#include "XrdSsi/XrdSsiTaskReal.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdSsiServReal;

class XrdSsiSessReal : public XrdSsiSession, public XrdSsiResponder,
                       public XrdCl::ResponseHandler
{
public:

union
{XrdSsiSessReal          *nextSess;
 XrdSsiService::Resource *resource;
};

        void     HandleResponse(XrdCl::XRootDStatus *status,
                                XrdCl::AnyObject    *response);

        void     InitSession(XrdSsiServReal *servP=0, const char *sName=0);

        void     Lock() {myMutex.Lock();}

XrdSysMutex     *MutexP() {return &myMutex;}

        bool     Open(XrdSsiService::Resource *resP, const char *epURL,
                      unsigned short tOut);

        bool     ProcessRequest(XrdSsiRequest *reqP, unsigned short tOut=0);

        void     Recycle(XrdSsiTaskReal *tP);

        void     RequestFinished(      XrdSsiRequest  *reqP,
                                 const XrdSsiRespInfo &rInfo,
                                       bool cancel=false);

static  void     SetErr(XrdCl::XRootDStatus &Status,
                        int &eNum, const char **eText);

static  void     SetErr(XrdCl::XRootDStatus &Status, XrdSsiErrInfo &eInfo);

        void     UnLock() {myMutex.UnLock();}

        bool     Unprovision(bool forced=false);

                 XrdSsiSessReal(XrdSsiServReal *servP, const char *sName)
                               : XrdSsiSession(strdup(sName), 0),
                                 XrdSsiResponder(this, (void *)0)
                                 {InitSession(servP);}

                ~XrdSsiSessReal();

XrdCl::File      epFile;

private:
static int       MapErr(int xEnum);
void             RelTask(XrdSsiTaskReal *tP);
void             Shutdown(XrdCl::XRootDStatus &epStatus);

XrdSysRecMutex   myMutex;
XrdSsiServReal  *myService;
XrdSsiTaskReal  *attBase;
XrdSsiTaskReal  *freeTask;
XrdSsiTaskReal  *pendTask;
short            nextTID;
short            alocLeft;
short            numPT;
bool             stopping;
};
#endif
