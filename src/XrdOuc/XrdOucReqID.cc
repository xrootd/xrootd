/******************************************************************************/
/*                                                                            */
/*                        X r d O u c R e q I D . c c                         */
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

#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifndef WIN32
#include <strings.h>
#else
#include "XrdSys/XrdWin32.hh"
#endif
#include <time.h>
#include <sys/types.h>
  
#include "XrdOucReqID.hh"

#include "XrdOuc/XrdOucCRC.hh"

/******************************************************************************/
/*                      S t a t i c   V a r i a b l e s                       */
/******************************************************************************/
  
XrdSysMutex  XrdOucReqID::myMutex;
char        *XrdOucReqID::reqFMT;
char        *XrdOucReqID::reqPFX;
int          XrdOucReqID::reqPFXlen = 0;
int          XrdOucReqID::reqNum = 0;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOucReqID::XrdOucReqID(int inst, const char *myHost, unsigned int myIP)
{
   time_t eNow = time(0);
   char xbuff[256];

   snprintf(xbuff, sizeof(xbuff)-1, "%08x:%04x.%08x:%%d", myIP, inst,
                                    static_cast<unsigned int>(eNow));
   reqFMT = strdup(xbuff);
   reqPFXlen = 13;
   xbuff[reqPFXlen] = '\0';
   reqPFX = strdup(xbuff);
}
 
/******************************************************************************/
/*                                i s M i n e                                 */
/******************************************************************************/
 
int XrdOucReqID::isMine(char *reqid, int &hport, char *hname, int hlen)
{
   unsigned int ipaddr, ipport;
   char *cp, *ep, *ip;

// Determine whether this is our host
//
   if (isMine(reqid)) return 1;

// Not ours, try to tell the caller who it is
//
   if (!hlen) return 0;

// Get the IP address of his id
//
   hport = 0;
   if (!(cp = index(reqid, int(':'))) || cp-reqid != 8) return 0;
   if (!(ipaddr = strtol(reqid, &ep, 16)) || ep != cp)  return 0;

// Get the port number
//
   ep++;
   if (!(cp = index(ep, int('.')))     || cp-ep != 4) return 0;
   if (!(ipport = strtol(ep, &cp, 16)) || ep != cp)   return 0;

// Format the address and return the port
//
   ip = (char *)&ipaddr;
   snprintf(hname, hlen-1, "%d.%d.%d.%d",
                   (int)ip[0], (int)ip[1], (int)ip[2], (int)ip[3]);
   hname[hlen-1] = '\0';
   hport = (int)ipport;
   return 0;
}
  
/******************************************************************************/
/*                                    I D                                     */
/******************************************************************************/
  
char *XrdOucReqID::ID(char *buff, int blen)
{
   int myNum;

// Get a new sequence number
//
   myMutex.Lock();
   myNum = (reqNum += 1);
   myMutex.UnLock();

// Generate the request id and return it
//
   snprintf(buff, blen-1, reqFMT, myNum);
   return buff;
}

/******************************************************************************/
/*                                 I n d e x                                  */
/******************************************************************************/
  
int XrdOucReqID::Index(int KeyMax, const char *KeyVal, int KeyLen)
{
   unsigned int pHash;

// Get hash value for the key and return modulo of the KeyMax value
//
   pHash = XrdOucCRC::CRC32((const unsigned char *)KeyVal,
                            (KeyLen ? KeyLen : strlen(KeyVal)));
   return (int)(pHash % KeyMax);
}
