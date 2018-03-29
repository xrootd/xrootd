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

#include <errno.h>
#include <sys/poll.h>

#include "XrdNet/XrdNet.hh"
#include "XrdNet/XrdNetMsg.hh"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdNetMsg::XrdNetMsg(XrdSysError *erp, const char *dest, bool *aOK)
{
   XrdNet myNet(erp);
   bool   aok = true;

   eDest = erp; FD = -1; destOK = 0;
   if (dest)
      {const char *eText = dfltDest.Set(dest);
       if (!eText) destOK = 1;
          else {eDest->Emsg("Msg", "Default", dest, "is unreachable");
                aok = false;
               }
      }

    if ((FD = myNet.Relay(dest)) < 0)
       {eDest->Emsg("Msg", "Unable to create UDP msg socket.");
        aok = false;
       }

    if (aOK) *aOK = aok;
}

/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/
  
int XrdNetMsg::Send(const char *Buff, int Blen, const char *dest, int tmo)
{
   XrdNetAddr *theDest;
   int retc;

   if (!Blen && !(Blen = strlen(Buff))) return  0;

   if (!dest)
       {if (!destOK)
           {eDest->Emsg("Msg", "Destination not specified."); return -1;}
        theDest = &dfltDest;
       }
      else if (specDest.Set(dest))
              {eDest->Emsg("Msg", dest, "is unreachable");    return -1;}
              else theDest = &specDest;

   if (tmo >= 0 && !OK2Send(tmo, dest)) return 1;

   do {retc = sendto(FD, (Sokdata_t)Buff, Blen, 0, 
                     theDest->SockAddr(), theDest->SockSize());}
       while (retc < 0 && errno == EINTR);

   return (retc < 0 ? retErr(errno, theDest) : 0);
}

/******************************************************************************/

int XrdNetMsg::Send(const struct iovec iov[], int iovcnt, 
                    const char  *dest,        int tmo)
{
   char buff[4096], *bp = buff;
   int i, dsz = sizeof(buff);

   if (tmo >= 0 && !OK2Send(tmo, dest)) return 1;

   for (i = 0; i < iovcnt; i++)
       {dsz -= iov[i].iov_len;
        if (dsz < 0) return retErr(EMSGSIZE, dest);
        memcpy((void *)bp,(const void *)iov[i].iov_base,iov[i].iov_len);
        bp += iov[i].iov_len;
       }

   return Send(buff, (int)(bp-buff), dest, -1);
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
      eDest->Emsg("Msg", "UDP link to", dest, "is blocked.");
      else if (retc < 0)
              eDest->Emsg("Msg",errno,"poll", dest);
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
          {eDest->Emsg("Msg", "Destination not specified."); return -1;}
       theDest = dfltDest.Name("unknown");
      }
   eDest->Emsg("Msg", ecode, "send to", theDest);
   return (EWOULDBLOCK == ecode || EAGAIN == ecode ? 1 : -1);
}
  
int XrdNetMsg::retErr(int ecode, XrdNetAddr *theDest)
{
   return retErr(ecode, theDest->Name("unknown"));
}
