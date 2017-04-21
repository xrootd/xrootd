/******************************************************************************/
/*                                                                            */
/*                    X r d S s i R e s p o n d e r . h h                     */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include "XrdSsi/XrdSsiResponder.hh"
#include "XrdSsi/XrdSsiRRAgent.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
namespace
{
class XeqUnBind : public XrdSsiResponder
{
public:

virtual void   Finished(      XrdSsiRequest  &rqstR,
                        const XrdSsiRespInfo &rInfo,
                              bool            cancel=false)
                       {XrdSsiRRAgent::Unbind(rqstR);}

               XeqUnBind() {}
              ~XeqUnBind() {}
};
}

/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/
  
XrdSsiMutex XrdSsiResponder::ubMutex;

XeqUnBind   ForceUnBind;

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdSsiResponder::~XrdSsiResponder()
{
// Obtain the shared mutex
//
   rrMutex->Lock();

// If we haven't taken a trip to unbind() then we need to unbind here. The
// issue we have is that the object may be deleted before Finish() is called
// which is quite dicey. So, we defer the UnBind() until Finished() is called.
//
   if (rrMutex == &ubMutex) rrMutex->UnLock();
      else {XrdSsiRequest *rP = reqP;
            if (reqP)
               {reqP = 0;
                if (rP->theRespond == 0) // Finish() has been called
                   {rrMutex->UnLock();
                    rP->Unbind(this);
                   } else {
                    rP->theRespond = &ForceUnBind;
                    rrMutex->UnLock();
                   }
               }
           }
}

/******************************************************************************/
/*                           B i n d R e q u e s t                            */
/******************************************************************************/
  

void XrdSsiResponder::BindRequest(XrdSsiRequest   &rqstR)
{
// Obtain the request mutex and share it with ourselves.
//
   XrdSsiMutexMon(rqstR.rrMutex);
   rrMutex = rqstR.rrMutex;

// Link up this object to the request and vice versa
//
   rqstR.theRespond = this;
   reqP = &rqstR;

// Initialize the request object
//
   rqstR.Resp.Init();
   rqstR.errInfo.Clr();

// Notify the request that the bind comleted (this is only used on the
// server to allow pending alerts to be sent to the responder.
//
   rqstR.BindDone();
}

/******************************************************************************/
/*                         U n B i n d R e q u e s t                          */
/******************************************************************************/
  
bool XrdSsiResponder::UnBindRequest()
{
// Lock the shared mutex
//
   rrMutex->Lock();

// If we have a request pointer and Finish() was called the we an actually
// do the unbind(). Otherwise, return a failure as this is just wrong!
//
   if (reqP && reqP->theRespond == 0)
      {XrdSsiMutex   *mP = rrMutex;
       XrdSsiRequest *rP = reqP;
       reqP = 0;
       rrMutex = &ubMutex;
       mP->UnLock();
       rP->Unbind(this);
       return true;
      }

// Unbind is not valid in this context. Tell he caller so.
//
   rrMutex->UnLock();
   return false;
}
