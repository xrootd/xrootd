/******************************************************************************/
/*                                                                            */
/*                       X r d S e c C l i e n t . c c                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iomanip>
#include <sys/param.h>
#include <sys/types.h>

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSec/XrdSecPManager.hh"
#include "XrdSec/XrdSecInterface.hh"

/******************************************************************************/
/*                 M i s c e l l a n e o u s   D e f i n e s                  */
/******************************************************************************/

#define DEBUG(x) {if (DebugON) cerr <<"sec_Client: " <<x <<endl;}

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdSecProtNone : public XrdSecProtocol
{
public:
int                Authenticate  (XrdSecCredentials  *cred,
                                  XrdSecParameters  **parms,
                                  XrdOucErrInfo      *einfo=0) 
                                 {return 0;}

XrdSecCredentials *getCredentials(XrdSecParameters  *parm=0,       // In
                                  XrdOucErrInfo     *einfo=0)
                                 {return new XrdSecCredentials();}

void               Delete() {}  // Never deleted because it's static!

              XrdSecProtNone() : XrdSecProtocol("") {}
             ~XrdSecProtNone() {}
};
  
/******************************************************************************/
/*                     X r d S e c G e t P r o t o c o l                      */
/******************************************************************************/

// This function is only invoked by the client. It exists in the top level
// shared library that interposes between all other protocol shared libraries.
//
extern "C"
{
XrdSecProtocol *XrdSecGetProtocol(const char             *hostname,
                                        XrdNetAddrInfo   &endPoint,
                                        XrdSecParameters &parms,
                                        XrdOucErrInfo    *einfo)
{
   static int DebugON = ((getenv("XrdSecDEBUG") &&
                          strcmp(getenv("XrdSecDEBUG"), "0")) ? 1 : 0);
   static XrdSecProtNone ProtNone;
   static XrdSecPManager PManager(DebugON);
   const char *noperr = "XrdSec: No authentication protocols are available.";

   XrdSecProtocol *protp;

// Perform any required debugging
//
   DEBUG("protocol request for host " <<hostname <<" token='"
         <<(parms.size > 0 ? setw(parms.size) : setw(1))
         <<(parms.size > 0 ? parms.buffer : "") <<"'");

// Check if the server wants no security.
//
   if (!parms.size || !parms.buffer[0]) return (XrdSecProtocol *)&ProtNone;

// Find a supported protocol.
//
   if (!(protp = PManager.Get(hostname, endPoint, parms)))
      {if (einfo) einfo->setErrInfo(ENOPROTOOPT, noperr);
         else cerr <<noperr <<endl;
      }

// All done
//
   return protp;
}
}
