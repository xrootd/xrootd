/******************************************************************************/
/*                                                                            */
/*                      X r d O f s T P C A u t h . c c                       */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdio>
#include <cstdlib>
#include <strings.h>
  
#include "XrdOfs/XrdOfsStats.hh"
#include "XrdOfs/XrdOfsTPCAuth.hh"
#include "XrdOfs/XrdOfsTPCConfig.hh"
#include "XrdOuc/XrdOucCallBack.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysTimer.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
extern XrdSysError  OfsEroute;
extern XrdOfsStats  OfsStats;

namespace XrdOfsTPCParms
{
extern XrdOfsTPCConfig Cfg;
}
using namespace XrdOfsTPCParms;

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
XrdSysMutex        XrdOfsTPCAuth::authMutex;
XrdOfsTPCAuth     *XrdOfsTPCAuth::authQ    = 0;

/******************************************************************************/
/*                     E x t e r n a l   L i n k a g e s                      */
/******************************************************************************/
  
void *XrdOfsTPCAuthttl(void *pp)
{
     XrdOfsTPCAuth::RunTTL(0);
     return (void *)0;
}

/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
int XrdOfsTPCAuth::Add(XrdOfsTPC::Facts &Args)
{
   XrdOfsTPCAuth  *aP;
   const char     *eMsg;
         char     Buff[512];

// Generate the origin information
//
   if (!genOrg(Args.Usr, Buff, sizeof(Buff))) return Fatal(Args, Buff, EINVAL);
   Args.Org = Buff;

// Check if there is a matching authorization in the queue. If this is for a
// pending authorization, indicated that we now have one. Otherwise, consider
// this a potential security breach and cancel both autorizations.
//
   authMutex.Lock();
   if ((aP = Find(Args)))
      {if (aP->Info.cbP)
          {aP->expT = expT;
           aP->Next = authQ; authQ = aP;
           aP->Info.Reply(SFS_OK, 0, "", &authMutex);
           return 1;
          } else {
           authMutex.UnLock();
           return Fatal(Args, "duplicate athorization", EPROTO);
          }
      }

// Set the copy authorization information
//
   if ((eMsg = Info.Set(Args.Key, Buff, Args.Lfn, Args.Dst)))
   {
     authMutex.UnLock();
     return Fatal(Args, eMsg, EINVAL);
   }

// Add this to queue
//
   Next = authQ; authQ = this; inQ = 1;

// All done
//
   authMutex.UnLock();
   return 1;
}
  
/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/
  
void XrdOfsTPCAuth::Del()
{
   XrdOfsTPCAuth *pP;

// Remove from queue if we are still in the queue
//
   authMutex.Lock();
   if (inQ)
      {if (this == authQ) authQ = Next;
          else {pP = authQ;
                while(pP && pP->Next != this) pP = pP->Next;
                if (pP) pP->Next = Next;
               }
       inQ = 0;
      }

// Delete the element if possible
//
   if (Refs <= 1) delete this;
      else Refs--;
   authMutex.UnLock();
}

/******************************************************************************/
/*                               E x p i r e d                                */
/******************************************************************************/

int XrdOfsTPCAuth::Expired(const char *Dst, int cnt)
{
   char Buff[1024];

// If there is a callback, tell the client they are no longer wanted
//
   if (Info.cbP) Info.Reply(SFS_ERROR, EACCES, "tpc authorization expired");

// Log this event
//
   snprintf(Buff, sizeof(Buff), "tpc grant by %s expired for", Info.Org);
   Buff[sizeof(Buff)-1] = 0;
   OfsEroute.Emsg("TPC", Dst, Buff, Info.Lfn);

// Count stats and return
//
   if (cnt) OfsStats.Add(OfsStats.Data.numTPCexpr);
   return 0;
}

/******************************************************************************/
/* Private:                         F i n d                                   */
/******************************************************************************/

XrdOfsTPCAuth *XrdOfsTPCAuth::Find(XrdOfsTPC::Facts &Args)
{
   XrdOfsTPCAuth *cP, *pP = 0;

// Find matching entry
//
   cP = authQ;
   while(cP && !(cP->Info.Match(Args.Key, Args.Org, Args.Lfn, Args.Dst)))
        {pP = cP; cP = cP->Next;}

// Remove from queue if found
//
   if (cP) {if (pP) pP->Next = cP->Next;
               else authQ    = cP->Next;
            cP->inQ = 0;
           }

// Return result
//
   return cP;
}
  
/******************************************************************************/
/*                                   G e t                                    */
/******************************************************************************/

int XrdOfsTPCAuth::Get(XrdOfsTPC::Facts &Args, XrdOfsTPCAuth **theTPC)
{
   XrdSysMutexHelper authMon(&authMutex);
   XrdOfsTPCAuth *aP;
   const char *eMsg;

// Check if there is a matching authorization in the queue. If this is for a
// pending authorization, then consider this a potential security breach and
// cancel both requests. Otherwise, indicate that authorization is present.
//
   if ((aP = Find(Args)))
      {if (aP->Info.cbP)
          {aP->Info.Reply(SFS_ERROR, EPROTO, "duplicate tpc auth request");
           return Fatal(Args, "duplicate tpc auth request", EPROTO);
          } else {
           aP->Refs++;
           *theTPC = aP;
           return SFS_OK;
          }
      }

// Add this request as a pending authorization to the queue
//
   if (!(aP = new XrdOfsTPCAuth(Cfg.maxTTL)))
      return Fatal(Args, "insufficient memory", ENOMEM);

// Set the copy authorization information
//
   if ((eMsg = aP->Info.Set(Args.Key, Args.Org, Args.Lfn, Args.Dst)))
      {delete aP;
       return Fatal(Args, eMsg, EINVAL);
      }

// Create a callback
//
   if (aP->Info.SetCB(Args.eRR)) {delete aP; return SFS_ERROR;}

// Add it to the queue
//
   aP->Next = authQ; authQ = aP;

// Return result
//
   *theTPC = aP;
   aP->Refs = 0;
   aP->Info.Engage();
   return SFS_STARTED;
}

/******************************************************************************/
/*                                R u n T T L                                 */
/******************************************************************************/
  
int XrdOfsTPCAuth::RunTTL(int Init)
{
   XrdOfsTPCAuth *cP, *pP, *nP;
   time_t        eNow;
   int           eWait, eDiff, numExp;

// Start the expiration thread
//
   if (Init)
      {pthread_t tid;
       int       rc;
       if ((rc = XrdSysThread::Run(&tid,XrdOfsTPCAuthttl,0,0,"TPC ttl runner")))
          OfsEroute.Emsg("TPC", rc, "create tpc ttl runner thread");
           return (rc ? 0 : 1);
      }

// Find all expired entries and remove them
//
do{authMutex.Lock();
   cP = authQ; pP = 0;
   eNow = time(0); eWait = Cfg.maxTTL; numExp = 0;
   while(cP)
        {if (eNow < cP->expT)
            {eDiff = cP->expT - eNow;
             if (eDiff < eWait) eWait = eDiff;
             pP = cP; cP = cP->Next;
            }
            else {if (pP) pP->Next = cP->Next;
                     else authQ    = cP->Next;
                  cP->Expired("localhost", 0); numExp++;
                  nP = cP->Next;
                  if (cP->Refs < 1) delete cP;
                  cP = nP;
                 }
        }
   authMutex.UnLock();

// Add number of expirations to statistics
//
   if (numExp)
      {OfsStats.sdMutex.Lock();
       OfsStats.Data.numTPCexpr += numExp;
       OfsStats.sdMutex.UnLock();
      }

// Wait as long as possible for a recan
//
   XrdSysTimer::Snooze(eWait);
  } while(1);
}
