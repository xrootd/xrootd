/******************************************************************************/
/*                                                                            */
/*                          X r d N e t M s g . c c                           */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cerrno>
#include <poll.h>
#include <sys/uio.h>

#include "XrdNet/XrdNet.hh"
#include "XrdNet/XrdNetMsg.hh"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetPeer.hh"
#include "XrdNet/XrdNetRefresh.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdNetMsg::XrdNetMsg(XrdSysError *erp, const char *dest, bool *aOK, bool refr) 
                    : eDest(erp)
{
   XrdNet myNet(erp);
   bool aok = true;

// Handle the case where no dest was specified. In this case we will always
// need the caller to specify a destination.
//
   if (!dest)
      {if ((FD = myNet.Relay(dest)) < 0)
         {eDest->Emsg("NetMsg", "Unable to create UDP msg socket.");
          aok = false;
         }
       if (aOK) *aOK = aok;
       return;
      }

// Hande the common case where a dest is specified. We first make
// sure the dest is valid, eventhough that will occur again, so we van
// generate a resonable error message.
//      
   XrdNetAddr specDest;
   const char *eText = specDest.Set(dest);
   if (eText)
      {eDest->Emsg("NetMsg", "Default", dest, "is unreachable");
       if (aOK) *aOK = false;
       return;
      }

// Obtain a file description for this socket and set the endpoint address
//
   XrdNetPeer myPeer;

   if (!myNet.Relay(myPeer, dest, XRDNET_SENDONLY))
      {eDest->Emsg("NetMsg", "Unable to create UDP msg socket.");
       if (aOK) *aOK = false;
       return;
      }

// Save the relevant information
//
   dfltDest = strdup(myPeer.InetName ? myPeer.InetName : "Unknown!");
   FD       = myPeer.fd;
   destOK   = true;

// If address refresh wanted, register this socket for refresh. This should
// never fail and if it does we return non-success.
//
   if (refr && !XrdNetRefresh::Register(myPeer)) aok = false;

// All done
//
   if (aOK) *aOK = aok;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdNetMsg::~XrdNetMsg()
{
// If we are registered,deregister
//
   if (isRefr) XrdNetRefresh::UnRegister(FD);

// Close the socket
//
   if (close(FD) < 0)
      eDest->Emsg("NetMsg", errno, "close socket for", dfltDest);

// Free the poiinter to the default dest
//
   free(dfltDest);
}
  
/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/
  
int XrdNetMsg::Send(const char *Buff, int Blen, const char *dest, int tmo)
{
   int retc;

// Get the buffer length of not specified
//
   if (!Blen && !(Blen = strlen(Buff))) return  0;

// Handle the case where we are sendingto he dest setup at construction. This
// is the most common case.
//
   if (!dest)
       {if (!destOK)
           {eDest->Emsg("NetMsg", "Destination not specified."); return -1;}

        if (tmo >= 0 && !OK2Send(tmo, dfltDest)) return 1;

        do {retc = send(FD, (Sokdata_t)Buff, Blen, 0);
           } while (retc < 0 && errno == EINTR);

        return (retc < 0 ? retErr(errno, dfltDest) : 0);
       }

// Caller want to send to a specific destination other than the default
//
   XrdNetAddr specDest;

   if (specDest.Set(dest))
      {eDest->Emsg("NetMsg", dest, "is unreachable"); return -1;}

   if (tmo >= 0 && !OK2Send(tmo, dest)) return 1;

   do {retc = sendto(FD, (Sokdata_t)Buff, Blen, 0, 
                     specDest.SockAddr(), specDest.SockSize());}
       while (retc < 0 && errno == EINTR);

   return (retc < 0 ? retErr(errno, specDest) : 0);
}

/******************************************************************************/
  
int XrdNetMsg::Send(const char *dest, const XrdNetSockAddr &netSA,
                    const char *Buff, int Blen, int tmo)
{
   int aSize, retc;

   if (!Blen && !(Blen = strlen(Buff))) return  0;

   if (netSA.Addr.sa_family == AF_INET) aSize = sizeof(netSA.v4);
      else if (netSA.Addr.sa_family == AF_INET6) aSize = sizeof(netSA.v6);
              else return retErr(EAFNOSUPPORT, (dest ? dest : "Unknown!"));

   if (tmo >= 0 && !OK2Send(tmo, dest)) return 1;

   do {retc = sendto(FD, (Sokdata_t)Buff, Blen, 0, &netSA.Addr, aSize);}
       while (retc < 0 && errno == EINTR);

   if (retc >= 0) return 0;
   return retErr(errno, (dest ? dest : "Unknown!"));
}

/******************************************************************************/

int XrdNetMsg::Send(const struct iovec iov[], int iovcnt, 
                    const char  *dest,        int tmo)
{

// Handle the common case of sendingto the cobbected address
//
   if (!dest)
      {if (!destOK)
          {eDest->Emsg("NetMsg", "Destination not specified."); return -1;}
       if (tmo >= 0 && !OK2Send(tmo, dfltDest)) return 1;
       if (writev(FD, iov, iovcnt) >= 0) return 0;
       return retErr(errno, dfltDest);
      }

// Caller want to send to a specific destination other than the default
//
   XrdNetAddr specDest;
   int retc;

   if (specDest.Set(dest))
      {eDest->Emsg("NetMsg", dest, "is unreachable"); return -1;}

// Create the message via the msghdr
//
   struct msghdr mHdr{};

   mHdr.msg_name = (void*)specDest.SockAddr();
   mHdr.msg_namelen = specDest.SockSize();
   mHdr.msg_iov = const_cast<struct iovec*>(iov);
   mHdr.msg_iovlen = iovcnt;

// Handle timeout if need be  
//
   if (tmo >= 0 && !OK2Send(tmo, dest)) return 1;

// Send the message
//
   do {retc = sendmsg(FD, &mHdr, 0);} while (retc < 0 && errno == EINTR);

// All done
//
   return (retc < 0 ? retErr(errno, specDest) : 0);
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                               O K 2 S e n d                                */
/******************************************************************************/
  
int XrdNetMsg::OK2Send(int timeout, const char *dest)
{
   struct pollfd polltab = {FD, POLLOUT|POLLWRNORM, 0};
   int retc;

   do {retc = poll(&polltab, 1, timeout);} while(retc < 0 && errno == EINTR);

   if (retc == 0 || !(polltab.revents & (POLLOUT | POLLWRNORM)))
      eDest->Emsg("NetMsg", "UDP link to", dest, "is blocked.");
      else if (retc < 0)
              eDest->Emsg("NetMsg",errno,"poll", dest);
              else return 1;
   return 0;
}
  
/******************************************************************************/
/*                                r e t E r r                                 */
/******************************************************************************/

int XrdNetMsg::retErr(int ecode, const char *theDest)
{
   if (!theDest)
      {if (!destOK)
          {eDest->Emsg("NetMsg", "Destination not specified."); return -1;}
       theDest = dfltDest;
      }
   eDest->Emsg("NetMsg", ecode, "send to", theDest);
   return (EWOULDBLOCK == ecode || EAGAIN == ecode ? 1 : -1);
}
  
int XrdNetMsg::retErr(int ecode, XrdNetAddr& theDest)
{
   return retErr(ecode, theDest.Name("unknown"));
}
