/******************************************************************************/
/*                                                                            */
/*             X r d S s i G e t C l i e n t S e r v i c e . c c              */
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
  
#include "Xrd/XrdScheduler.hh"
#include "Xrd/XrdTrace.hh"

#include "XrdNet/XrdNetAddr.hh"

#include "XrdSsi/XrdSsiDebug.hh"
#include "XrdSsi/XrdSsiProvider.hh"
#include "XrdSsi/XrdSsiServReal.hh"

#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysError.hh"
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdSsiClientProvider : public XrdSsiProvider
{
public:

XrdSsiService *GetService(XrdSsiErrInfo &eInfo,
                          const char    *contact,
                          int            oHold=256
                         );

virtual bool   Init(XrdSsiLogger  *logP,
                    XrdSsiCluster *clsP,
                    const char    *cfgFn,
                    const char    *parms,
                    int            argc,
                    char         **argv
                   ) {return true;}

virtual rStat  QueryResource(const char *rName,
                             const char *contact=0
                            ) {return notPresent;}

virtual void   SetCBThreads(int tNum) {maxTCB = tNum;}

               XrdSsiClientProvider() : maxTCB(300) {}
virtual       ~XrdSsiClientProvider() {}

private:
void SetScheduler();

int  maxTCB;
};

/******************************************************************************/
/*      X r d S s i C l i e n t P r o v i d e r : : G e t S e r v i c e       */
/******************************************************************************/

namespace XrdSsi
{
XrdScheduler *schedP = 0;
}
  
XrdSsiService *XrdSsiClientProvider::GetService(XrdSsiErrInfo &eInfo,
                                                const char    *contact,
                                                int            oHold)
{
   XrdNetAddr netAddr;
   const char *eText;
   char buff[512];
   int  n;

// Allocate a scheduler if we do not have one (1st call)
//
  if (!XrdSsi::schedP) SetScheduler();

// If no contact is given then declare an error
//
   if (!contact || !(*contact))
      {eInfo.Set("Contact not specified.", EINVAL); return 0;}

// Validate the given contact
//
   if ((eText = netAddr.Set(contact)))
      {eInfo.Set(eText, EINVAL); return 0;}

// Construct new binding
//
   if (!(n = netAddr.Format(buff, sizeof(buff), XrdNetAddrInfo::fmtName)))
      {eInfo.Set("Unable to validate contact.", EINVAL); return 0;}

// Allocate a service object and return it
//
   return new XrdSsiServReal(buff, oHold);
}

/******************************************************************************/
/*    X r d S s i C l i e n t P r o v i d e r : : S e t S c h e d u l e r     */
/******************************************************************************/

namespace
{
XrdSysError myLog(0, "Ssi");

XrdOucTrace myTrc(&myLog);
}
  
void XrdSsiClientProvider::SetScheduler()
{
   XrdSysLogger *logP;
   int eFD;

// Get a file descriptor mirroring standard error
//
#if defined(__linux__) && defined(O_CLOEXEC)
   eFD = fcntl(STDERR_FILENO, F_DUPFD_CLOEXEC, 0);
#else
   eFD = dup(STDERR_FILENO);
   fcntl(eFD, F_SETFD, FD_CLOEXEC);
#endif

// Now we need to get a logger object. We make this a real dumb one.
//
   logP = new XrdSysLogger(eFD, 0);
   myLog.logger(logP);

// Now construct the proper trace object
//
   if (XrdSsi::DeBug.isON) myTrc.What = TRACE_SCHED;

// We can now set allocate a scheduler
//
   XrdSsi::schedP = new XrdScheduler(&myLog, &myTrc);

// Set thread count for callbacks
//
   XrdSsi::schedP->setParms(-1, maxTCB, -1, -1, 0);

// Start the scheduler
//
   XrdSsi::schedP->Start();
}

/******************************************************************************/
/*                        G l o b a l   S t a t i c s                         */
/******************************************************************************/
  
namespace
{
XrdSsiClientProvider ClientProvider;
}

XrdSsiProvider *XrdSsiProviderClient = &ClientProvider;
