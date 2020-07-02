#ifndef __XRDXROOTDMONFILE__
#define __XRDXROOTDMONFILE__
/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d M o n F i l e . h h                    */
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

#include "Xrd/XrdJob.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdXrootd/XrdXrootdMonFMap.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"

class XrdXrootdFileStats;
class XrdXrootdMonHeader;
class XrdXrootdMonTrace;
  
class XrdXrootdMonFile : XrdJob
{
public:

static void Close(XrdXrootdFileStats *fsP, bool isDisc=false);

static void Defaults(int intv, int opts, int iocnt, int fbsz);

static void Disc(unsigned int usrID);

       void DoIt();

static bool Init();

static void Open(XrdXrootdFileStats *fsP,
                 const char *Path, unsigned int uDID, bool isRW);

       XrdXrootdMonFile() : XrdJob("monitor fstat") {}
      ~XrdXrootdMonFile() {}

private:

static void                 DoXFR();
static void                 DoXFR(XrdXrootdFileStats *fsP);
static void                 Flush();
static char                *GetSlot(int slotSZ);
                          
static XrdSysMutex          bfMutex;
static XrdSysMutex          fmMutex;
static XrdXrootdMonFMap     fmMap[XrdXrootdMonFMap::mapNum];
static short                fmUse[XrdXrootdMonFMap::mapNum];
static char                *repBuff;
static XrdXrootdMonHeader  *repHdr;
static XrdXrootdMonFileTOD *repTOD;
static char                *repNext;
static char                *repFirst;
static char                *repLast;
static int                  totRecs;
static int                  xfrRecs;
static int                  repSize;
static int                  repTime;
static int                  fmHWM;
static int                  crecSize;
static int                  xfrCnt;
static int                  fBsz;
static int                  xfrRem;
static XrdXrootdMonFileXFR  xfrRec;
static short                crecNLen;
static short                trecNLen;
static char                 fsLFN;
static char                 fsLVL;
static char                 fsOPS;
static char                 fsSSQ;
static char                 fsXFR;
static char                 crecFlag;
};
#endif
