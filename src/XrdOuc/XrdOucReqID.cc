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
#include <cstdio>
#include <cstring>
#ifndef WIN32
#include <strings.h>
#else
#include "XrdSys/XrdWin32.hh"
#endif
#include <ctime>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
  
#include "XrdOucReqID.hh"

#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucCRC.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOucReqID::XrdOucReqID()
{
   char xbuff[256];
   int eNow = static_cast<int>(time(0)), myPid = static_cast<int>(getpid());

// Now format the formatting template
//
   snprintf(xbuff, sizeof(xbuff)-1, "%08X:%08x.%%d", myPid, eNow);
   reqFMT    = strdup(xbuff);
   xbuff[8]  = 0;
   reqPFX    = strdup(xbuff);
   reqPFXlen = 8;
   reqIntern = 0;
   reqNum    = 0;
}

/******************************************************************************/
  
XrdOucReqID::XrdOucReqID(const XrdNetSockAddr *myAddr, int myPort)
{
   char ybuff[256], xbuff[512];
   unsigned int pHash;
   int n, eNow = static_cast<unsigned int>(time(0));

// Encode our address as the prefix
//
   if ( (n = XrdNetUtils::Encode(myAddr, ybuff, sizeof(ybuff), myPort)) <= 0)
      n = sprintf(ybuff, "%04X%08X", myPort, eNow);
   reqPFX    = strdup(ybuff);
   reqPFXlen = n;
   reqIntern = n+1;

// Generate out hash
//
   pHash = XrdOucCRC::CRC32((const unsigned char *)ybuff, n);

// Now format the formatting template
//
   snprintf(xbuff, sizeof(xbuff)-1, "%s:%08x.%08x:%%d", ybuff, pHash, eNow);
   reqFMT = strdup(xbuff);
   reqNum = 0;
}
 
/******************************************************************************/
/*                                i s M i n e                                 */
/******************************************************************************/
 
char *XrdOucReqID::isMine(char *reqid, int &hport, char *hname, int hlen)
{
   XrdNetAddr theAddr;
   XrdNetSockAddr IP;
   const char *theHost;
   int thePort;
   char *cp;

// Determine whether this is our host
//
   if (!strncmp(reqPFX,reqid,reqPFXlen) && (cp = index(reqid,':'))) return cp+1;

// Not ours, try to tell the caller who it is
//
   hport = 0;
   if (!hlen) return 0;

// Get the IP address of his id
//
   thePort = XrdNetUtils::Decode(&IP, reqid, reqPFXlen);
   if (thePort <= 0) return 0;

// Convert this in the appropriate way
//
   if (theAddr.Set(&IP.Addr)
   ||  !(theHost = theAddr.Name())
   ||  strlen(theHost) >= (unsigned int)hlen) return 0;

// Return the alternate host
//
   strcpy(hname, theHost);
   hport = thePort;
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
   return buff+reqIntern;
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
