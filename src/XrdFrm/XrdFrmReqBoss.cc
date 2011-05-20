/******************************************************************************/
/*                                                                            */
/*                      X r d F r m R e q B o s s . c c                       */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdFrc/XrdFrcCID.hh"
#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrc/XrdFrcUtils.hh"
#include "XrdFrm/XrdFrmReqBoss.hh"
#include "XrdFrm/XrdFrmXfrQueue.hh"
#include "XrdNet/XrdNetMsg.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysHeaders.hh"

using namespace XrdFrc;

/******************************************************************************/
/*                     T h r e a d   I n t e r f a c e s                      */
/******************************************************************************/
  
void *mainServerXeq(void *parg)
{
    XrdFrmReqBoss *theBoss = (XrdFrmReqBoss *)parg;
    theBoss->Process();
    return (void *)0;
}

/******************************************************************************/
/* Public:                           A d d                                    */
/******************************************************************************/
  
void XrdFrmReqBoss::Add(XrdFrcRequest &Request)
{

// Complete the request including verifying the priority
//
   if (Request.Prty > XrdFrcRequest::maxPrty)
      Request.Prty = XrdFrcRequest::maxPrty;
      else if (Request.Prty < 0)Request.Prty = 0;
   Request.addTOD = time(0);

// Now add it to the queue
//
   rQueue[static_cast<int>(Request.Prty)]->Add(&Request);

// Now wake ourselves up
//
   Wakeup(1);
}

/******************************************************************************/
/* Public:                           D e l                                    */
/******************************************************************************/
  
void XrdFrmReqBoss::Del(XrdFrcRequest &Request)
{
   int i;
  
// Remove all pending requests for this id
//
   for (i = 0; i <= XrdFrcRequest::maxPrty; i++) rQueue[i]->Can(&Request);
}

/******************************************************************************/
/* Public:                       P r o c e s s                                */
/******************************************************************************/
  
void XrdFrmReqBoss::Process()
{
   EPNAME("Process");
   XrdFrcRequest myReq;
   int i, rc, numXfr, numPull;;

// Perform staging in an endless loop
//
do{Wakeup(0);
   do{numXfr = 0;
      for (i = XrdFrcRequest::maxPrty; i >= 0; i--)
          {numPull = i+1;
           while(numPull && (rc = rQueue[i]->Get(&myReq)))
                {if (myReq.Options & XrdFrcRequest::Register) Register(myReq,i);
                    else {numPull -= XrdFrmXfrQueue::Add(&myReq,rQueue[i],theQ);
                          numXfr++;
                          DEBUG(Persona <<" from Q " << i <<' ' <<numPull <<" left");
                          if (rc < 0) break;
                         }
                }
          }
     } while(numXfr);
  } while(1);
}

/******************************************************************************/
/* Private:                     R e g i s t e r                               */
/******************************************************************************/

void XrdFrmReqBoss::Register(XrdFrcRequest &Req, int qNum)
{
   EPNAME("Register");
   char *eP;
   int Pid;

// Ignore this request if there is no cluster id or the process if is invalid
//
   if (!(*Req.LFN)) return;
   Pid = strtol(Req.ID, &eP, 10);
   if (*eP || Pid == 0) return;

// Register this cluster
//
   if (CID.Add(Req.iName, Req.LFN, static_cast<time_t>(Req.addTOD), Pid))
      {DEBUG("Instance=" <<Req.iName <<" cluster=" <<Req.LFN <<" pid=" <<Pid);}
      else rQueue[qNum]->Del(&Req);
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
int XrdFrmReqBoss::Start(char *aPath, int aMode)
{
   pthread_t tid;
   char buff[2048], *qPath;
   int retc, i;

// Generate the queue directory path
//
   if (!(qPath = XrdFrcUtils::makeQDir(aPath, aMode))) return 0;

// Initialize the request queues if all went well
//
   for (i = 0; i <= XrdFrcRequest::maxPrty; i++)
       {sprintf(buff, "%s%sQ.%d", qPath, Persona, i);
        rQueue[i] = new XrdFrcReqFile(buff, 0);
        if (!rQueue[i]->Init()) return 0;
       }

// Start the request processing thread
//
   if ((retc = XrdSysThread::Run(&tid, mainServerXeq, (void *)this,
                                 XRDSYSTHREAD_BIND, Persona)))
      {sprintf(buff, "create %s request thread", Persona);
       Say.Emsg("Start", retc, buff);
       return 0;
      }

// All done
//
   return 1;
}

/******************************************************************************/
/* Public:                        W a k e u p                                 */
/******************************************************************************/
  
void XrdFrmReqBoss::Wakeup(int PushIt)
{
   static XrdSysMutex     rqMutex;

// If this is a PushIt then see if we need to push the binary semaphore
//
   if (PushIt) {rqMutex.Lock();
                if (!isPosted) {rqReady.Post(); isPosted = 1;}
                rqMutex.UnLock();
               }
      else     {rqReady.Wait();
                rqMutex.Lock(); isPosted = 0; rqMutex.UnLock();
               }
}
