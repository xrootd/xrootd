/******************************************************************************/
/*                                                                            */
/*                     X r d F r m X f r Q u e u e . c c                      */
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

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdFrc/XrdFrcReqFile.hh"
#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrm/XrdFrmConfig.hh"
#include "XrdFrm/XrdFrmXfrJob.hh"
#include "XrdFrm/XrdFrmXfrQueue.hh"
#include "XrdNet/XrdNetMsg.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysPlatform.hh"

using namespace XrdFrc;
using namespace XrdFrm;
  
/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/

XrdSysMutex               XrdFrmXfrQueue::hMutex;
XrdOucHash<XrdFrmXfrJob>  XrdFrmXfrQueue::hTab;
  
XrdSysMutex               XrdFrmXfrQueue::qMutex;
XrdSysSemaphore           XrdFrmXfrQueue::qReady(0);

XrdFrmXfrQueue::theQueue  XrdFrmXfrQueue::xfrQ[XrdFrcRequest::numQ];

/******************************************************************************/
/* Public:                           A d d                                    */
/******************************************************************************/
  
int XrdFrmXfrQueue::Add(XrdFrcRequest *rP, XrdFrcReqFile *reqFQ, int qNum)
{
   XrdFrmXfrJob *xP;
   struct stat buf;
   const char *xfrType = xfrName(*rP, qNum);
   char *Lfn, lclpath[MAXPATHLEN];
   int Outgoing = (qNum & XrdFrcRequest::outQ);

// Validate queue number
//
   if (qNum < 0 || qNum >= XrdFrcRequest::numQ-1)
      {sprintf(lclpath, "%d", qNum);
       Say.Emsg("Queue", lclpath, " is an invalid queue; skipping", rP->LFN);
       if (reqFQ) reqFQ->Del(rP);
       return 0;
      }

// First check if this request is active or pending. If it's an inbound request
// then only the lfn matters regardless of source. For outgoing requests then
// the lfn plus the target only matters.
//
   Lfn = (Outgoing ? rP->LFN : (rP->LFN)+rP->LFO);
   hMutex.Lock();
   if ((xP = hTab.Find(Lfn)))
      {if (rP->Options & (XrdFrcRequest::msgSucc | XrdFrcRequest::msgFail)
       &&  strcmp(xP->reqData.Notify, rP->Notify))
          {XrdOucTList *tP = new XrdOucTList(rP->Notify, 0, xP->NoteList);
           xP->NoteList = tP;
          }
       hMutex.UnLock();
       if (Config.Verbose || Trace.What & TRACE_Debug)
          {sprintf(lclpath, " in progress; %s skipped for ", xfrType);
           Say.Say(0, xP->Type, xP->reqData.LFN, lclpath, rP->User);
          }
       if (reqFQ) reqFQ->Del(rP);
       return 0;
      }
   hMutex.UnLock();

// Obtain the local name
//
   if (!Config.LocalPath((rP->LFN)+rP->LFO, lclpath, sizeof(lclpath)-16))
      {if (reqFQ) reqFQ->Del(rP);
       return Notify(rP, qNum, 1, "Unable to generate pfn");
      }

// Check if the file exists or not. For incomming requests, the file must not
// exist. For outgoing requests the file must exist.
//
   if (Config.Stat((rP->LFN)+rP->LFO, lclpath, &buf))
      {if (Outgoing)
          {if (Config.Verbose || Trace.What & TRACE_Debug)
              Say.Say(0, xfrType,"skipped; ",lclpath," not resident.");
           if (reqFQ) reqFQ->Del(rP);
           return Notify(rP, qNum, 2, "file not resident");
          }
      } else {
       if (!Outgoing)
          {if (Config.Verbose || Trace.What & TRACE_Debug)
              Say.Say(0, xfrType, "skipped; ", lclpath, " exists.");
           if (reqFQ) reqFQ->Del(rP);
           return Notify(rP, qNum, 0);
          }
      }

// Obtain a queue slot, we may block until one is available
//
   do {qMutex.Lock();
       if ((xP = xfrQ[qNum].Free)) break;
       qMutex.UnLock();
       xfrQ[qNum].Avail.Wait();
      } while(!xP);
   xfrQ[qNum].Free = xP->Next;
   qMutex.UnLock();

// Initialize the slot
//
   xP->Next     = 0;
   xP->NoteList = 0;
   xP->reqFQ    = reqFQ;
   xP->reqData  = *rP;
   xP->reqFile  = (Outgoing ? xP->reqData.LFN : (xP->reqData.LFN)+rP->LFO);
   strcpy(xP->PFN, lclpath);
   xP->pfnEnd   = strlen(lclpath);
   xP->RetCode  = 0;
   xP->qNum     = qNum;
   xP->Act      =*xfrType;
   xP->Type     = xfrType+1;

// Add this to the table of requests
//
   hMutex.Lock();
   hTab.Add(xP->reqFile, xP, 0, Hash_keep);
   hMutex.UnLock();

// Place request in the appropriate transfer queue
//
   qMutex.Lock();
   if (xfrQ[qNum].Last) {xfrQ[qNum].Last->Next = xP; xfrQ[qNum].Last = xP;}
      else               xfrQ[qNum].Last       = xfrQ[qNum].First    = xP;
   qMutex.UnLock();
   qReady.Post();

// All done
//
   return 1;
}

/******************************************************************************/
/* Public:                          D o n e                                   */
/******************************************************************************/

void XrdFrmXfrQueue::Done(XrdFrmXfrJob *xP, const char *Msg)
{
   XrdOucTList *tP;

// Send notifications to everyone that wants it that this job is done
//
   do {Notify(&(xP->reqData), xP->qNum, xP->RetCode, Msg);
       if ((tP = xP->NoteList))
          {strcpy(xP->reqData.Notify, tP->text);
           xP->NoteList = tP->next;
           delete tP;
          }
      } while(tP);

// Remove this job from the queue file
//
   if (xP->reqFQ) xP->reqFQ->Del(&(xP->reqData));

// Remove this job from the active table
//
   hMutex.Lock(); hTab.Del(xP->reqFile); hMutex.UnLock();
  
// Place job element on the free queue
//
   qMutex.Lock();
   xP->Next = xfrQ[xP->qNum].Free;
   xfrQ[xP->qNum].Free = xP;
   xfrQ[xP->qNum].Avail.Post();
   qMutex.UnLock();
}

/******************************************************************************/
/* Public:                           G e t                                    */
/******************************************************************************/
  
XrdFrmXfrJob *XrdFrmXfrQueue::Get()
{
   XrdFrmXfrJob *xfrP;

// Wait for an available job and return it
//
   do {qReady.Wait();} while(!(xfrP = Pull()));
   return xfrP;
}
  
/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
void *InitStop(void *parg)
{   XrdFrmXfrQueue::StopMon(parg);
    return (void *)0;
}
  
int XrdFrmXfrQueue::Init()
{
   static const char *StopFN[] = {"STAGE", "MIGR", "COPYIN", "COPYOUT"};
   static const char *StopQN[] = {"stage", "migr", "copyin", "copyout"};
   XrdFrmXfrJob *xP;
   pthread_t tid;
   char StopFile[1024], *fnSfx;
   int n, qNum, retc;

// Prepare to initialize the queues
//
   strcpy(StopFile, Config.AdminPath);
   strcat(StopFile, "STOP");
   fnSfx = StopFile + strlen(StopFile);

// Initialize each queue
//
   for (qNum= 0; qNum < XrdFrcRequest::numQ-1; qNum++)
      {

   // Initialize the stop file name and set the queue name and number
   //
        strcpy(fnSfx, StopFN[qNum]);
        xfrQ[qNum].File = strdup(StopFile);
        xfrQ[qNum].Name = StopQN[qNum];
        xfrQ[qNum].qNum = qNum;

   // Start the stop file monitor thread for this queue
   //
        if ((retc = XrdSysThread::Run(&tid, InitStop, (void *)&xfrQ[qNum],
                                      XRDSYSTHREAD_BIND, "Stopfile monitor")))
           {Say.Emsg("main", retc, "create stopfile thread"); return 0;}

   // Create twice as many free queue elements as we have xfr agents for the
   // queue. This prevents stalls when a particular queue is stopped but keeps
   // us from exceeding internal resources when we get flooded with requests.
   //
        n = Config.xfrMax*2;
        while(n--)
             {xP = new XrdFrmXfrJob;
              xP->Next = xfrQ[qNum].Free;
              xfrQ[qNum].Free = xP;
              xfrQ[qNum].Avail.Post();
             }
       }

// All done
//
   return 1;
}

/******************************************************************************/
/* Private:                         P u l l                                   */
/******************************************************************************/
  
XrdFrmXfrJob *XrdFrmXfrQueue::Pull()
{
   static int ioX = 0, prevQ[2] = {0,0};
   XrdFrmXfrJob *xfrP;
   int pikQ, theQ, Q1, Q2, nSel = 1;

// Setup to pick a request equally multiplexing between all possible queues
//
   qMutex.Lock();
do{ioX = (ioX + 1) & 1;
   if (ioX) {Q1 = XrdFrcRequest::migQ; Q2 = XrdFrcRequest::putQ; pikQ = 1;}
      else  {Q1 = XrdFrcRequest::stgQ; Q2 = XrdFrcRequest::getQ; pikQ = 0;}

// Check if we should avoid either queue because it is stopped
//
   if (xfrQ[Q1].Stop || Stopped(Q1)) Q1 = XrdFrcRequest::nilQ;
   if (xfrQ[Q2].Stop || Stopped(Q2)) Q2 = XrdFrcRequest::nilQ;

// Pick the oldest possible request
//
   if (xfrQ[Q1].First && xfrQ[Q2].First)
      {     if (xfrQ[Q1].First->reqData.addTOD < xfrQ[Q2].First->reqData.addTOD)
               theQ = Q1;
       else if (xfrQ[Q1].First->reqData.addTOD > xfrQ[Q2].First->reqData.addTOD)
               theQ = Q2;
       else theQ = (prevQ[pikQ] == Q1 ? Q2 : Q1);
      }else theQ = (xfrQ[Q1].First    ? Q1 : Q2);

// Dequeue the request (we may have an empty selectoin here)
//
   if ((xfrP = xfrQ[theQ].First)
   &&  !(xfrQ[theQ].First = xfrP->Next)) xfrQ[theQ].Last = 0;
  } while(!xfrP && nSel--);

// Return the job, if any
//
   prevQ[pikQ] = theQ;
   qMutex.UnLock();
   return xfrP;
}

/******************************************************************************/
/* Private:                       N o t i f y                                 */
/******************************************************************************/
  
int XrdFrmXfrQueue::Notify(XrdFrcRequest *rP, int qNum, int rc, const char *msg)
{
   static const char *isFile = "file:///";
   static const int   lnFile = 8;
   static const char *isUDP  = "udp://";
   static const int   lnUDP  = 6;
   static const char *qOpr[] = {"stage", "migr", "get", "put"};
   char msgbuff[4096], *nP, *mP = rP->Notify;
   int n;

// Check if message really needs to be sent
//
   if ((!rc && !(rP->Options & XrdFrcRequest::msgSucc))
   ||  ( rc && !(rP->Options & XrdFrcRequest::msgFail))) return 0;

// Multiple destinations can be specified, each destination separated by a
// carriable rturn. We don't screen out duplicates.
//
do{if ((nP = index(rP->Notify, '\r'))) *nP++ = '\0';

// Check for file destination
//
        if (!strncmp(mP, isFile, lnFile))
           {if (rc) n = sprintf(msgbuff, "%s %s %s %s\n", qOpr[qNum],
                        (rc > 1 ? "ENOENT":"BAD"), rP->LFN, (msg ? msg:"?"));
               else n = sprintf(msgbuff, "stage OK %s\n", rP->LFN);
            Send2File(mP+lnFile, msgbuff, n);
           }

// Check for udp destination
//
   else if (!strncmp(mP, isUDP,  lnUDP))
           {char *txtP, *dstP = mP+lnUDP;
            if ((txtP = index(dstP, '/'))) *txtP++ = '\0';
               else txtP = (char *)"";
            n = sprintf(msgbuff, "%s %s %s %s", (rc ? "unprep" : "ready"),
                                 rP->ID, txtP, rP->LFN);
            Send2UDP(dstP, msgbuff, n);
           }

// Issue warning as we don't yet support mail or tcp notifications
//
   else if (*mP != '-')
           Say.Emsg("Notify", "Unsupported notification path '", mP, "'.");
  } while((mP = nP));

// All done
//
   return 0;
}

/******************************************************************************/
/* Private:                    S e n d 2 F i l e                              */
/******************************************************************************/
  
void XrdFrmXfrQueue::Send2File(char *Dest, char *Msg, int Mln)
{
   EPNAME("Notify");
   int FD;

// Do some debugging
//
   DEBUG("sending '" <<Msg <<"' via " <<Dest);

// Open the file
//
   if ((FD = XrdSysFD_Open(Dest, O_WRONLY)) < 0)
      {Say.Emsg("Notify", errno, "send notification via", Dest); return;}

// Write the message
//
   if (write(FD, Msg, Mln) < 0)
      Say.Emsg("Notify", errno, "send notification via", Dest);
   close(FD);
}

/******************************************************************************/
/* Private:                     S e n d 2 U D P                               */
/******************************************************************************/

void XrdFrmXfrQueue::Send2UDP(char *Dest, char *Msg, int Mln)
{
   EPNAME("Notify");
   static XrdNetMsg Relay(&Say, 0);

// Do some debugging
//
   DEBUG("sending '" <<Msg <<"' via " <<Dest);
  
// Send off the message
//
   Relay.Send(Msg, Mln, Dest);
}

/******************************************************************************/
/* Public:                       S t o p M o n                                */
/******************************************************************************/
  
void XrdFrmXfrQueue::StopMon(void *parg)
{
   struct theQueue *monQ = (struct theQueue *)parg;
   XrdFrmXfrJob *xP;
   struct stat buf;
   char theMsg[80];
   int Cnt;

// Establish which message to produce
//
   sprintf(theMsg, "exists; %s transfers suspended.", monQ->Name);

// Wait until someone needs to tell us to check for a stop file
//
   while(1)
        {monQ->Alert.Wait();
         Cnt = 0;
         while(!stat(monQ->File, &buf))
              {if (!Cnt--) {Say.Emsg("StopMon", monQ->File, theMsg); Cnt = 12;}
               XrdSysTimer::Snooze(5);
              }
         qMutex.Lock();
         monQ->Stop = 0;
         xP = monQ->First;
         while(xP) {qReady.Post(); xP = xP->Next;}
         qMutex.UnLock();
        }
}

/******************************************************************************/
/* Private:                      S t o p p e d                                */
/******************************************************************************/
  
int XrdFrmXfrQueue::Stopped(int qNum) // Called with qMutex locked!
{
   struct stat buf;

// Check for stop file existence. If it exists and the queue has not been
// stopped; stop it and alert the stop file monitor.
//
   if (stat(xfrQ[qNum].File, &buf)) return 0;
   if (!xfrQ[qNum].Stop) {xfrQ[qNum].Stop = 1; xfrQ[qNum].Alert.Post();}
   return 1;
}

/******************************************************************************/
/* Private:                      x f r N a m e                                */
/******************************************************************************/
  
const char *XrdFrmXfrQueue::xfrName(XrdFrcRequest &reqData, int qNum)
{

// Return a human name for this transfer:
// Migrate
// Migr+rm
// Staging
// CopyIn
// CopyOut
// Copy+rm
//
   switch(qNum)
         {case XrdFrcRequest::getQ:
               return "1CopyIn ";
               break;
          case XrdFrcRequest::migQ: 
               return (reqData.Options & XrdFrcRequest::Purge ?
                       "3Migr+rm ":"2Migrate ");
               break;
          case XrdFrcRequest::putQ:
               return (reqData.Options&XrdFrcRequest::Purge ?
                       "5Copy+rm " : "4CopyOut ");
               break;
          case XrdFrcRequest::stgQ:
               return "6Staging ";
               break;
          default:   break;
         }

   return "0Unknown ";
}
