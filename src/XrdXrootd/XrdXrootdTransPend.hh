#ifndef __XRDXROOTDTRANSPEND_HH_
#define __XRDXROOTDTRANSPEND_HH_
/******************************************************************************/
/*                                                                            */
/*                 X r d X r o o t d T r a n s P e n d . h h                  */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdlib>
#include <cstring>

#include "XProtocol/XProtocol.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdLink;
class XrdXrootdTransit;

class XrdXrootdTransPend
{public:

XrdXrootdTransPend        *next;
XrdLink                   *link;
XrdXrootdTransit          *bridge;
union {ClientRequest       Request;
       short               theSid;
      }                    Pend;

static void                Clear(XrdXrootdTransit *trP);

       void                Queue();

static XrdXrootdTransPend *Remove(XrdLink *lP, short sid);

       XrdXrootdTransPend(XrdLink                   *lkP,
                          XrdXrootdTransit          *brP,
                          ClientRequest             *rqP)
                         : next(0), link(lkP), bridge(brP)
                          {memcpy(&Pend.Request, rqP, sizeof(Pend.Request));}

      ~XrdXrootdTransPend() {}

private:
static XrdSysMutex         myMutex;
static XrdXrootdTransPend *rqstQ;
};
#endif
