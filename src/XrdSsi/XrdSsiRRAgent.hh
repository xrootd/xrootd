#ifndef __XRDSSIRRAGENT_HH__
#define __XRDSSIRRAGENT_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d S s i R R A g e n t . h h                       */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSsi/XrdSsiRequest.hh"
#include "XrdSsi/XrdSsiResponder.hh"

class XrdSsiMuex;

class XrdSsiRRAgent
{
public:

static void            Alert(XrdSsiRequest &reqR, XrdSsiRespInfoMsg &aMsg)
                            {reqR.Alert(aMsg);}

static void            CleanUp(XrdSsiRequest &reqR) {reqR.CleanUp();}

static void            Dispose(XrdSsiRequest &reqR) {reqR.Dispose();}

static XrdSsiErrInfo  &ErrInfoRef(XrdSsiRequest *rP) {return rP->errInfo;}

static bool            isaRetry(XrdSsiRequest *rP, bool reset=false)
                               {if (rP->flags & XrdSsiRequest::isaRetry)
                                   {if (reset)
                                       rP->flags &= ~XrdSsiRequest::isaRetry;
                                    return true;
                                   }
                                return false;
                               }

static void            onServer(XrdSsiRequest *rP) {rP->onClient = false;}

static XrdSsiRequest  *Request(XrdSsiResponder *rP) {return rP->reqP;}

static XrdSsiRespInfo *RespP(XrdSsiRequest *rP) {return &(rP->Resp);}

static void            SetNode(XrdSsiRequest *rP, const char *name)
                              {rP->epNode = name;}

static void            ResetResponder(XrdSsiResponder *rP)
                                     {rP->spMutex.Lock();
                                      rP->reqP = 0;
                                      rP->spMutex.UnLock();
                                     }

static void            SetMutex(XrdSsiRequest *rP, XrdSsiMutex *mP)
                               {rP->rrMutex = mP;}
};
#endif
