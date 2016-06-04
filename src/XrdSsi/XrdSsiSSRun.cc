/******************************************************************************/
/*                                                                            */
/*                        X r d S s i S S R u n . c c                         */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSsi/XrdSsiAtomics.hh"
#include "XrdSsi/XrdSsiRequest.hh"
#include "XrdSsi/XrdSsiSSRun.hh"

/******************************************************************************/
/*                         L o c a l   S t a t i c s                          */
/******************************************************************************/
  
namespace
{
XrdSsiMutex       ssrMutex;
XrdSsiSSRun      *freeSSR = 0;
int               freeNum = 0;

static const int  maxFree = 512;
}

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/

XrdSsiSSRun *XrdSsiSSRun::Alloc(XrdSsiRequest  *reqp,
                                XrdSsiResource &rsrc,
                                unsigned short  tmo)
{
   XrdSsiSSRun *ssrP;
   char *datP;
   int totLen = 0, rNameLen = 0, rUserLen = 0, rInfoLen = 0, hAvoidLen = 0;

// The first step is to get the lengths of all the resource data members
//
                    {rNameLen  = strlen(rsrc.rName) +1; totLen  = rNameLen;}
   if (rsrc.rUser)  {rUserLen  = strlen(rsrc.rUser) +1; totLen += rUserLen;}
   if (rsrc.rInfo)  {rInfoLen  = strlen(rsrc.rInfo) +1; totLen += rInfoLen;}
   if (rsrc.hAvoid) {hAvoidLen = strlen(rsrc.hAvoid)+1; totLen += hAvoidLen;}

// Now allocate memory to copy all the members
//
   datP = (char *)malloc(totLen);
   if (!datP) return 0;

// Now allocate a new SSRun object
//
   ssrMutex.Lock();
   if ((ssrP = freeSSR))
      {freeSSR = ssrP->freeNext;
       freeNum--;
       ssrMutex.UnLock();
      } else ssrP = new XrdSsiSSRun(reqp, tmo);

// Copy all the members (we are assured to have an rName by the caller).
//
      {ssrP->rDesc.rName  = datP;
       strcpy(datP, rsrc.rName);
       datP += rNameLen;
      }
   if (rUserLen)
      {ssrP->rDesc.rUser  = datP;
       strcpy(datP, rsrc.rUser);
       datP += rUserLen;
      }
   if (rInfoLen)
      {ssrP->rDesc.rInfo  = datP;
       strcpy(datP, rsrc.rInfo);
       datP += rInfoLen;
      }
   if (hAvoidLen)
      {ssrP->rDesc.hAvoid = datP;
       strcpy(datP, rsrc.hAvoid);
      }
   ssrP->rDesc.affinity = rsrc.affinity;

// Indicate wwe want an automatic unprovision upon finish
//
   ssrP->rDesc.rOpts |= XrdSsiResource::autoUnP;

// Return the object
//
   return ssrP;
}
  
/******************************************************************************/
/*                         P r o v i s i o n D o n e                          */
/******************************************************************************/
  
void XrdSsiSSRun::ProvisionDone(XrdSsiSession *sessP)
{

// If provisioning was successful and if so, run it.
//
   if (sessP) sessP->ProcessRequest(theReq, tOut);
      else {const char *eText; int eNum;
            eText = eInfo.Get(eNum);
            theReq->eInfo.Set(eText, eNum);
            theReq->Resp.eMsg  = theReq->eInfo.Get(theReq->Resp.eNum);
            theReq->Resp.rType = XrdSsiRespInfo::isError;
            theReq->ProcessResponse(theReq->Resp, false);
           }

// We are done, recycle ourselves prior to returning.
//
   if (rDesc.rName) free((void *)rDesc.rName);
   memset(&rDesc, 0, sizeof(rDesc));

   ssrMutex.Lock();
   if (freeNum < maxFree)
      {freeNext = freeSSR;
       freeSSR  = this;
       freeNum++;
       ssrMutex.UnLock();
      } else {
       ssrMutex.UnLock();
       delete this;
      }
}
