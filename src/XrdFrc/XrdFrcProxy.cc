/******************************************************************************/
/*                                                                            */
/*                        X r d F r c P r o x y . c c                         */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "errno.h"
#include <fcntl.h>
#include "stdio.h"
#include "unistd.h"
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdFrc/XrdFrcReqAgent.hh"
#include "XrdFrc/XrdFrcProxy.hh"
#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrc/XrdFrcUtils.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPlatform.hh"

using namespace XrdFrc;

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
XrdFrcProxy::o2qMap XrdFrcProxy::oqMap[] =
                               {{"getf", XrdFrcRequest::getQ, opGet},
                                {"migr", XrdFrcRequest::migQ, opMig},
                                {"pstg", XrdFrcRequest::stgQ, opStg},
                                {"putf", XrdFrcRequest::putQ, opPut}};

int                 XrdFrcProxy::oqNum = sizeof(oqMap)/sizeof(oqMap[0]);

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdFrcProxy::XrdFrcProxy(XrdSysLogger *lP, const char *iName, int Debug)
{
   char buff[256];

// Clear agent vector
//
   memset(Agent, 0, sizeof(Agent));

// Link the logger to our message facility
//
   Say.logger(lP);

// Set the debug flag
//
   if (Debug) Trace.What |= TRACE_ALL;

// Develop our internal name
//
   QPath = 0;
   insName = XrdOucUtils::InstName(iName,0);
   sprintf(buff,"%s.%d",XrdOucUtils::InstName(iName),static_cast<int>(getpid()));
   intName = strdup(buff);
}

/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
int XrdFrcProxy::Add(char Opc, const char *Lfn, const char *Opq,
                               const char *Usr, const char *Rid,
                               const char *Nop, const char *Pop, int Prty)
{
   XrdFrcRequest myReq;
   int n, Options = 0;
   int qType = XrdFrcUtils::MapR2Q(Opc, &Options);

// Verify that we can support this operation
//
   if (!Agent[qType]) return -ENOTSUP;

// Initialize the request element
//
   memset(&myReq, 0, sizeof(myReq));
   myReq.OPc = Opc;

// Insert the Lfn and Opaque information
//
   n = strlen(Lfn);
   if (Opq && *Opq)
      {if (n + strlen(Opq) + 2 > sizeof(myReq.LFN)) return -ENAMETOOLONG;
       strcpy(myReq.LFN, Lfn); strcpy(myReq.LFN+n+1, Opq), myReq.Opaque = n+1;
      } else if (n < int(sizeof(myReq.LFN))) strcpy(myReq.LFN, Lfn);
                else return -ENAMETOOLONG;

// Get the LFN offset in case this is a url
//
   if (myReq.LFN[0] != '/' && !(myReq.LFO = XrdFrcUtils::chkURL(myReq.LFN)))
      return -EILSEQ;

// Set the user, request id, notification path, and priority
//
   if (Usr && *Usr) strlcpy(myReq.User, Usr, sizeof(myReq.User));
      else strcpy(myReq.User, intName);
   if (Rid) strlcpy(myReq.ID, Rid, sizeof(myReq.ID));
      else *(myReq.ID) = '?';
   if (Nop && *Nop) strlcpy(myReq.Notify, Nop, sizeof(myReq.Notify));
      else *(myReq.Notify) = '-';
   myReq.Prty = Prty;

// Establish processing options
//
   myReq.Options = Options | XrdFrcUtils::MapM2O(myReq.Notify, Pop);

// Add this request to the queue of requests via the agent
//
   Agent[qType]->Add(myReq);
   return 0;
}

/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/
  
int XrdFrcProxy::Del(char Opc, const char *Rid)
{
   XrdFrcRequest myReq;
   int qType = XrdFrcUtils::MapR2Q(Opc);

// Verify that we can support this operation
//
   if (!Agent[qType]) return -ENOTSUP;

// Initialize the request element
//
   memset(&myReq, 0, sizeof(myReq));
   strlcpy(myReq.ID, Rid, sizeof(myReq.ID));

// Delete the request from the queue
//
   Agent[qType]->Del(myReq);
   return 0;
}

/******************************************************************************/
/*                                  L i s t                                   */
/******************************************************************************/
  
int XrdFrcProxy::List(XrdFrcProxy::Queues &State, char *Buff, int Bsz)
{
   int i;

// Get a queue type
//
do{if (!State.Active)
      while(State.QList & opAll)
           {for (i = 0; i < oqNum; i++) if (oqMap[i].oType & State.QList) break;
            if (i >= oqNum) return 0;
            State.QNow   =  oqMap[i].qType;
            State.QList &= ~oqMap[i].oType;
            if (!Agent[int(State.QNow)]) continue;
            State.Active = 1;
            break;
           }

   for (i = State.Prty; i <= XrdFrcRequest::maxPrty; i++)
       if (Agent[int(State.QNow)]->NextLFN(Buff,Bsz,i,State.Offset)) return 1;
          else State.Prty = i+1;

   State.Active = 0; State.Offset = 0; State.Prty = 0;
  } while(State.QList & opAll);

// We've completed returning all info
//
   return 0;
}

/******************************************************************************/
  
int XrdFrcProxy::List(int qType, int qPrty, XrdFrcRequest::Item *Items, int Num)
{
   int i, n, Cnt = 0;

// List each queue
//
   while(qType & opAll)
        {for (i = 0; i < oqNum; i++) if (oqMap[i].oType & qType) break;
         if (i >= oqNum) return Cnt;
         qType &= ~oqMap[i].oType; n = oqMap[i].qType;
         if (!Agent[n]) continue;
         if (qPrty < 0) Cnt += Agent[n]->List(Items, Num);
            else Cnt += Agent[n]->List(Items, Num, qPrty);
        }

// All done
//
   return Cnt;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

int XrdFrcProxy::Init(int opX, const char *aPath, int aMode, const char *qPath)
{
   const char *configFN = getenv("XRDCONFIGFN"), *iName = 0;
   int i;

// If a qPath was specified, and the "Queues" component will be added later.
// Otherwise, we check the config file to see if there is a qpath there.
// If not we use the aPath which must be unqualified with a component name
// which we will add here). All paths must have the instance name if so needed.
//
        if (qPath) QPath = strdup(qPath);
   else if (!configFN) iName = insName;
   else if (Init2(configFN)) return 0;

// Create the queue path directory if it does not exists
//
   if (!QPath && !(QPath = XrdFrcUtils::makePath(iName, aPath, aMode)))
      return 0;

// Now create and start an agent for each wanted service
//
   for (i = 0; i < oqNum; i++)
       if (opX & oqMap[i].oType)
          {Agent[oqMap[i].qType]
                = new XrdFrcReqAgent(oqMap[i].qName, oqMap[i].qType);
           if (!Agent[oqMap[i].qType]->Start(QPath, aMode)) return 0;
          }

// All done
//
   return 1;
}

/******************************************************************************/
/* Private:                        I n i t 2                                  */
/******************************************************************************/

int XrdFrcProxy::Init2(const char *ConfigFN)
{
  char *var;
  int  cfgFD, retc, NoGo = 0;
  XrdOucEnv myEnv;
  XrdOucStream cfgFile(&Say, getenv("XRDINSTANCE"), &myEnv, "=====> ");

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {Say.Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   cfgFile.Attach(cfgFD);
   static const char *cvec[] = { "*** frm client plugin config:", 0 };
   cfgFile.Capture(cvec);

// Now start reading records until eof looking for our directive
//
   while((var = cfgFile.GetMyFirstWord()))
        {if (!strcmp(var, "frm.xfr.qcheck") &&  qChk(cfgFile))
            {cfgFile.Echo(); NoGo = 1;}
        }

// Now check if any errors occurred during file i/o
//
   if ((retc = cfgFile.LastError()))
      NoGo = Say.Emsg("Config", retc, "read config file", ConfigFN);
   cfgFile.Close();

// All done
//
   return NoGo;
}

/******************************************************************************/
/* Private:                         q C h k                                   */
/******************************************************************************/
  
int XrdFrcProxy::qChk(XrdOucStream &cfgFile)
{
    char *val;

// Get the next token, we must have one here
//
   if (!(val = cfgFile.GetWord()))
      {Say.Emsg("Config", "qcheck time not specified"); return 1;}

// If not a path, then it must be a time
//
   if (*val != '/' && !(val = cfgFile.GetWord())) return 0;

// The next token has to be an absolute path if it is present at all
//
   if (*val != '/')
      {Say.Emsg("Config", "qcheck path not absolute"); return 1;}
   if (QPath) free(QPath);
   QPath = strdup(val);
   return 0;
}
