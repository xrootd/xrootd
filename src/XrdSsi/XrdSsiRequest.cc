/******************************************************************************/
/*                                                                            */
/*                      X r d S s i R e q u e s t . c c                       */
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "XrdSsi/XrdSsiPacer.hh"
#include "XrdSsi/XrdSsiRespInfo.hh"
#include "XrdSsi/XrdSsiRequest.hh"
#include "XrdSsi/XrdSsiResource.hh"
#include "XrdSsi/XrdSsiService.hh"
#include "XrdSsi/XrdSsiSSRun.hh"
  
/******************************************************************************/
/* Private:                     C o p y D a t a                               */
/******************************************************************************/
  
bool XrdSsiRequest::CopyData(char *buff, int blen)
{
   bool last;

// Make sure the buffer length is valid
//
   if (blen <= 0)
      {eInfo.Set("Buffer length invalid", EINVAL);
       return false;
      }

// Check if we have any data here
//
   reqMutex.Lock();
   if (Resp.blen > 0)
      {if (Resp.blen > blen) last = false;
          else {blen = Resp.blen; last = true;}
       memcpy(buff, Resp.buff, blen);
       Resp.buff += blen; Resp.blen -= blen;
      } else {blen = 0; last = true;}
   reqMutex.UnLock();

// Invoke the callback
//
   ProcessResponseData(buff, blen, last);
   return true;
}

/******************************************************************************/
/*                              F i n i s h e d                               */
/******************************************************************************/

bool XrdSsiRequest::Finished(bool cancel)
{
   XrdSsiMutexMon(reqMutex);

// If there is no session, return failure
//
   if (!theSession) return false;

// Tell the session we are finished
//
   theSession->RequestFinished(this, Resp, cancel);

// Clear response and error information
//
   Resp.Init();
   eInfo.Clr();

// Clear pointers and return
//
   theRespond = 0;
   theSession = 0;
   return true;
}
  
/******************************************************************************/
/*                                 S S R u n                                  */
/******************************************************************************/
  
void XrdSsiRequest::SSRun(XrdSsiService  &srvc,
                          XrdSsiResource &rsrc,
                          unsigned short  tmo)
{
   XrdSsiSSRun *runP;

// Make sure that atleats the resource name was specified
//
   if (!rsrc.rName || !(*rsrc.rName))
      {eInfo.Set("Resource name missing.", EINVAL);
       Resp.eMsg  = eInfo.Get();
       Resp.eNum  = EINVAL;
       Resp.rType = XrdSsiRespInfo::isError;
       ProcessResponse(Resp, false);
       return;
      }

// Now allocate memory to copy all the members
//
   runP = XrdSsiSSRun::Alloc(this, rsrc, tmo);
   if (!runP)
      {eInfo.Set(0, ENOMEM);
       Resp.eMsg  = eInfo.Get();
       Resp.eNum  = ENOMEM;
       Resp.rType = XrdSsiRespInfo::isError;
       ProcessResponse(Resp, false);
       return;
      }

// Now provision the resource and we are done here. The SSRun object takes over.
//
   srvc.Provision(runP, tmo);
}

/******************************************************************************/

void XrdSsiRequest::SSRun(XrdSsiService  &srvc,
                          const char     *rname,
                          const char     *ruser,
                          unsigned short  tmo)
{
   XrdSsiResource myRes(rname, ruser);

   SSRun(srvc, myRes, tmo);
}

/******************************************************************************/
/*                   R e s t a r t D a t a R e s p o n s e                    */
/******************************************************************************/
  
XrdSsiRequest::RDR_Info XrdSsiRequest::RestartDataResponse
                                      (XrdSsiRequest::RDR_How  rhow,
                                       const char             *reqid
                                      )
{
   RDR_Info rInfo;

   XrdSsiPacer::Run(rInfo, rhow, reqid);
   return rInfo;
}
