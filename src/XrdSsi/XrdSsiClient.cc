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

#include <cerrno>
#include <fcntl.h>
#include <cstdio>
#include <string>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/types.h>
  
#include "Xrd/XrdScheduler.hh"
#include "Xrd/XrdTrace.hh"

#include "XrdCl/XrdClDefaultEnv.hh"

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetRegistry.hh"

#include "XrdSsi/XrdSsiAtomics.hh"
#include "XrdSsi/XrdSsiLogger.hh"
#include "XrdSsi/XrdSsiProvider.hh"
#include "XrdSsi/XrdSsiServReal.hh"
#include "XrdSsi/XrdSsiScale.hh"
#include "XrdSsi/XrdSsiTrace.hh"

#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysLogging.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTrace.hh"
  
/******************************************************************************/
/*                    N a m e   S p a c e   G l o b a l s                     */
/******************************************************************************/

namespace XrdSsi
{
extern XrdSysError          Log;
extern XrdSysLogger        *Logger;
extern XrdSsiScale          sidScale;
extern XrdSysTrace          Trace;
extern XrdSsiLogger::MCB_t *msgCB;
extern XrdSsiLogger::MCB_t *msgCBCl;

       XrdSysMutex   clMutex;
       XrdScheduler *schedP   = 0;
       XrdCl::Env   *clEnvP   = 0;
       Atomic(int)   contactN(1);
       short         maxTCB   = 300;
       short         maxCLW   =  30;
       short         maxPEL   =   3;
       Atomic(bool)  initDone(false);
       bool          dsTTLSet = false;
       bool          reqTOSet = false;
       bool          strTOSet = false;
       bool          hiResTime= false;

static const int rDispNone =  0;
static const int rDispRand = -1;
static const int rDispRR   =  1;

       char          rDisp    = rDispRR;
}

using namespace XrdSsi;
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdSsiClientProvider : public XrdSsiProvider
{
public:

XrdSsiService *GetService(XrdSsiErrInfo     &eInfo,
                          const std::string &contact,
                          int                oHold=256
                         );

virtual bool   Init(XrdSsiLogger  *logP,
                    XrdSsiCluster *clsP,
                    std::string    cfgFn,
                    std::string    parms,
                    int            argc,
                    char         **argv
                   ) {return true;}

virtual rStat  QueryResource(const char *rName,
                             const char *contact=0
                            ) {return notPresent;}

virtual void   SetCBThreads(int cbNum, int ntNum);

virtual bool   SetConfig(XrdSsiErrInfo &eInfo,
                         std::string   &optname, int optvalue);

virtual void   SetSpread(short ssz);

virtual void   SetTimeout(tmoType what, int tmoval);

               XrdSsiClientProvider() {}
virtual       ~XrdSsiClientProvider() {}

private:
void SetLogger();
void SetScheduler();
};

/******************************************************************************/
/*      X r d S s i C l i e n t P r o v i d e r : : G e t S e r v i c e       */
/******************************************************************************/
  
XrdSsiService *XrdSsiClientProvider::GetService(XrdSsiErrInfo     &eInfo,
                                                const std::string &contact,
                                                int                oHold)
{
   static const int maxTMO = 0x7fffffff;
   XrdNetAddr netAddr;
   std::string eMsg;
   const char *eText = 0;
   char buff[512];

// Allocate a scheduler if we do not have one and set default env (1st call)
//
  if (!Atomic_GET(initDone))
     {clMutex.Lock();
      if (!Logger)   SetLogger();
      if (!schedP)   SetScheduler();
      if (!clEnvP)   clEnvP = XrdCl::DefaultEnv::GetEnv();
      if (!dsTTLSet) clEnvP->PutInt("DataServerTTL",  maxTMO);
      if (!reqTOSet) clEnvP->PutInt("RequestTimeout", maxTMO);
      if (!strTOSet) clEnvP->PutInt("StreamTimeout",  maxTMO);
      clEnvP->PutInt("ParallelEvtLoop",maxPEL);
      if (rDisp == rDispNone || rDisp == rDispRR)
         clEnvP->PutInt("IPNoShuffle", 1);
      initDone = true;
      clMutex.UnLock();
     }

// If no contact is given then declare an error
//
   if (contact.empty())
      {eInfo.Set("Contact not specified.", EINVAL); return 0;}

// If this is a single contact which is really a singleton, don't create
// a registry entry for it as it's just not needed (we need one to rotate).
//
   if (contact.find(',') != std::string::npos
   ||  !XrdNetUtils::Singleton(contact.c_str()))
      {int cNum = contactN++;
       bool rotate = rDisp == rDispRR;
       snprintf(buff,sizeof(buff),"%ccontact-%d:4901",XrdNetRegistry::pfx,cNum);
       if (!XrdNetRegistry::Register(buff, contact.c_str(), &eMsg, rotate))
          eText = (eMsg.size() ? eMsg.c_str() : "reason unknown");

      } else {

       if (!(eText = netAddr.Set(contact.c_str()))
       &&  !netAddr.Format(buff, sizeof(buff), XrdNetAddrInfo::fmtName))
          eText = "formatting failed";
      }

// Check if validation or registration failed
//
   if (eText)
      {snprintf(buff, sizeof(buff), "Unable to validate contact; %s", eText);
       eInfo.Set(buff, EINVAL);
       return 0;
      }

// Allocate a service object and return it
//
   return new XrdSsiServReal(buff, oHold);
}

/******************************************************************************/
/*    X r d S s i C l i e n t P r o v i d e r : : S e t C B T h r e a d s     */
/******************************************************************************/

void XrdSsiClientProvider::SetCBThreads(int cbNum, int ntNum)
{
// Validate thread number
//
   if (cbNum > 1)
      {if (cbNum > 32767) cbNum = 32767; // Max short value
       if (ntNum < 1) ntNum = cbNum*10/100;
       if (ntNum < 3) ntNum = 0;
          else if (ntNum > 100) ntNum = 100;
       clMutex.Lock();
       maxTCB = static_cast<short>(cbNum);
       maxCLW = static_cast<short>(ntNum);
       clMutex.UnLock();
      }
}
 
/******************************************************************************/
/*       X r d S s i C l i e n t P r o v i d e r : : S e t C o n f i g        */
/******************************************************************************/
  
bool XrdSsiClientProvider::SetConfig(XrdSsiErrInfo &eInfo,
                                     std::string   &optname, int optvalue)
{
// Look for an option we recognize
//
        if (optname == "cbThreads")
           {if (optvalue < 1)
               {eInfo.Set("invalid cbThreads value.", EINVAL); return false;}
            if (optvalue > 32767) optvalue = 32767;
            clMutex.Lock();
            maxTCB = static_cast<short>(optvalue);
            clMutex.UnLock();
           }
   else if (optname == "hiResTime") hiResTime = true;
   else if (optname == "netThreads")
           {if (optvalue < 1)
               {eInfo.Set("invalid netThreads value.", EINVAL); return false;}
            if (optvalue > 32767) optvalue = 32767;
            clMutex.Lock();
            maxCLW = static_cast<short>(optvalue);
            clMutex.UnLock();
           }
   else if (optname == "pollers")
           {if (optvalue < 1)
               {eInfo.Set("invalid pollers value.", EINVAL); return false;}
            if (optvalue > 32767) optvalue = 32767;
            clMutex.Lock();
            maxPEL =  static_cast<short>(optvalue);
            clMutex.UnLock();
           }
   else if (optname == "reqDispatch")
           {clMutex.Lock();
            if (optvalue < 0) rDisp = rDispRand;
               else if (optvalue > 0) rDisp = rDispRR;
                       else rDisp = rDispNone;
            clMutex.UnLock();
           }
   else {eInfo.Set("invalid option name.", EINVAL); return false;}

   return true;
}

/******************************************************************************/
/*       X r d S s i C l i e n t P r o v i d e r : : S e t L o g g e r        */
/******************************************************************************/
  
void XrdSsiClientProvider::SetLogger()
{
   int eFD;

// Get a file descriptor mirroring standard error
//
#if ( defined(__linux__) || defined(__GNU__) ) && defined(F_DUPFD_CLOEXEC)
   eFD = fcntl(STDERR_FILENO, F_DUPFD_CLOEXEC, 0);
#else
   eFD = dup(STDERR_FILENO);
   fcntl(eFD, F_SETFD, FD_CLOEXEC);
#endif

// Now we need to get a logger object. We make this a real dumb one.
//
   Logger = new XrdSysLogger(eFD, 0);
   if (hiResTime || getenv("XRDSSI_HIRESLOG")) Logger->setHiRes();
   Log.logger(Logger);
   Trace.SetLogger(Logger);
   if (getenv("XRDSSIDEBUG") != 0) Trace.What = TRACESSI_Debug;

// Check for a message callback object. This must be set at global init time.
//
   if (msgCBCl)
      {XrdSysLogging::Parms logParms;
       msgCB = msgCBCl;
       logParms.logpi = msgCBCl;
       logParms.bufsz = 0;
       XrdSysLogging::Configure(*Logger, logParms);
      }
}
  
/******************************************************************************/
/*    X r d S s i C l i e n t P r o v i d e r : : S e t S c h e d u l e r     */
/******************************************************************************/

// This may not be called before the logger object is created!
  
void XrdSsiClientProvider::SetScheduler()
{
   static XrdSysTrace myTrc("XrdSsi", Log.logger());

// Now construct the proper trace object (note that we do not set tracing if
// message forwarding is on because these messages will not be forwarded).
//
   if (!msgCBCl && Trace.What & TRACESSI_Debug) myTrc.What = TRACE_SCHED;

// We can now set allocate a scheduler
//
   XrdSsi::schedP = new XrdScheduler(&Log, &myTrc);

// Set thread count for callbacks
//
   XrdSsi::schedP->setParms(-1, maxTCB, -1, -1, 0);

// Set number of framework worker hreads if need be
//
   if (maxCLW)
      {if (!XrdSsi::clEnvP) clEnvP = XrdCl::DefaultEnv::GetEnv();
       clEnvP->PutInt("WorkerThreads", maxCLW);
      }

// Start the scheduler
//
   XrdSsi::schedP->Start();
}

/******************************************************************************/
/*       X r d S s i C l i e n t P r o v i d e r : : S e t S p r e a d        */
/******************************************************************************/

void XrdSsiClientProvider::SetSpread(short ssz)
{
    sidScale.setSpread(ssz);
}

/******************************************************************************/
/*      X r d S s i C l i e n t P r o v i d e r : : S e t T i m e o u t       */
/******************************************************************************/

void XrdSsiClientProvider::SetTimeout(XrdSsiProvider::tmoType what, int tmoval)
{

// Ignore invalid timeouts
//
   if (tmoval <= 0) return;

// Get global environment
//
  clMutex.Lock();
  if (!XrdSsi::clEnvP) clEnvP = XrdCl::DefaultEnv::GetEnv();

// Set requested timeout
//
   switch(what)
         {case connect_N:  clEnvP->PutInt("ConnectionRetry",  tmoval);
                           break;
          case connect_T:  clEnvP->PutInt("ConnectionWindow", tmoval);
                           break;
          case idleClose:  clEnvP->PutInt("DataServerTTL",    tmoval);
                           dsTTLSet = true;
                           break;
          case request_T:  clEnvP->PutInt("RequestTimeout",   tmoval);
                           reqTOSet = true;
                           break;
          case stream_T:   clEnvP->PutInt("StreamTimeout",    tmoval);
                           strTOSet = true;
                           break;
          default:         break;
         }

// All done
//
   clMutex.UnLock();
}
  
/******************************************************************************/
/*                        G l o b a l   S t a t i c s                         */
/******************************************************************************/
  
namespace
{
XrdSsiClientProvider ClientProvider;
}

XrdSsiProvider *XrdSsiProviderClient = &ClientProvider;
