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
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdVersion.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSsi/XrdSsiSfsConfig.hh"
#include "XrdSsi/XrdSsiLogger.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysLogPI.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdSys/XrdSysPthread.hh"
 
/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdSsi
{
          XrdSysError     Log(0);
          XrdSysLogger   *Logger;
          XrdOucTrace     Trace(&Log);
}

using namespace XrdSsi;

/******************************************************************************/
/*                      L o g   P l u g i n   H o o k s                       */
/******************************************************************************/
/******************************************************************************/
/*                             C o n f i g L o g                              */
/******************************************************************************/

namespace
{
XrdSsiLogger::MCB_t   *msgCB = 0;

void ConfigLog(const char *cFN)
{
   XrdVERSIONINFODEF(myVersion, ssi, XrdVNUMBER, XrdVERSION);
   const char *lName;
   char eBuff[2048], *var, *val, **lDest, *logPath = 0, *svcPath = 0;
   XrdSysPlugin *myLib;
   XrdSsiLogger::MCB_t **theCB;
   XrdOucEnv myEnv;
   XrdOucStream cStrm(0, getenv("XRDINSTANCE"), &myEnv, "=====> ");
   int  cfgFD, retc, NoGo = 0;

// Try to open the configuration file.
//
   if ((cfgFD = open(cFN, O_RDONLY, 0)) < 0)
      {cerr <<"Config " <<strerror(errno) <<" opening " <<cFN <<endl;
       return;
      }
   cStrm.Attach(cfgFD);

// Now start reading records until eof.
//
   while((var = cStrm.GetMyFirstWord()))
        {     if (!strcmp(var, "ssi.loglib")) {lDest = &logPath; lName = "log";}
         else if (!strcmp(var, "ssi.svclib")) {lDest = &svcPath; lName = "svc";}
         else continue;
         if (!(val = cStrm.GetWord()) || !val[0])
            {cerr <<"Config "<<lName<<"lib path not specified."<<endl; NoGo=1;}
            else {if (*lDest) free(*lDest);
                  *lDest = strdup(val);
                 }
        }

// Now check if any errors occured during file i/o
//
   if ((retc = cStrm.LastError()))
      {cerr <<"Config " <<strerror(-retc) <<" reading " <<cFN <<endl;
       NoGo = 1;
      }
   cStrm.Close();

// If we don't have a loglib then revert to using svclib
//
   if (!logPath) {logPath = svcPath; svcPath = 0; lName = "svclib";}
      else lName = "loglib";

// Check if we have a logPath (we must)
//
   if (!NoGo && !logPath)
      {cerr <<"Config neither ssi.loglib nor ssi.svclib directive specified in "
            <<cFN <<endl;
       return;
      }

// Create a plugin object
//
   if (!(myLib = new XrdSysPlugin(eBuff, sizeof(eBuff), logPath, lName,
                                  &myVersion)))
      {cerr <<"Config " <<eBuff <<endl;
       return;
      }

// Now get the entry point of the message callback function if the dynamic
// initialization of the plugin library hasn't already set it.
//
   if (!msgCB)
      {theCB = (XrdSsiLogger::MCB_t **)(myLib->getPlugin("XrdSsiLoggerMCB"));
       if (!msgCB && !theCB) cerr <<"Config " <<eBuff <<endl;
          else {if (!msgCB) msgCB = *theCB;
                myLib->Persist();
               }
      }
      else myLib->Persist();

// All done
//
   delete myLib;
}

/******************************************************************************/
/*              C l i e n t   L o g g i n g   I n t e r c e p t               */
/******************************************************************************/

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
/*                        X r d S y s L o g P I n i t                         */
/******************************************************************************/
 
extern "C"
{
XrdSysLogPI_t  XrdSysLogPInit(const char *cfgfn, char **argv, int argc)
          {if (cfgfn && *cfgfn) ConfigLog(cfgfn);
           if (!msgCB)
              cerr <<"Config '-l@' requires a logmsg callback function "
                   <<"but it was found!" <<endl;
           return msgCB;
          }
}

XrdVERSIONINFO(XrdSysLogPInit,XrdSsiLPI);

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
   cerr <<endl;
   Logger->traceEnd();
}
