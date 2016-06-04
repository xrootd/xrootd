#ifndef __XRDSSISSRUN_HH__
#define __XRDSSISSRUN_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d S s i S S R u n . h h                         */
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

#include "XrdSsi/XrdSsiService.hh"
  
//-----------------------------------------------------------------------------
//! The XrdSsiSSRun object effects the SSRun() methods in the SsiRequest object.
//-----------------------------------------------------------------------------

class XrdSsiRequest;

class XrdSsiSSRun : public XrdSsiService::Resource
{
public:

static
XrdSsiSSRun *Alloc(XrdSsiRequest *reqp, XrdSsiResource &rsrc,
                   unsigned short tmo=0);

void         ProvisionDone(XrdSsiSession *sessP);

             XrdSsiSSRun(XrdSsiRequest *reqp, unsigned short tmo=0)
                        : XrdSsiService::Resource(0),
                          theReq(reqp), tOut(tmo) {}

      ~XrdSsiSSRun() {}

private:
union {XrdSsiRequest  *theReq;
       XrdSsiSSRun    *freeNext;
      };
unsigned short         tOut;
};
#endif
