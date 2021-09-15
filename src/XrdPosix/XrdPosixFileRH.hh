#ifndef __POSIX_FILERH_HH__
#define __POSIX_FILERH_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d P o s i x F l e R H . h h                       */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdint>
#include <vector>

#include "Xrd/XrdJob.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdOucCacheIOCB;
class XrdPosixFile;

/******************************************************************************/
/*                        X r d P o s i x F i l e R H                         */
/******************************************************************************/
  
class XrdPosixFileRH : public XrdJob,
                       public XrdCl::ResponseHandler
{
public:

enum ioType {nonIO = 0, isRead = 1, isReadV = 2, isWrite = 3,
                                    isReadP = 4, isWriteP= 5};

static XrdPosixFileRH  *Alloc(XrdOucCacheIOCB *cbp, XrdPosixFile *fp,
                              long long offs, int xResult, ioType typeIO);

        void            DoIt() {theCB->Done(result); Recycle();}

        void            HandleResponse(XrdCl::XRootDStatus *status,
                                       XrdCl::AnyObject    *response);

        void            Recycle();

inline  void            setCSVec(std::vector<uint32_t> *csv, int *csf,
                                 bool fcs=false)
                                {csVec = csv; csfix = csf; csFrc = fcs;}

static  void            SetMax(int mval) {maxFree = mval;}

        void            Sched(int result);

private:
             XrdPosixFileRH() : theCB(0), theFile(0), csVec(0), csfix(0),
                                result(0),typeIO(nonIO), csFrc(false) {}
virtual     ~XrdPosixFileRH() {}

static  XrdSysMutex      myMutex;
static  XrdPosixFileRH  *freeRH;
static  int              numFree;
static  int              maxFree;

union  {XrdOucCacheIOCB *theCB;
        XrdPosixFileRH  *next;
       };
XrdPosixFile            *theFile;
std::vector<uint32_t>   *csVec;
int                     *csfix;
long long                offset;
int                      result;
ioType                   typeIO;
bool                     csFrc;
};
#endif
