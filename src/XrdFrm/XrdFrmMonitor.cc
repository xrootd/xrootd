/******************************************************************************/
/*                                                                            */
/*                      X r d F r m M o n i t o r . c c                       */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/types.h>

#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrm/XrdFrmMonitor.hh"
#include "XrdNet/XrdNetMsg.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"

using namespace XrdFrc;

/******************************************************************************/
/*                     S t a t i c   A l l o c a t i o n                      */
/******************************************************************************/
  
char              *XrdFrmMonitor::Dest1      = 0;
int                XrdFrmMonitor::monMode1   = 0;
XrdNetMsg         *XrdFrmMonitor::InetDest1  = 0;
char              *XrdFrmMonitor::Dest2      = 0;
int                XrdFrmMonitor::monMode2   = 0;
XrdNetMsg         *XrdFrmMonitor::InetDest2  = 0;
kXR_int32          XrdFrmMonitor::startTime  = 0;
int                XrdFrmMonitor::isEnabled  = 0;
char              *XrdFrmMonitor::idRec      = 0;
int                XrdFrmMonitor::idLen      = 0;
int                XrdFrmMonitor::sidSize    = 0;
char              *XrdFrmMonitor::sidName    = 0;
int                XrdFrmMonitor::idTime     = 3600;
char               XrdFrmMonitor::monMIGR    = 0;
char               XrdFrmMonitor::monPURGE   = 0;
char               XrdFrmMonitor::monSTAGE   = 0;

/******************************************************************************/
/*                     T h r e a d   I n t e r f a c e s                      */
/******************************************************************************/

void *XrdFrmMonitorID(void *parg)
{
   (void)parg;
   XrdFrmMonitor::Ident();
   return (void *)0;
}
  
/******************************************************************************/
/*                              D e f a u l t s                               */
/******************************************************************************/

void XrdFrmMonitor::Defaults(char *dest1, int mode1, char *dest2, int mode2,
                             int iTime)
{

// Make sure if we have a proper destinations and modes
//
   if (dest1 && !mode1) {free(dest1); dest1 = 0; mode1 = 0;}
   if (dest2 && !mode2) {free(dest2); dest2 = 0; mode2 = 0;}

// Propogate the destinations
//
   if (!dest1)
      {mode1 = (dest1 = dest2) ? mode2 : 0;
       dest2 = 0; mode2 = 0;
      }

// Set the default destinations (caller supplied strdup'd strings)
//
   if (Dest1) free(Dest1);
   Dest1 = dest1; monMode1 = mode1;
   if (Dest2) free(Dest2);
   Dest2 = dest2; monMode2 = mode2;

// Set overall monitor mode
//
   monMIGR   = ((mode1 | mode2) & XROOTD_MON_MIGR  ? 1 : 0);
   monPURGE  = ((mode1 | mode2) & XROOTD_MON_PURGE ? 1 : 0);
   monSTAGE  = ((mode1 | mode2) & XROOTD_MON_STAGE ? 1 : 0);

// Do final check
//
   isEnabled = (Dest1 == 0 && Dest2 == 0 ? 0 : 1);
   idTime = iTime;
}

/******************************************************************************/
/*                                 I d e n t                                  */
/******************************************************************************/
  
void XrdFrmMonitor::Ident()
{
do{Send(-1, idRec, idLen);
   XrdSysTimer::Snooze(idTime);
  } while(1);
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
int XrdFrmMonitor::Init(const char *iHost, const char *iProg, const char *iName)
{
   XrdXrootdMonMap *mP;
   long long  mySid;
   const char *etext = 0;
   char        iBuff[1024];
   bool       aOK;

// Generate our server ID
//
   sidName = XrdOucUtils::Ident(mySid, iBuff, sizeof(iBuff), iHost, iProg,
                                (iName ? iName : "anon"), 0);
   sidSize = strlen(sidName);
   startTime = htonl(time(0));

// There is nothing to do unless we have been enabled via Defaults()
//
   if (!isEnabled) return 1;

// Ignore array bounds warning from gcc 12 triggered because the allocated
// memory for the XrdXrootdMonMap is smaller than sizeof(XrdXrootdMonMap)
#if defined(__GNUC__) && __GNUC__ >= 12
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
// Create identification record
//
   idLen = strlen(iBuff) + sizeof(XrdXrootdMonHeader) + sizeof(kXR_int32);
   idRec = (char *)malloc(idLen+1);
   mP = (XrdXrootdMonMap *)idRec;
   fillHeader(&(mP->hdr), XROOTD_MON_MAPIDNT, idLen);
   mP->hdr.pseq = 0;
   mP->dictid   = 0;
   strcpy(mP->info, iBuff);
#if defined(__GNUC__) && __GNUC__ >= 12
#pragma GCC diagnostic pop
#endif

// Setup the primary destination
//
   InetDest1 = new XrdNetMsg(&Say, Dest1, &aOK);
   if (!aOK)
      {Say.Emsg("Monitor", "setup monitor collector;", etext);
       return 0;
      }

// Do the same for the secondary destination
//
   if (Dest2)
      {InetDest2 = new XrdNetMsg(&Say, Dest2, &aOK);
       if (!aOK)
          {Say.Emsg("Monitor", "setup monitor collector;", etext);
           return 0;
          }
      }

// Check if we will be producing identification records
//
   if (idTime)
      {pthread_t tid;
       int retc;
       if ((retc = XrdSysThread::Run(&tid,XrdFrmMonitorID,0,0,"mon ident")))
          {Say.Emsg("Init", retc, "create monitor ident thread"); return 0;}
      }

// All done
//
   return 1;
}

/******************************************************************************/
/*                                   M a p                                    */
/******************************************************************************/
  
kXR_unt32 XrdFrmMonitor::Map(char code, const char *uname, const char *path)
{
   XrdXrootdMonMap     map;
   const char *colonP, *atP;
   char uBuff[1024];
   int  size, montype;

// Decode the user name as a.b:c@d
//
   if ((colonP = index(uname, ':')) && (atP = index(colonP+1, '@')))
      {int n = colonP - uname + 1;
       strncpy(uBuff, uname, n);
       strcpy(uBuff+n, sidName);
       strcpy(uBuff+n+sidSize, atP);
      } else strcpy(uBuff, uname);

// Copy in the username and path the dictid is always zero for us.
//
   map.dictid = 0;
   strcpy(map.info, uBuff);
   size = strlen(uBuff);
   if (path)
      {*(map.info+size) = '\n';
       strlcpy(map.info+size+1, path, sizeof(map.info)-size-1);
       size = size + strlen(path) + 1;
      }

// Route the packet to all destinations that need them
//
        if (code == XROOTD_MON_MAPSTAG){montype = XROOTD_MON_STAGE;
                                        code    = XROOTD_MON_MAPXFER;
                                       }
   else if (code == XROOTD_MON_MAPMIGR){montype = XROOTD_MON_MIGR;
                                        code    = XROOTD_MON_MAPXFER;
                                       }
   else if (code == XROOTD_MON_MAPPURG) montype = XROOTD_MON_PURGE;
   else                                 montype = XROOTD_MON_INFO;

// Fill in the header and route the packet
//
   size = sizeof(XrdXrootdMonHeader)+sizeof(kXR_int32)+size;
   fillHeader(&map.hdr, code, size);
// std::cerr <<"Mon send "<<code <<": " <<map.info <<std::endl;
   Send(montype, (void *)&map, size);

// Return the dictionary id
//
   return map.dictid;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                            f i l l H e a d e r                             */
/******************************************************************************/
  
void XrdFrmMonitor::fillHeader(XrdXrootdMonHeader *hdr,
                               const char id, int size)
{  static XrdSysMutex seqMutex;
   static int         seq = 0;
          int         myseq;

// Generate a new sequence number
//
   seqMutex.Lock();
   myseq = 0x00ff & (seq++);
   seqMutex.UnLock();

// Fill in the header
//
   hdr->code = static_cast<kXR_char>(id);
   hdr->pseq = static_cast<kXR_char>(myseq);
   hdr->plen = htons(static_cast<uint16_t>(size));
   hdr->stod = startTime;
}
 
/******************************************************************************/
/*                                  S e n d                                   */
/******************************************************************************/
  
int XrdFrmMonitor::Send(int monMode, void *buff, int blen)
{
    EPNAME("Send");
    static XrdSysMutex sendMutex;
    int rc1, rc2;
    sendMutex.Lock();
    if (monMode & monMode1 && InetDest1)
       {rc1  = InetDest1->Send((char *)buff, blen);
        DEBUG(blen <<" bytes sent to " <<Dest1 <<" rc=" <<rc1);
       }
       else rc1 = 0;
    if (monMode & monMode2 && InetDest2)
       {rc2  = InetDest2->Send((char *)buff, blen);
        DEBUG(blen <<" bytes sent to " <<Dest2 <<" rc=" <<rc2);
       }
       else rc2 = 0;
    sendMutex.UnLock();

    return (rc1 ? rc1 : rc2);
}
