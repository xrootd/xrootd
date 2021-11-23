#ifndef __CMS_CACHE__H
#define __CMS_CACHE__H
/******************************************************************************/
/*                                                                            */
/*                        X r d C m s C a c h e . h h                         */
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

#include <cstring>
  
#include "Xrd/XrdJob.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdCms/XrdCmsKey.hh"
#include "XrdCms/XrdCmsNash.hh"
#include "XrdCms/XrdCmsPList.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdCms/XrdCmsSelect.hh"
#include "XrdCms/XrdCmsTypes.hh"
  
class XrdCmsCache
{
public:
friend class XrdCmsCacheJob;

XrdCmsPList_Anchor Paths;

// AddFile() returns true if this is the first addition, false otherwise. See
//           method for detailed information on processing.
//
int         AddFile(XrdCmsSelect &Sel, SMask_t mask);

// DelFile() returns true if this is the last deletion, false otherwise
//
int         DelFile(XrdCmsSelect &Sel, SMask_t mask);

// GetFile() returns true if we actually found the file
//
int         GetFile(XrdCmsSelect &Sel, SMask_t mask);

// UnkFile() updates the unqueried vector and returns 1 upon success, 0 o/w.
//
int         UnkFile(XrdCmsSelect &Sel, SMask_t mask);

// WT4File() adds a request to the callback queue and returns a 0 if added
//           of a wait time to be returned to the client.
//
int         WT4File(XrdCmsSelect &Sel, SMask_t mask);

void        Bounce(SMask_t smask, int SNum);

void        Drop(SMask_t mask, int SNum, int xHi);

int         Init(int fxHold, int fxDelay, int fxQuery, int seFS, int nxHold);

void       *TickTock();

static const int min_nxTime = 60;

            XrdCmsCache() : okVec(0), Tick(8*60*60), Tock(0), BClock(0), 
                            nilTMO(0),
                            DLTime(5), QDelay(5), Bhits(0), Bmiss(0), vecHi(-1),
                            isDFS(0)
                          {memset(Bounced,  0, sizeof(Bounced));
                           memset(Bhistory, 0, sizeof(Bhistory));
                          }
           ~XrdCmsCache() {}   // Never gets deleted

private:

void          Add2Q(XrdCmsRRQInfo *Info, XrdCmsKeyItem *cp, int selOpts);
void          Dispatch(XrdCmsSelect &Sel, XrdCmsKeyItem *cinfo,
                       short roQ, short rwQ);
SMask_t       getBVec(unsigned int todA, unsigned int &todB);
void          Recycle(XrdCmsKeyItem *theList);

struct  {SMask_t      Vec;
         unsigned int Start;
         unsigned int End;
        }             Bhistory[XrdCmsKeyItem::TickRate];

XrdSysMutex   myMutex;
XrdCmsNash    CTable;
unsigned int  Bounced[STMax];
SMask_t       okVec;
unsigned int  Tick;
unsigned int  Tock;
unsigned int  BClock;
         int  nilTMO;
         int  DLTime;
         int  QDelay;
         int  Bhits;
         int  Bmiss;
         int  vecHi;
         int  isDFS;
};

namespace XrdCms
{
extern    XrdCmsCache Cache;
}
#endif
