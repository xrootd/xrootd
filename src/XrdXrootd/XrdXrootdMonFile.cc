/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d M o n F i l e . c c                    */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>

#include "Xrd/XrdScheduler.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "XrdXrootd/XrdXrootdMonFile.hh"
#include "XrdXrootd/XrdXrootdFileStats.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdXrootdMonInfo
{
extern long long mySID;
}

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
                          
XrdSysError         *XrdXrootdMonFile::eDest    = 0;
XrdScheduler        *XrdXrootdMonFile::Sched    = 0;
XrdSysMutex          XrdXrootdMonFile::bfMutex;
XrdSysMutex          XrdXrootdMonFile::fmMutex;
XrdXrootdMonFMap     XrdXrootdMonFile::fmMap[XrdXrootdMonFMap::mapNum];
short                XrdXrootdMonFile::fmUse[XrdXrootdMonFMap::mapNum] = {0};
char                *XrdXrootdMonFile::repBuff  = 0;
XrdXrootdMonHeader  *XrdXrootdMonFile::repHdr   = 0;
XrdXrootdMonFileTOD *XrdXrootdMonFile::repTOD   = 0;
char                *XrdXrootdMonFile::repNext  = 0;
char                *XrdXrootdMonFile::repFirst = 0;
char                *XrdXrootdMonFile::repLast  = 0;
int                  XrdXrootdMonFile::totRecs  = 0;
int                  XrdXrootdMonFile::xfrRecs  = 0;
int                  XrdXrootdMonFile::repSize  = 0;
int                  XrdXrootdMonFile::repTime  = 0;
int                  XrdXrootdMonFile::fmHWM    =-1;
int                  XrdXrootdMonFile::crecSize = 0;
int                  XrdXrootdMonFile::xfrCnt   = 0;
int                  XrdXrootdMonFile::xfrRem   = 0;
XrdXrootdMonFileXFR  XrdXrootdMonFile::xfrRec;
short                XrdXrootdMonFile::crecNLen = 0;
short                XrdXrootdMonFile::trecNLen = 0;
char                 XrdXrootdMonFile::fsLFN    = 0;
char                 XrdXrootdMonFile::fsLVL    = 0;
char                 XrdXrootdMonFile::fsOPS    = 0;
char                 XrdXrootdMonFile::fsSSQ    = 0;
char                 XrdXrootdMonFile::fsXFR    = 0;
char                 XrdXrootdMonFile::crecFlag = 0;
  
/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

void XrdXrootdMonFile::Close(XrdXrootdFileStats *fsP, bool isDisc)
{
   XrdXrootdMonFileCLS cRec;
   char *cP;
   int iEnt, iMap, iSlot;

// If this object was registered for I/O reporting, deregister it.
//
   if (fsP->MonEnt != -1)
      {iEnt = fsP->MonEnt & 0xffff;
       iMap  = iEnt >> XrdXrootdMonFMap::fmShft;
       iSlot = iEnt &  XrdXrootdMonFMap::fmMask;
       fsP->MonEnt = -1;
       fmMutex.Lock();
       if (fmMap[iMap].Free(iSlot)) fmUse[iMap]--;
       if (iMap == fmHWM) while(fmHWM >= 0 && !fmUse[fmHWM]) fmHWM--;
       fmMutex.UnLock();
      }

// Insert a close record header (mostly precomputed)
//
   cRec.Hdr.recType = XrdXrootdMonFileHdr::isClose;
   cRec.Hdr.recFlag = crecFlag;
   if (isDisc) cRec.Hdr.recFlag |= XrdXrootdMonFileHdr::forced;
   cRec.Hdr.recSize = crecNLen;
   cRec.Hdr.fileID  = fsP->FileID;

// Insert the I/O bytes
//
   cRec.Xfr.read  = htonll(fsP->xfr.read);
   cRec.Xfr.readv = htonll(fsP->xfr.readv);
   cRec.Xfr.write = htonll(fsP->xfr.write);

// Insert ops if so wanted
//
   if (fsOPS)
      {cRec.Ops.read  = htonl (fsP->ops.read);
       if (fsP->ops.read)
          {cRec.Ops.rdMin = htonl (fsP->ops.rdMin);
           cRec.Ops.rdMax = htonl (fsP->ops.rdMax);
          } else {
           cRec.Ops.rdMin = cRec.Ops.rdMax = 0;
          }
       cRec.Ops.readv = htonl (fsP->ops.readv);
       cRec.Ops.rsegs = htonll(fsP->ops.rsegs);
       if (fsP->ops.readv)
          {cRec.Ops.rsMin = htons (fsP->ops.rsMin);
           cRec.Ops.rsMax = htons (fsP->ops.rsMax);
           cRec.Ops.rvMin = htonl (fsP->ops.rvMin);
           cRec.Ops.rvMax = htonl (fsP->ops.rvMax);
          } else {
           cRec.Ops.rsMin = cRec.Ops.rsMax = 0;
           cRec.Ops.rvMin = cRec.Ops.rvMax = 0;
          }
       cRec.Ops.write = htonl (fsP->ops.write);
       if (fsP->ops.write)
          {cRec.Ops.wrMin = htonl (fsP->ops.wrMin);
           cRec.Ops.wrMax = htonl (fsP->ops.wrMax);
          } else {
           cRec.Ops.wrMin = cRec.Ops.wrMax = 0;
          }
      }

// Record sum of squares if so needed
//
   if (fsSSQ)
      {XrdXrootdMonDouble xval;
       xval.dreal           = fsP->ssq.read;
       cRec.Ssq.read.dlong  = htonll(xval.dlong);
       xval.dreal           = fsP->ssq.readv;
       cRec.Ssq.readv.dlong = htonll(xval.dlong);
       xval.dreal           = fsP->ssq.rsegs;
       cRec.Ssq.rsegs.dlong = htonll(xval.dlong);
       xval.dreal           = fsP->ssq.write;
       cRec.Ssq.write.dlong = htonll(xval.dlong);
      }

// Get a pointer to the next slot (the buffer gets locked)
//
   cP = GetSlot(crecSize);
   memcpy(cP, &cRec, crecSize);
   bfMutex.UnLock();
}

/******************************************************************************/
/*                              D e f a u l t s                               */
/******************************************************************************/
  
void XrdXrootdMonFile::Defaults(int intv, int opts, int xfrcnt)
{

// Set the reporting interval and I/O counter
//
   repTime = intv;
   xfrCnt  = xfrcnt;
   xfrRem  = xfrcnt;

// Expand out the options
//
   fsXFR  = (opts &  XROOTD_MON_FSXFR) != 0;
   fsLFN  = (opts &  XROOTD_MON_FSLFN) != 0;
   fsOPS  = (opts & (XROOTD_MON_FSOPS  | XROOTD_MON_FSSSQ)) != 0;
   fsSSQ  = (opts &  XROOTD_MON_FSSSQ) != 0;

// Set monitoring level
//
        if (fsSSQ) fsLVL = XrdXrootdFileStats::monSsq;
   else if (fsOPS) fsLVL = XrdXrootdFileStats::monOps;
   else if (intv)  fsLVL = XrdXrootdFileStats::monOn;
   else            fsLVL = XrdXrootdFileStats::monOff;
}

/******************************************************************************/
/*                                  D i s c                                   */
/******************************************************************************/
  
void XrdXrootdMonFile::Disc(unsigned int usrID)
{
   static short drecSize = htons(sizeof(XrdXrootdMonFileDSC));
   XrdXrootdMonFileDSC *dP;

// Get a pointer to the next slot (the buffer gets locked)
//
   dP = (XrdXrootdMonFileDSC *)GetSlot(sizeof(XrdXrootdMonFileDSC));

// Fill out the record. It's pretty simple
//
   dP->Hdr.recType = XrdXrootdMonFileHdr::isDisc;
   dP->Hdr.recFlag = 0;
   dP->Hdr.recSize = drecSize;
   dP->Hdr.userID  = usrID;
   bfMutex.UnLock();
}
  
/******************************************************************************/
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdXrootdMonFile::DoIt()
{

// First check if we need to report all the I/O stats
//
   xfrRem--;
   if (!xfrRem) DoXFR();

// Check if we should flush the buffer
//
   bfMutex.Lock();
   if (repNext) Flush();
   bfMutex.UnLock();

// Reschedule ourselves
//
   XrdXrootdMonitor::Sched->Schedule((XrdJob *)this, time(0)+repTime);
}

/******************************************************************************/
/* Private:                        D o X F R                                  */
/******************************************************************************/
  
void XrdXrootdMonFile::DoXFR()
{
   XrdXrootdFileStats *fsP;
   int keep, i, n, hwm;

// Reset interval counter
//
   xfrRem = xfrCnt;

// Grab the high watermark once
//
   fmMutex.Lock();
   hwm = fmHWM;
   fmMutex.UnLock();

// Report on all the files we have registered. This is a CPU burner as we
// periodically drop the lock to allow open/close requests to come through.
//
   for (i = 0; i <= hwm; i++)
       {fmMutex.Lock();
        if (fmUse[i])
           {n    = 0;
            keep = XrdXrootdMonFMap::fmHold;
            while((fsP = fmMap[i].Next(n)))
                 {if (fsP->xfrXeq) DoXFR(fsP);
                  if (!keep--)
                     {fmMutex.UnLock();
                      keep = XrdXrootdMonFMap::fmHold;
                      fmMutex.Lock();
                     }
                 }
           }
        fmMutex.UnLock();
       }
}

/******************************************************************************/

void XrdXrootdMonFile::DoXFR(XrdXrootdFileStats *fsP)
{
   long long xfrRead, xfrReadv, xfrWrite;
   char *cP;

// Turn off the activity flag
//
   fsP->xfrXeq = 0;

// Grab the I/O bytes to get a somewhat consistent image here
//
   xfrRead  = fsP->xfr.read;
   xfrReadv = fsP->xfr.readv;
   xfrWrite = fsP->xfr.write;

// Complete the record
//
   xfrRec.Hdr.fileID = fsP->FileID;
   xfrRec.Xfr.read   = htonll(xfrRead);
   xfrRec.Xfr.readv  = htonll(xfrReadv);
   xfrRec.Xfr.write  = htonll(xfrWrite);

// Get a pointer to the next slot (the buffer gets locked)
//
   cP = GetSlot(sizeof(xfrRec));
   memcpy(cP, &xfrRec, sizeof(xfrRec));
   xfrRecs++;
   bfMutex.UnLock();
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
bool XrdXrootdMonFile::Init(XrdScheduler *sp, XrdSysError  *errp, int bfsz)
{
   XrdXrootdMonFile *mfP;
   int alignment, pagsz = getpagesize();

// Set the variables
//
   Sched = sp;
   eDest = errp;

// Allocate a socket buffer
//
   alignment = (bfsz < pagsz ? 1024 : pagsz);
   if (posix_memalign((void **)&repBuff, alignment, bfsz))
      {eDest->Emsg("MonFile", "Unable to allocate monitor buffer.");
       return false;
      }

// Set the header (always present)
//
   repHdr = (XrdXrootdMonHeader *)repBuff;
   repHdr->code = XROOTD_MON_MAPFSTA;
   repHdr->pseq = 0;
   repHdr->stod = XrdXrootdMonitor::startTime;

// Set the time record (always present)
//
   repTOD   = (XrdXrootdMonFileTOD *)(repBuff + sizeof(XrdXrootdMonHeader));
   repTOD->Hdr.recType = XrdXrootdMonFileHdr::isTime;
   repTOD->Hdr.recFlag = XrdXrootdMonFileHdr::hasSID;
   repTOD->Hdr.recSize = htons(sizeof(XrdXrootdMonFileTOD));
   repTOD->sID = static_cast<kXR_int64>(XrdXrootdMonInfo::mySID);

// Establish first real record in the buffer (always fixed)
//
   repFirst = repBuff+sizeof(XrdXrootdMonHeader)+sizeof(XrdXrootdMonFileTOD);

// Calculate the end nut the next slot always starts with a null pointer
//
   repLast = repBuff+bfsz-1;
   repNext = 0;

// Calculate the close record size and the initial flags
//
   crecSize = sizeof(XrdXrootdMonFileHdr) + sizeof(XrdXrootdMonStatXFR);
   if (fsSSQ || fsOPS)
      {crecSize += sizeof(XrdXrootdMonStatOPS);
       crecFlag = XrdXrootdMonFileHdr::hasOPS;
      } else crecFlag = 0;
   if (fsSSQ)
      {crecSize += sizeof(XrdXrootdMonStatSSQ);
       crecFlag |= XrdXrootdMonFileHdr::hasSSQ;
      }
   crecNLen = htons(static_cast<short>(crecSize));

// Preformat the i/o record
//
   xfrRec.Hdr.recType = XrdXrootdMonFileHdr::isXfr;
   xfrRec.Hdr.recFlag = 0;
   xfrRec.Hdr.recSize = htons(static_cast<short>(sizeof(xfrRec)));

// Calculate the tod record size
//
   trecNLen = htons(static_cast<short>(sizeof(XrdXrootdMonFileTOD)));

// Allocate an instance of ourselves so we can schedule ourselves
//
   mfP = new XrdXrootdMonFile();

// Schedule an the flushes
//
   XrdXrootdMonitor::Sched->Schedule((XrdJob *)mfP, time(0)+repTime);
   return true;
}
  
/******************************************************************************/
/* Private:                        F l u s h                                  */
/******************************************************************************/

void XrdXrootdMonFile::Flush() // The bfMutex must be locked
{
   static int seq = 0;
   int bfSize;

// Update the sequence number
//
   repHdr->pseq = static_cast<char>(0x00ff & seq++);

// Insert ending timestamp and record counts
//
   repTOD->Hdr.nRecs[0] = htons(static_cast<short>(xfrRecs));
   repTOD->Hdr.nRecs[1] = htons(static_cast<short>(totRecs));
   repTOD->tEnd = htonl(static_cast<int>(time(0)));

// Calculate buffer size and stick into the header
//
   bfSize = (repNext - repBuff);
   repHdr->plen = htons(static_cast<short>(bfSize));
   repNext = 0;

// Write this out
//
   XrdXrootdMonitor::Send(XROOTD_MON_FSTA, repBuff, bfSize);
   repTOD->tBeg = repTOD->tEnd;
   xfrRecs = totRecs = 0;
}

/******************************************************************************/
/* Private:                      G e t S l o t                                */
/******************************************************************************/
  
char *XrdXrootdMonFile::GetSlot(int slotSZ)
{
   char *myRec;

// Lock this code to prevent interference (we should use double buffering)
// Note that the caller must do the unlock when finished with the slot.
//
   bfMutex.Lock();

// Check if we need to flush the buffer (sets repNext to zero). Otherwise,
// if this is the first record insert a timestamp.
//
   if (repNext)
      {if ((repNext + slotSZ) > repLast)
          {Flush();
           repNext = repFirst;
          }
      } else {
       repTOD->tBeg = htonl(static_cast<int>(time(0)));
       repNext = repFirst;
      }

// Return the slot
//
   totRecs++;
   myRec = repNext;
   repNext += slotSZ;
   return myRec;
}

/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
void XrdXrootdMonFile::Open(XrdXrootdFileStats *fsP, const char *Path,
                            unsigned int uDID, bool isRW)
{
   static const int minRecSz = sizeof(XrdXrootdMonFileOPN)
                             - sizeof(XrdXrootdMonFileLFN);
   XrdXrootdMonFileOPN *oP;
   int i = 0, sNum = -1, rLen, pLen = 0;

// Assign the path a dictionary id if not assigned via file monitoring
//
   if (fsP->FileID == 0) fsP->FileID = XrdXrootdMonitor::GetDictID();

// Add this open to the map table if we are doing I/O stats.
//
   if (fsXFR)
      {fmMutex.Lock();
       for (i = 0; i < XrdXrootdMonFMap::mapNum; i++)
           if (fmUse[i] < XrdXrootdMonFMap::fmSize)
              {if ((sNum = fmMap[i].Insert(fsP)) >= 0)
                  {fmUse[i]++;
                   if (i > fmHWM) fmHWM = i;
                   break;
                  }
              }
       fmMutex.UnLock();
      }

// Generate the cookie (real or virtual) to find the entry in the map table.
// Supply the monitoring options for effeciency.
//
   fsP->MonEnt = (sNum | (i << XrdXrootdMonFMap::fmShft)) & 0xffff;
   fsP->monLvl = fsLVL;
   fsP->xfrXeq = 0;

// Compute the size of this record
//
   rLen = minRecSz;
   if (fsLFN)
      {pLen  = strlen(Path);
       rLen += sizeof(kXR_unt32) + pLen;
       i     = (rLen + 8) & ~0x00000003;
       pLen  = pLen + (i - rLen);
       rLen  = i;
      }

// Get a pointer to the next slot (the buffer gets locked)
//
   oP = (XrdXrootdMonFileOPN *)GetSlot(rLen);

// Fill out the record
//
   oP->Hdr.recType = XrdXrootdMonFileHdr::isOpen;
   oP->Hdr.recFlag = (isRW ?  XrdXrootdMonFileHdr::hasRW : 0);
   oP->Hdr.recSize = htons(static_cast<short>(rLen));
   oP->Hdr.fileID  = fsP->FileID;
   oP->fsz         = htonll(fsP->fSize);

// Append user and path if so wanted (sizes have been verified)
//
   if (fsLFN)
      {oP->Hdr.recFlag |= XrdXrootdMonFileHdr::hasLFN;
       oP->ufn.user = uDID;
       strncpy(oP->ufn.lfn, Path, pLen);
      }
   bfMutex.UnLock();
}
