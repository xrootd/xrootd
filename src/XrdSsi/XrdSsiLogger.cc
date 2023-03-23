/******************************************************************************/
/*                                                                            */
/*                       X r d S s i L o g g e r . c c                        */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/* Produced by Andrew Hanushevsky for Stanford University under contract      */
/*            DE-AC02-76-SFO0515 with the Deprtment of Energy                 */
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

#include <fcntl.h>
#include <cstdio>
#include <cstdarg>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "XrdVersion.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSsi/XrdSsiSfsConfig.hh"
#include "XrdSsi/XrdSsiLogger.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTrace.hh"
 
/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdSsi
{
          XrdSysError          Log(0, "ssi_");
          XrdSysLogger        *Logger = 0;
          XrdSysTrace          Trace("Ssi", Logger);
          XrdSsiLogger::MCB_t *msgCB   = 0;
          XrdSsiLogger::MCB_t *msgCBCl = 0;
}

using namespace XrdSsi;

/******************************************************************************/
/*              C l i e n t   L o g g i n g   I n t e r c e p t               */
/******************************************************************************/

namespace
{
class LogMCB : public XrdCl::LogOut
{
public:

virtual void Write(const std::string &msg);

             LogMCB(XrdSsiLogger::MCB_t *pMCB) : mcbP(pMCB) {}
virtual     ~LogMCB() {}

private:
XrdSsiLogger::MCB_t *mcbP;
};

void LogMCB::Write(const std::string &msg)
{
   timeval tNow;
   const char *brak, *cBeg, *cMsg = msg.c_str();
   unsigned long tID = XrdSysThread::Num();
   int cLen = msg.size();

// Get the actual time right now
//
   gettimeofday(&tNow, 0);

// Client format: [tod][loglvl][topic] and [pid] may follow
//
   cBeg = cMsg;
   for (int i = 0; i < 4; i++)
       {if (*cMsg != '[' || !(brak = index(cMsg, ']'))) break;
        cMsg = brak+1;
       }

// Skip leading spaces now
//
   while(*cMsg == ' ') cMsg++;

// Recalculate string length
//
   cLen = cLen - (cMsg - cBeg);
   if (cLen < 0) cLen = strlen(cMsg);
   mcbP(tNow, tID, cMsg, cLen);
}
}

/******************************************************************************/
/*                                   M s g                                    */
/******************************************************************************/
  
void XrdSsiLogger::Msg(const char *pfx,  const char *txt1,
                       const char *txt2, const char *txt3)
{

// Route the message appropriately
//
   if (pfx) Log.Emsg(pfx, txt1, txt2, txt3);
      else {const char *tout[6] = {txt1, 0};
            int i = 1;
            if (txt2) {tout[i++] = " "; tout[i++] = txt2;}
            if (txt3) {tout[i++] = " "; tout[i++] = txt3;}
            tout[i] = txt3;
            Log.Say(tout[0], tout[1], tout[2], tout[3], tout[4], tout[5]);
           }
}

/******************************************************************************/
/*                                  M s g f                                   */
/******************************************************************************/

void XrdSsiLogger::Msgf(const char *pfx, const char *fmt, ...)
{
   char buffer[2048];
   va_list  args;
   va_start (args, fmt);

// Format the message
//
   vsnprintf(buffer, sizeof(buffer), fmt, args);

// Route it
//
   if (pfx) Log.Emsg(pfx, buffer);
      else  Log.Say(buffer);
}

/******************************************************************************/
/*                                  M s g v                                   */
/******************************************************************************/

void XrdSsiLogger::Msgv(const char *pfx, const char *fmt, va_list aP)
{
   char buffer[2048];

// Format the message
//
   vsnprintf(buffer, sizeof(buffer), fmt, aP);

// Route it
//
   if (pfx) Log.Emsg(pfx, buffer);
      else  Log.Say(buffer);
}

/******************************************************************************/

void XrdSsiLogger::Msgv(struct iovec *iovP, int iovN)
{
   Logger->Put(iovN, iovP);
}

/******************************************************************************/
/*                                S e t M C B                                 */
/******************************************************************************/
  
bool XrdSsiLogger::SetMCB(XrdSsiLogger::MCB_t  &mcbP,
                          XrdSsiLogger::mcbType mcbt)
{
// Record the callback, this may be on the server or the client
//
   if (mcbt == mcbAll || mcbt == mcbServer) msgCB = mcbP;

// If setting the clientside, get the client logging object and set a new
// logging intercept object that will route the messages here.
//
   if (mcbt == mcbAll || mcbt == mcbClient)
      {XrdCl::Log *logP = XrdCl::DefaultEnv::GetLog();
       if (!logP) return false;
       logP->SetOutput(new LogMCB(&mcbP));
       msgCBCl = mcbP;
      }

// All done
//
   return true;
}

/******************************************************************************/
/*                                  T B e g                                   */
/******************************************************************************/
  
const char *XrdSsiLogger::TBeg() {return Logger->traceBeg();}

/******************************************************************************/
/*                                  T E n d                                   */
/******************************************************************************/
  
void XrdSsiLogger::TEnd()
{
   std::cerr <<std::endl;
   Logger->traceEnd();
}
