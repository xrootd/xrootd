/******************************************************************************/
/*                                                                            */
/*                      X r d S s i L o g g i n g . c c                       */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdVersion.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSsi/XrdSsiLogger.hh"
#include "XrdSys/XrdSysLogPI.hh"
#include "XrdSys/XrdSysPlugin.hh"
 
/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdSsi
{
extern XrdSsiLogger::MCB_t *msgCB;
}

using namespace std;
using namespace XrdSsi;

/******************************************************************************/
/*                      L o g   P l u g i n   H o o k s                       */
/******************************************************************************/
/******************************************************************************/
/*                             C o n f i g L o g                              */
/******************************************************************************/

namespace
{
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
