/******************************************************************************/
/*                                                                            */
/*                            X r d I n e t . c c                             */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include "XrdSys/XrdSysError.hh"

#include "Xrd/XrdInet.hh"
#include "Xrd/XrdLink.hh"

#define XRD_TRACE XrdTrace->
#include "Xrd/XrdTrace.hh"

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetSecurity.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

       const char *XrdInet::TraceID = "Inet";

       XrdNetIF    XrdInet::netIF;

/******************************************************************************/
/*                                A c c e p t                                 */
/******************************************************************************/

XrdLink *XrdInet::Accept(int opts, int timeout, XrdSysSemaphore *theSem)
{
   static const char *unk = "unkown.endpoint";
   XrdNetAddr myAddr;
   XrdLink   *lp;
   int        anum=0, lnkopts = (opts & XRDNET_MULTREAD ? XRDLINK_RDLOCK : 0);

// Perform regular accept. This will be a unique TCP socket. We loop here
// until the accept succeeds as it should never fail at this stage.
//
   while(!XrdNet::Accept(myAddr, opts | XRDNET_NORLKUP, timeout))
        {if (timeout >= 0)
            {if (theSem) theSem->Post();
             return (XrdLink *)0;
            }
         sleep(1); anum++;
         if (!(anum%60)) eDest->Emsg("Accept", "Unable to accept connections!");
        }

// If authorization was defered, tell call we accepted the connection but
// will be doing a background check on this connection.
//
   if (theSem) theSem->Post();
   if (!(netOpts & XRDNET_NORLKUP)) myAddr.Name();

// Authorize by ip address or full (slow) hostname format. We defer the check
// so that the next accept can occur before we do any DNS resolution.
//
   if (Patrol)
      {if (!Patrol->Authorize(myAddr))
          {char ipbuff[512];
           myAddr.Format(ipbuff, sizeof(ipbuff), XrdNetAddr::fmtAuto,
                                                 XrdNetAddrInfo::noPort);
           eDest->Emsg("Accept",EACCES,"accept TCP connection from",ipbuff);
           close(myAddr.SockFD());
           return (XrdLink *)0;
          }
      }

// Allocate a new network object
//
   if (!(lp = XrdLink::Alloc(myAddr, lnkopts)))
      {eDest->Emsg("Accept", ENOMEM, "allocate new link for", myAddr.Name(unk));
       close(myAddr.SockFD());
      } else {
       TRACE(NET, "Accepted connection from " <<myAddr.SockFD()
                  <<'@' <<myAddr.Name(unk));
      }

// All done
//
   return lp;
}

/******************************************************************************/
/*                               C o n n e c t                                */
/******************************************************************************/

XrdLink *XrdInet::Connect(const char *host, int port, int opts, int tmo)
{
   static const char *unk = "unkown.endpoint";
   XrdNetAddr myAddr;
   XrdLink   *lp;
   int        lnkopts = (opts & XRDNET_MULTREAD ? XRDLINK_RDLOCK : 0);

// Try to do a connect. This will be a unique TCP socket.
//
   if (!XrdNet::Connect(myAddr, host, port, opts, tmo)) return (XrdLink *)0;

// Return a link object
//
   if (!(lp = XrdLink::Alloc(myAddr, lnkopts)))
      {eDest->Emsg("Connect", ENOMEM, "allocate new link to", myAddr.Name(unk));
       close(myAddr.SockFD());
      } else {
       TRACE(NET, "Connected to " <<myAddr.Name(unk) <<':' <<port);
      }

// All done, return whatever object we have
//
   return lp;
}
  
/******************************************************************************/
/*                                S e c u r e                                 */
/******************************************************************************/
  
void XrdInet::Secure(XrdNetSecurity *secp)
{

// If we don't have a Patrol object then use the one supplied. Otherwise
// merge the supplied object into the existing object.
//
   if (Patrol) Patrol->Merge(secp);
      else     Patrol = secp;
}
