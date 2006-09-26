/******************************************************************************/
/*                                                                            */
/*                       X r d O l b X m i R e q . c c                        */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include <string.h>

#include "XrdOlb/XrdOlbTrace.hh"
#include "XrdOlb/XrdOlbXmiReq.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
XrdOlbXmi       *XrdOlbXmiReq::XmiP;
XrdOucMutex      XrdOlbXmiReq::prpMutex;
XrdOucSemaphore  XrdOlbXmiReq::prpReady(0);
XrdOlbXmiReq    *XrdOlbXmiReq::prpFirst = 0;
XrdOlbXmiReq    *XrdOlbXmiReq::prpLast  = 0;
XrdOucMutex      XrdOlbXmiReq::reqMutex;
XrdOucSemaphore  XrdOlbXmiReq::reqReady(0);
XrdOlbXmiReq    *XrdOlbXmiReq::reqFirst = 0;
XrdOlbXmiReq    *XrdOlbXmiReq::reqLast  = 0;
XrdOucMutex      XrdOlbXmiReq::stgMutex;
XrdOucSemaphore  XrdOlbXmiReq::stgReady(0);
XrdOlbXmiReq    *XrdOlbXmiReq::stgFirst = 0;
XrdOlbXmiReq    *XrdOlbXmiReq::stgLast  = 0;

using namespace XrdOlb;

/******************************************************************************/
/*            E x t e r n a l   T h r e a d   I n t e r f a c e s             */
/******************************************************************************/
  
void *XrdOlbXmi_StartPrpQ(void *parg)
{  
// XrdOlbXmiReq *requestProcessor = (XrdOlbXmiReq *)parg;

//?requestProcessor->processPrpQ();

   return (void *)0;
}
  
void *XrdOlbXmi_StartReqQ(void *parg)
{  
   XrdOlbXmiReq *requestProcessor = (XrdOlbXmiReq *)parg;

   requestProcessor->processReqQ();

   return (void *)0;
}
  
void *XrdOlbXmi_StartStgQ(void *parg)
{  
   XrdOlbXmiReq *requestProcessor = (XrdOlbXmiReq *)parg;

   requestProcessor->processStgQ();

   return (void *)0;
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOlbXmiReq::XrdOlbXmiReq(XrdOlbXmi *xp)
{
   ReqP    = 0;
   Path    = 0;
   Parms   = 0;
   Next    = 0;
   XmiP    = xp;
   Start();
}
  
XrdOlbXmiReq::XrdOlbXmiReq(XrdOlbReq *reqp, ReqType rtype,
                           const char *path, int parms)
{

   ReqP    = reqp;
   Path    = strdup(path);
   Parms   = parms;
   Rtype   = rtype;
   Next    = 0;

// Place ourselves on the proper request queue
//
        if (rtype == do_stage)
           {stgMutex.Lock();
            if (stgLast) {stgLast->Next = this; stgLast = this;}
               else      {stgFirst = stgLast = this; stgReady.Post();}
            stgMutex.UnLock();
           }
   else if (rtype == do_prep)
           {prpMutex.Lock();
            if (prpLast) {prpLast->Next = this; prpLast = this;}
               else      {prpFirst = prpLast = this; prpReady.Post();}
            prpMutex.UnLock();
           }
   else    {reqMutex.Lock();
            if (reqLast) {reqLast->Next = this; reqLast = this;}
               else      {reqFirst = reqLast = this; reqReady.Post();}
            reqMutex.UnLock();
           }
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOlbXmiReq::~XrdOlbXmiReq()
{
   if (Path) free(Path);
}

/******************************************************************************/
/*                                R e n a m e                                 */
/******************************************************************************/

int XrdOlbXmiReq::Rename(      XrdOlbReq      *Request,
                         const char           *oldpath,
                         const char           *newpath)
{
    char buff[4096], *bp;
    int olen = strlen(oldpath);
    int nlen = strlen(newpath);

    if (olen+nlen+1 >= static_cast<int>(sizeof(buff)))
       {Request->Reply_Error("Path length is too long");
        return 1;
       }

    strcpy(buff, oldpath);
    bp = buff+olen; *bp = ' '; bp++;
    strcpy(bp, newpath);

    return Qit(Request, do_mv, buff, 0);
}

/******************************************************************************/
/*                           p r o c e s s R e q Q                            */
/******************************************************************************/
  
void XrdOlbXmiReq::processReqQ()
{
   XrdOlbXmiReq *myQueue, *rp;
   char *cp;

// This is one big loop where we take off as many requests from the queue
// as we can and feed them to the general request processor
//
   while(1)
        {reqReady.Wait();
         reqMutex.Lock();
         myQueue  = reqFirst;
         reqFirst = reqLast = 0;
         reqMutex.UnLock();

         while((rp = myQueue))
              {myQueue = rp->Next;
               switch(rp->Rtype)
                     {case do_stat:   XmiP->Stat(rp->ReqP, rp->Path);
                                      break;
                      case do_mkdir:  XmiP->Mkdir(rp->ReqP, rp->Path, rp->Parms);
                                      break;
                      case do_mkpath: XmiP->Mkpath(rp->ReqP, rp->Path, rp->Parms);
                                      break;
                      case do_rmdir:  XmiP->Remdir(rp->ReqP, rp->Path);
                                      break;
                      case do_rm:     XmiP->Remove(rp->ReqP, rp->Path);
                                      break;
                      case do_mv:     if ((cp = index(rp->Path, ' ')))
                                         XmiP->Rename(rp->ReqP, rp->Path, cp+1);
                                         else rp->ReqP->Reply_Error("Internal request format error");
                                      break;
                      case do_chmod:  XmiP->Chmod(rp->ReqP, rp->Path, rp->Parms);
                                      break;
                      default: Say.Emsg("reqQ", "Invalid request code.");
                               rp->ReqP->Reply_Error("Internal server error");
                               break;
                     }
               delete rp;
              }
        }
}

/******************************************************************************/
/*                           p r o c e s s S t g Q                            */
/******************************************************************************/
  
void XrdOlbXmiReq::processStgQ()
{
   XrdOlbXmiReq *myQueue, *rp;

// This is one big loop where we take off as many requests from the queue
// as we can. However we feed them one at a time the Xmi select processor
// as we have found that the interfaces can be so gruesome that batching
// requests outweighs incurs complexity beyond belief.
//
   while(1)
        {stgReady.Wait();
         stgMutex.Lock();
         myQueue  = stgFirst;
         stgFirst = stgLast = 0;
         stgMutex.UnLock();

         while((rp = myQueue))
              {myQueue = rp->Next;
               XmiP->Select(rp->ReqP, rp->Path, rp->Parms);
               delete rp;
              }
        }
}

/******************************************************************************/
/*                                 S t a r t                                  */
/******************************************************************************/
  
void XrdOlbXmiReq::Start()
{
   pthread_t tid;
   int       retc;

// Start the thread that handles prepare requests
//
   if ((retc = XrdOucThread::Run(&tid, XrdOlbXmi_StartPrpQ, (void *)this,
                            XRDOUCTHREAD_BIND, "xmi prepare handler")))
      {Say.Emsg("XmiReq", retc, "create prepare thread"); _exit(3);}

// Start the thread that handles general requests
//
   if ((retc = XrdOucThread::Run(&tid, XrdOlbXmi_StartReqQ, (void *)this,
                            XRDOUCTHREAD_BIND, "xmi request handler")))
      {Say.Emsg("XmiReq", retc, "create request thread"); _exit(3);}

// Start the thread that handles staging requests
//
   if ((retc = XrdOucThread::Run(&tid, XrdOlbXmi_StartStgQ, (void *)this,
                            XRDOUCTHREAD_BIND, "xmi staging handler")))
      {Say.Emsg("XmiReq", retc, "create staging thread"); _exit(3);}
}
 
/******************************************************************************/
/*                                   Q i t                                    */
/******************************************************************************/
  
int XrdOlbXmiReq::Qit(XrdOlbReq *rp, ReqType rt, const char *path, int parms)
{
    new XrdOlbXmiReq(rp->Reply_WaitResp(5940), rt, path, parms);
    return 1;
}
