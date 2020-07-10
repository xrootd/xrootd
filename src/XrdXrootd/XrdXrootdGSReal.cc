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

#include <stdio.h>
#include <string.h>

#include "Xrd/XrdScheduler.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdXrootd/XrdXrootdGSReal.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdXrootdMonInfo
{
extern XrdScheduler *Sched;
extern char         *monHost;
extern long long     mySID;
extern int           startTime;
}

using namespace XrdXrootdMonInfo;
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdXrootdGSReal::XrdXrootdGSReal(const char *gNamePI, char gDataID,
                                 int mtype, int flint)
                                : XrdJob("GStream"), XrdXrootdGStream(*this)
{
   char idBuff[1024];

// Initialze the udp buffer
//
   memset(&gMsg.info, 0, sizeof(gMsg.info));
   gMsg.info.hdr.code = XROOTD_MON_MAPGSTA;
   gMsg.info.hdr.stod = startTime;

   long long theSID = ntohll(mySID) & 0x00ffffffffffffff;
   gMsg.info.sID = htonll(theSID | (static_cast<long long>(gDataID) << 56));

// Setup buffer pointers
//
   udpBFirst = udpBNext = gMsg.buff;
   udpBEnd = gMsg.buff + sizeof(gMsg.buff) - 1;

// Initialize remaining variables
//
   monType = mtype;
   rsvbytes = 0;
   afRunning = false;
   SetAutoFlush(flint);

// Construct our user name as in <gNamePI>.0:0@<myhost>
//
   snprintf(idBuff, sizeof(idBuff), "%s.0:0@%s", gNamePI, monHost);

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
      {if (gMsg.info.tBeg && time(0)-gMsg.info.tBeg >= afTime) Expel(0);
       AutoFlush();
      }
}

/******************************************************************************/
/* Private:                        E x p e l                                  */
/******************************************************************************/

void XrdXrootdGSReal::Expel(int dlen) // gMutex is held
{
   int size;

// Check if we need to flush this buffer.
//
   if (udpBFirst == udpBNext || (dlen && (udpBNext + dlen) < udpBEnd)) return;

// Complete the buffer
//
   size =  udpBNext-(char *)&gMsg;
   gMsg.info.hdr.pseq++;
   gMsg.info.hdr.plen = htons(static_cast<uint16_t>(size));
   *(udpBNext-1) = 0;

// Send off the packet
//
   XrdXrootdMonitor::Send(monType, &gMsg, size, false);

// Reset the buffer
//
   udpBNext = udpBFirst;
   gMsg.info.tBeg = gMsg.info.tEnd = 0;
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
// Record the mapping and return it
//
   return (isPath ? gMon.MapPath(text) : gMon.MapInfo(text));
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
   gMsg.info.tEnd = time(0);
   if (udpBNext == udpBFirst) gMsg.info.tBeg = gMsg.info.tEnd;
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
   gMsg.info.tEnd = time(0);
   if (udpBNext == udpBFirst) gMsg.info.tBeg = gMsg.info.tEnd;
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
