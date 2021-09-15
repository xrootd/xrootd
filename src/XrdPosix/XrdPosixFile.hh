#ifndef __XRDPOSIXFILE_HH__
#define __XRDPOSIXFILE_HH__
/******************************************************************************/
/*                                                                            */
/*                       X r d P o s i x F i l e . h h                        */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <fcntl.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/uio.h>

#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include "XrdOuc/XrdOucCache.hh"

#include "XrdPosix/XrdPosixMap.hh"
#include "XrdPosix/XrdPosixObject.hh"

/******************************************************************************/
/*                    X r d P o s i x F i l e   C l a s s                     */
/******************************************************************************/

class XrdPosixCallBack;
class XrdPosixPrepIO;

class XrdPosixFile : public XrdPosixObject, 
                     public XrdOucCacheIO,
                     public XrdOucCacheIOCD,
                     public XrdCl::ResponseHandler
{
public:

XrdOucCacheIO  *XCio;
XrdPosixPrepIO *PrepIO;
XrdCl::File     clFile;

       long long     addOffset(long long offs, int updtSz=0)
                              {updMutex.Lock();
                               currOffset += offs;
                               if (updtSz && currOffset > (long long)mySize)
                                  mySize = currOffset;
                               long long retOffset = currOffset;
                               updMutex.UnLock();
                               return retOffset;
                              }

//atic XrdPosixFile *Alloc(const char *path, XrdPosixCallBack *cbP, int Opts);

static void*         DelayedDestroy(void*);

static void          DelayedDestroy(XrdPosixFile *fp);

       bool          Close(XrdCl::XRootDStatus &Status);

       bool          Detach(XrdOucCacheIOCD &cdP) override
                           {(void)cdP; return true;}

       void          DetachDone() override {unRef();}

       bool          Finalize(XrdCl::XRootDStatus *Status);

       long long     FSize() override
                          {AtomicBeg(updMutex);
                           long long retSize = AtomicGet(mySize);
                           AtomicEnd(updMutex);
                           return retSize;
                          }

       int           Fstat(struct stat &buf) override;

       const char   *Location(bool refresh=false) override;

       void          HandleResponse(XrdCl::XRootDStatus *status,
                                    XrdCl::AnyObject    *response) override;

       void          updLock()   {updMutex.Lock();}

       void          updUnLock() {updMutex.UnLock();}

       long long     Offset() {AtomicRet(updMutex, currOffset);}

       const char   *Origin() {return fOpen;}

       const char   *Path() override {return fPath;}

       int           pgRead(char *buff, long long offs, int rdlen,
                            std::vector<uint32_t> &csvec, uint64_t opts=0,
                            int *csfix=0) override;

       void          pgRead(XrdOucCacheIOCB &iocb,
                            char *buff, long long offs, int rdlen,
                            std::vector<uint32_t> &csvec, uint64_t opts=0,
                            int *csfix=0) override;

        int          pgWrite(char *buff, long long offs, int wrlen,
                             std::vector<uint32_t> &csvec, uint64_t opts=0,
                             int *csfix=0) override;

        void         pgWrite(XrdOucCacheIOCB &iocb,
                             char *buff, long long offs, int wrlen,
                             std::vector<uint32_t> &csvec, uint64_t opts=0,
                             int *csfix=0) override;

       int           Read (char *Buff, long long Offs, int Len) override;

       void          Read (XrdOucCacheIOCB &iocb, char *buff, long long offs,
                           int rlen) override;

       int           ReadV (const XrdOucIOVec *readV, int n) override;

       void          ReadV (XrdOucCacheIOCB &iocb, const XrdOucIOVec *readV,
                            int n) override;

inline long long     setOffset(long long offs)
                              {updMutex.Lock();
                               currOffset = offs;
                               updMutex.UnLock();
                               return offs;
                              }

       bool          Stat(XrdCl::XRootDStatus &Status, bool force=false);

       int           Sync() override;

       void          Sync(XrdOucCacheIOCB &iocb) override;

       int           Trunc(long long Offset) override;

inline void          UpdtSize(size_t newsz)
                              {updMutex.Lock();
                               if (newsz > mySize) mySize = newsz;
                               updMutex.UnLock();
                              }

       using         XrdPosixObject::Who;

inline bool          Who(XrdPosixFile **fileP)
                          {*fileP = this; return true;}

       int           Write(char *Buff, long long Offs, int Len) override;

       void          Write(XrdOucCacheIOCB &iocb, char *buff, long long offs,
                           int wlen) override;

       size_t        mySize;
       time_t        myAtime;
       time_t        myCtime;
       time_t        myMtime;
       dev_t         myRdev;
       ino_t         myInode;
       mode_t        myMode;

static
XrdSysSemaphore      ddSem;
static XrdSysMutex   ddMutex;
static XrdPosixFile *ddList;
static XrdPosixFile *ddLost;
static char         *sfSFX;
static short         sfSLN;
static bool          ddPosted;
static int           ddNum;

static const int realFD = 1;
static const int isStrm = 2;
static const int isUpdt = 4;

           XrdPosixFile(bool &aOK, const char *path, XrdPosixCallBack *cbP=0,
                        int   Opts=0);
          ~XrdPosixFile();

private:

union {long long         currOffset;
       XrdPosixCallBack *theCB;
       XrdPosixFile     *nextFile;
      };

char       *fPath;
char       *fOpen;
char       *fLoc;
union {int  cOpt; int numTries;};
char        isStream;
};
#endif
