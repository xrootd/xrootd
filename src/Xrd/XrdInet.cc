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
#include <sys/socket.h>

#include "XrdSys/XrdSysDNS.hh"
#include "XrdSys/XrdSysError.hh"

#include "Xrd/XrdInet.hh"
#include "Xrd/XrdLink.hh"

#define XRD_TRACE XrdTrace->
#include "Xrd/XrdTrace.hh"

#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetPeer.hh"
#include "XrdNet/XrdNetSecurity.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

       const char *XrdInet::TraceID = "Inet";

/******************************************************************************/
/*                                A c c e p t                                 */
/******************************************************************************/

XrdLink *XrdInet::Accept(int opts, int timeout, XrdSysSemaphore *theSem)
{
   XrdNetPeer myPeer;
   XrdLink   *lp;
   char *hname = 0;
   int ismyfd, anum=0, lnkopts = (opts & XRDNET_MULTREAD ? XRDLINK_RDLOCK : 0);

// Perform regular accept. This will be a unique TCP socket. We loop here
// until the accept succeeds as it should never fail at this stage.
//
   opts |= XRDNET_NORLKUP;
   while(!XrdNet::Accept(myPeer, opts, timeout))
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
   if ((ismyfd = (myPeer.fd == iofd))) lnkopts |= XRDLINK_NOCLOSE;

// Authorize by ip address or full (slow) hostname format. We defer the check
// so that the next accept can occur before we do any DNS resolution.
//
   if (Patrol)
      {if (!(hname = Patrol->Authorize(&myPeer.InetAddr)))
          {eDest->Emsg("Accept",EACCES,"accept TCP connection from",
                       myPeer.InetName);
           if (!ismyfd) close(myPeer.fd);
           return (XrdLink *)0;
          }
      } else if (!(netOpts & XRDNET_NORLKUP))
                hname = XrdSysDNS::getHostName(myPeer.InetAddr);

// Undo the nodnr option if we actually forced it here
//
   if (hname)
      {if (myPeer.InetName) free(myPeer.InetName);
       myPeer.InetName = hname;
      }

// Allocate a new network object
//
   if (!(lp = XrdLink::Alloc(myPeer, lnkopts)))
      {eDest->Emsg("Accept",ENOMEM,"allocate new link for",myPeer.InetName);
       if (!ismyfd) close(myPeer.fd);
      } else {
       myPeer.InetBuff = 0; // Keep buffer after object goes away
       TRACE(NET, "Accepted connection from " <<myPeer.fd <<'@' <<myPeer.InetName);
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
   XrdNetPeer myPeer;
   XrdLink   *lp;
   int ismyfd, lnkopts = (opts & XRDNET_MULTREAD ? XRDLINK_RDLOCK : 0);

// Try to do a connect
//
   if (!XrdNet::Connect(myPeer, host, port, opts, tmo)) return (XrdLink *)0;
   if ((ismyfd = (myPeer.fd == iofd))) lnkopts |= XRDLINK_NOCLOSE;

// Return a link object
//
   if (!(lp = XrdLink::Alloc(myPeer, lnkopts)))
      {eDest->Emsg("Connect",ENOMEM,"allocate new link to",myPeer.InetName);
       if (!ismyfd) close(myPeer.fd);
      } else {
       myPeer.InetBuff = 0; // Keep buffer after object goes away
       TRACE(NET, "Connected to " <<myPeer.InetName <<':' <<port);
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
