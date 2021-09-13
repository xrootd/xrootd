/******************************************************************************/
/*                                                                            */
/*                    X r d X r o o t d G S R e a l . h h                     */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/uio.h>

#include "Xrd/XrdScheduler.hh"
#include "XrdNet/XrdNetMsg.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdXrootd/XrdXrootdGSReal.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdXrootdMonInfo
{
extern XrdScheduler *Sched;
extern XrdSysError  *eDest;
extern char         *monHost;
extern char         *kySID;
extern long long     mySID;
extern int           startTime;

extern char         *SidCGI[4];
extern char         *SidJSON[4];
}

using namespace XrdXrootdMonInfo;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdXrootdGSReal::XrdXrootdGSReal(const XrdXrootdGSReal::GSParms &gsParms,
                                 bool &aOK)
                                : XrdJob("GStream"), XrdXrootdGStream(*this),
                                  Hello(gsParms.Opt & XrdXrootdGSReal::optNoID
                                    || gsParms.Hdr == XrdXrootdGSReal::hdrNone
                                     ? 0 : gsParms.dest, gsParms.Fmt),
                                  pSeq(0), pSeqID(0), pSeqDID(0), binHdr(0),
                                  isCGI(false)
{
   static const int minSZ = 1024;
   static const int dflSZ = 1024*32;
   static const int maxSZ = 1024*64;
   int flsT, maxL, hdrLen;

// Do common initialization
//
   memset(&hInfo, 0, sizeof(hInfo));
   aOK = true;

// Compute the correct size of the UDP buffer
//
   if (gsParms.maxL <= 0) maxL = dflSZ;
      else if (gsParms.maxL < minSZ) maxL = minSZ;
              else if (gsParms.maxL > maxSZ) maxL = maxSZ;
                      else maxL = gsParms.maxL;
   maxL &= ~7; // Doubleword lengths

// Allocate the UDP buffer. Try to keep the data within a single page.
//
   int align;
   if (maxL >= getpagesize())         align = getpagesize();
      else if (maxL >= 2048)          align = 2048;
              else if (maxL >= 1024)  align = 1024;
                      else            align = sizeof(void*);

   if (posix_memalign((void **)&udpBuffer, align, maxL)) {aOK = false; return;}

// Setup the header as needed
//
   if (gsParms.Hdr == hdrNone)
      {hdrLen = 0;
       binHdr = 0;
       dictHdr = idntHdr0 = idntHdr1 = 0;
      } else {
       switch(gsParms.Fmt)
             {case fmtBin:  hdrLen = hdrBIN(gsParms);
                            break;
              case fmtCgi:  hdrLen = hdrCGI(gsParms, udpBuffer, maxL);
                            break;
              case fmtJson: hdrLen = hdrJSN(gsParms, udpBuffer, maxL);
                            break;
              default:      hdrLen = 0;
             }
       if (gsParms.Opt & optNoID)
          {if (idntHdr0) {free(idntHdr0); idntHdr0 = 0;}
           if (idntHdr1) {free(idntHdr1); idntHdr1 = 0;}
          }
      }

// Setup buffer pointers
//
   udpBFirst = udpBNext = udpBuffer + hdrLen;
   udpBEnd = udpBuffer + maxL - 1;

   tBeg = tEnd = afTime = 0;

// Initialize remaining variables
//
   monType = gsParms.Mode;
   rsvbytes = 0;

// If we have a specific end-point, then create a network relay to it
//
   if (gsParms.dest) udpDest = new XrdNetMsg(eDest, gsParms.dest, &aOK);
      else udpDest = 0;

// Setup autoflush (a negative value uses the default)
//
   if (gsParms.flsT < 0) flsT = XrdXrootdMonitor::Flushing();
      else flsT = gsParms.flsT;
   afRunning = false;
   SetAutoFlush(flsT);

// Construct our user name as in <gNamePI>.0:0@<myhost>
//
   char idBuff[1024];
   snprintf(idBuff, sizeof(idBuff), "%s.0:0@%s", gsParms.pin, monHost);

// Register ourselves
//
   gMon.Register(idBuff, monHost, "xroot");
}

/******************************************************************************/
/*                             A u t o F l u s h                              */
/******************************************************************************/

void XrdXrootdGSReal::AutoFlush() // gMutex is locked outside constructor
{
   if (afTime && !afRunning)
      {Sched->Schedule((XrdJob *)this, time(0)+afTime);
       afRunning = true;
      }
}
  
/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdXrootdGSReal::DoIt()
{
   XrdSysMutexHelper gHelp(gMutex);

// Check if we need to do anything here
//
   afRunning = false;
   if (afTime)
      {if (tBeg && time(0)-tBeg >= afTime) Expel(0);
       AutoFlush();
      }
}

/******************************************************************************/
/* Private:                        E x p e l                                  */
/******************************************************************************/

void XrdXrootdGSReal::Expel(int dlen) // gMutex is held
{

// Check if we need to flush this buffer.
//
   if (udpBFirst == udpBNext || (dlen && (udpBNext + dlen) < udpBEnd)) return;
   int size =  udpBNext-udpBuffer;

// Complete the buffer header if may be binary of text
//
   if (binHdr)
      {binHdr->hdr.pseq++;
       binHdr->hdr.plen = htons(static_cast<uint16_t>(size));
       binHdr->tBeg = htonl(tBeg);
       binHdr->tEnd = htonl(tEnd);
      } else {
       if (hInfo.pseq)
          {char tBuff[32];
           if (pSeq >= 999) pSeq = 0;
              else pSeq++;
           snprintf(tBuff, sizeof(tBuff), "%3d%10u%10u", pSeq,
                           (unsigned int)tBeg, (unsigned int)tEnd);
           if (isCGI)
              {char *plus, *bP = tBuff;
               while((plus = index(bP, ' '))) {*plus = '+'; bP = plus+1;}
              }
           memcpy(hInfo.pseq, tBuff,     3);
           memcpy(hInfo.tbeg, tBuff+ 3, 10);
           memcpy(hInfo.tend, tBuff+13, 10);
          }
      }

// Make sure the whole thing ends with a null byte
//
   *(udpBNext-1) = 0;

// Send off the packet
//
   if (udpDest) udpDest->Send(udpBuffer, size);
      else XrdXrootdMonitor::Send(monType, udpBuffer, size, false);

// Reset the buffer
//
   udpBNext = udpBFirst;
   tBeg = tEnd = 0;
}

/******************************************************************************/
/*                                 F l u s h                                  */
/******************************************************************************/

void XrdXrootdGSReal::Flush()
{
   XrdSysMutexHelper gHelp(gMutex);
   Expel(0);
}
  
/******************************************************************************/
/*                             G e t D i c t I D                              */
/******************************************************************************/
  
uint32_t XrdXrootdGSReal::GetDictID(const char *text, bool isPath)
{
// If this is binary encoded, the record the mapping and return it
//
   if (binHdr) return (isPath ? gMon.MapPath(text) : gMon.MapInfo(text));

// If there are no headers then we can't produce this record
//
   uint32_t psq, did = XrdXrootdMonitor::GetDictID(true);
   if (!dictHdr) return htonl(did);

// We need to do some additional work to generate non-binary headers here
//
   struct iovec iov[3];
   char dit = (isPath ? XROOTD_MON_MAPPATH : XROOTD_MON_MAPINFO);
   char buff[1024];

// Generate a new packet sequence number
//
   gMutex.Lock();
   if (pSeqDID >= 999) pSeqDID = 0;
      else pSeqDID++;
   psq = pSeqDID;
   gMutex.UnLock();

// Generate the packet
//
   iov[0].iov_base = buff;
   iov[0].iov_len  = snprintf(buff, sizeof(buff), dictHdr, dit, psq, did);
   iov[1].iov_base = (void *)text;
   iov[1].iov_len  = strlen(text);
   iov[2].iov_base = (void *)"\"}";
   iov[2].iov_len  = 3;

// Now send it off
//
   udpDest->Send(iov, (*dictHdr == '{' ? 3 : 2));
   return htonl(did);
}

/******************************************************************************/
/*                                H a s H d r                                 */
/******************************************************************************/

bool XrdXrootdGSReal::HasHdr()
{
   return binHdr != 0 || dictHdr != 0;
}
  
/******************************************************************************/
/* Private:                       h d r B I N                                 */
/******************************************************************************/

int XrdXrootdGSReal::hdrBIN(const XrdXrootdGSReal::GSParms &gs)
{

// Initialze the udp heaader in the buffer
//
   binHdr = (XrdXrootdMonGS*)udpBuffer;
   memset(binHdr, 0, sizeof(XrdXrootdMonGS));
   binHdr->hdr.code = XROOTD_MON_MAPGSTA;
   binHdr->hdr.stod = startTime;

   long long theSID = ntohll(mySID) & 0x00ffffffffffffff;
   theSID = theSID | (static_cast<long long>(gs.Type) << XROOTD_MON_PIDSHFT);
   binHdr->sID = htonll(theSID);

   return (int)sizeof(XrdXrootdMonGS);
}

/******************************************************************************/
/* Private:                       h d r C G I                                 */
/******************************************************************************/

int XrdXrootdGSReal::hdrCGI(const XrdXrootdGSReal::GSParms &gs,
                            char *buff, int blen)
{
   const char *hdr, *plug = "\n";
   char hBuff[2048];
   int n;

// Pick any needed extensions to this header
//
   switch(gs.Hdr)
         {case hdrSite: plug = SidCGI[0]; break;
          case hdrHost: plug = SidCGI[1]; break;
          case hdrInst: plug = SidCGI[2]; break;
          case hdrFull: plug = SidCGI[3]; break;
          default:      break;
         }

// Generate the header to use for 'd' or 'i' packets
//
   hdr = "code=%%c&pseq=%%u&stod=%u&sid=%s%s&gs.type=%c&did=%%u&data=";

   snprintf(hBuff, sizeof(hBuff), hdr, ntohl(startTime), kySID, plug, gs.Type);
   dictHdr = strdup(hBuff);

// Generate the headers to use for '=' packets. These have a changeable part
// and a non-changeable part.
//
   hdr = "code=%c&pseq=%%u";

   snprintf(hBuff, sizeof(hBuff), hdr, XROOTD_MON_MAPIDNT);
   idntHdr0 = strdup(hBuff);

   hdr = "&stod=%u&sid=%s%s";

   n = snprintf(hBuff, sizeof(hBuff), hdr, ntohl(startTime), kySID, SidCGI[3]);
   idntHdr1 = strdup(hBuff);
   idntHsz1 = n+1;

// Format the header
//
   hdr = "code=%c&pseq=$12&stod=%u&sid=%s%s&gs.type=%c"
         "&gs.tbeg=$123456789&gs.tend=$123456789%s\n";

   n = snprintf(buff, blen, hdr, XROOTD_MON_MAPGSTA, ntohl(startTime),
                                 kySID, plug, gs.Type);

// Return all of the substitution addresses
//
   hInfo.pseq = index(buff, '$');
   hInfo.tbeg = index(hInfo.pseq+1, '$');
   hInfo.tend = index(hInfo.tbeg+1, '$');

// Return the length
//
   isCGI = true;
   return n;
}

/******************************************************************************/
/* Private:                       h d r J S N                                 */
/******************************************************************************/
  
int XrdXrootdGSReal::hdrJSN(const XrdXrootdGSReal::GSParms &gs,
                            char *buff, int blen)
{
   const char *hdr, *plug1 = "", *plug2 = "";
   char hBuff[2048];
   int n;

// Add any needed extensions to this header
//
   if (gs.Hdr != hdrNorm)
      {plug1 = ",";
       switch(gs.Hdr)
             {case hdrSite: plug2 = SidJSON[0]; break;
              case hdrHost: plug2 = SidJSON[1]; break;
              case hdrInst: plug2 = SidJSON[2]; break;
              case hdrFull: plug2 = SidJSON[3]; break;
              default:      plug1 = ""; break;
             }
      }

// Generate the header to use for 'd' or 'i' packets
//
   hdr = "{\"code\":\"%%c\",\"pseq\":%%u,\"stod\":%u,\"sid\":%s%s%s,"
         "\"gs\":{\"type\":\"%c\"},\"did\":%%u,\"data\":\"";

   snprintf(hBuff, sizeof(hBuff), hdr, ntohl(startTime), kySID,
                                       plug1, plug2, gs.Type);
   dictHdr = strdup(hBuff);

// Generate the headers to use for '=' packets. These have a changeable part
// and a non-changeable part.
//
   hdr = "{\"code\":\"%c\",\"pseq\":%%u,";

   snprintf(hBuff, sizeof(hBuff), hdr, XROOTD_MON_MAPIDNT);
   idntHdr0 = strdup(hBuff);

   hdr = "\"stod\":%u,\"sid\":%s,%s}";

   n = snprintf(hBuff, sizeof(hBuff), hdr, ntohl(startTime), kySID, SidJSON[3]);
   idntHdr1 = strdup(hBuff);
   idntHsz1 = n+1;

// Generate the header of plugin output
//
   hdr = "{\"code\":\"%c\",\"pseq\":$12,\"stod\":%u,\"sid\":%s%s%s,"
         "\"gs\":{\"type\":\"%c\",\"tbeg\":$123456789,\"tend\":$123456789}}\n";

// Format the header (we are gauranteed to have at least 1024 bytes here)
//
   n = snprintf(buff, blen, hdr, XROOTD_MON_MAPGSTA, ntohl(startTime),
                                 kySID, plug1, plug2, gs.Type);

// Return all of the substitution addresses
//
   hInfo.pseq = index(buff, '$');
   hInfo.tbeg = index(hInfo.pseq+1, '$');
   hInfo.tend = index(hInfo.tbeg+1, '$');

// Return the length
//
   return n;
}
  
/******************************************************************************/
/*                                 I d e n t                                  */
/******************************************************************************/

void XrdXrootdGSReal::Ident()
{
   struct iovec iov[2];
   char buff[40];
   uint32_t psq;

// If identification suppressed, then just return
//
   if (!idntHdr0 || !udpDest) return;

// Generate a new packet sequence number
//
   gMutex.Lock();
   if (pSeqID >= 999) pSeqID = 0;
      else pSeqID++;
   psq = pSeqID;
   gMutex.UnLock();

// Create header and iovec to send the header
//
   iov[0].iov_base = buff;
   iov[0].iov_len  = snprintf(buff, sizeof(buff), idntHdr0, psq);
   iov[1].iov_base = idntHdr1;
   iov[1].iov_len  = idntHsz1;
   udpDest->Send(iov, 2);
}

/******************************************************************************/
/*                                I n s e r t                                 */
/******************************************************************************/
  
bool XrdXrootdGSReal::Insert(const char *data, int dlen)
{

// Validate the length and message
//
   if (dlen < 8 || dlen > XrdXrootdGStream::MaxDataLen
   ||  !data    || data[dlen-1]) return false;

// Reserve the storage and copy the message. It always will end with a newline
//
   gMutex.Lock();
   Expel(dlen);
   memcpy(udpBNext, data, dlen-1);
   udpBNext[dlen-1] = '\n';

// Timestamp the record and aAdjust buffer pointers
//
   tEnd = time(0);
   if (udpBNext == udpBFirst) tBeg = tEnd;
   udpBNext += dlen;

// All done
//
   gMutex.UnLock();
   return true;
}

/******************************************************************************/
  
bool XrdXrootdGSReal::Insert(int dlen)
{
   XrdSysMutexHelper gHelp(gMutex);

// Make sure space is reserved
//
   if (!rsvbytes) return false;

// We are now sure that the recursive lock is held twice by this thread. So,
// make it a unitary lock so it gets fully unlocked upon rturn.
//
   gMutex.UnLock();

// Check for cancellation
//
   if (!dlen)
      {rsvbytes = 0;
       return true;
      }

// Length, it must >= 8 and <= reserved amount and the data must end with a 0.
//
   if (dlen > rsvbytes || dlen < 8 || *(udpBNext+dlen-1))
      {rsvbytes = 0;
       return false;
      }

// Adjust the buffer space and time stamp the record
//
   tEnd = time(0);
   if (udpBNext == udpBFirst) tBeg = tEnd;
   udpBNext += dlen;
   *(udpBNext-1) = '\n';
   rsvbytes = 0;

// All done

   return true;
}

/******************************************************************************/
/*                               R e s e r v e                                */
/******************************************************************************/
  
char *XrdXrootdGSReal::Reserve(int dlen)
{
// Validate the length
//
   if (dlen < 8 || dlen > XrdXrootdGStream::MaxDataLen) return 0;

// Make sure there is no reserve outstanding
//
   gMutex.Lock();
   if (rsvbytes)
      {gMutex.UnLock();
       return 0;
      }

// Return the allocated the space but keep the lock until Insert() is called.
//
   rsvbytes = dlen;
   Expel(dlen);
   return udpBNext;
}

/******************************************************************************/
/*                          S e t A u t o F l u s h                           */
/******************************************************************************/
  
int XrdXrootdGSReal::SetAutoFlush(int afsec)
{
   XrdSysMutexHelper gHelp(gMutex);

// Save the current settting and establish the new one and relaunch
//
   int afNow = afTime;
   afTime = (afsec > 0 ? afsec : 0);
   AutoFlush();

// All done
//
   return afNow;
}

/******************************************************************************/
/*                                 S p a c e                                  */
/******************************************************************************/
  
int XrdXrootdGSReal::Space()
{
   XrdSysMutexHelper gHelp(gMutex);
  
// Return amount of space left
//
   return udpBEnd - udpBNext;
}
