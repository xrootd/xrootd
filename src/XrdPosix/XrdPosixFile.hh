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

#include <errno.h>
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

class XrdPosixFile : public XrdPosixObject, 
                     public XrdOucCacheIO,
                     public XrdCl::ResponseHandler
{
public:

XrdOucCacheIO *XCio;
XrdCl::File    clFile;

       long long     addOffset(long long offs, int updtSz=0)
                              {currOffset += offs;
                               if (updtSz && currOffset > (long long)mySize)
                                  mySize = currOffset;
                               return currOffset;
                              }

static XrdPosixFile *Alloc(const char *path, XrdPosixCallBack *cbP, int Opts);

       bool          Close(XrdCl::XRootDStatus &Status);

       bool          Finalize(XrdCl::XRootDStatus &Status);

       long long     FSize() {return static_cast<long long>(mySize);}

       void          HandleResponse(XrdCl::XRootDStatus *status,
                                    XrdCl::AnyObject    *response);

       void          isOpen();

       long long     Offset() {return currOffset;}

       const char   *Path();

       int           Read (char *Buff, long long Offs, int Len);

       int           ReadV (const XrdOucIOVec *readV, int n);

       long long     setOffset(long long offs)
                              {currOffset = offs;
                               return currOffset;
                              }

       bool          Stat(XrdCl::XRootDStatus &Status, bool force=false);

       int           Sync() {return XrdPosixMap::Result(clFile.Sync());}

       int           Trunc(long long Offset)
                          {return XrdPosixMap::Result(clFile.Truncate((uint64_t)Offset));}

       using         XrdPosixObject::Who;

       bool          Who(XrdPosixFile **fileP)
                          {*fileP = this; return true;}

       int           Write(char *Buff, long long Offs, int Len)
                          {return XrdPosixMap::Result(clFile.Write((uint64_t)Offs,
                                                         (uint32_t)Len, Buff));
                          }

       size_t        mySize;
       time_t        myMtime;
       ino_t         myInode;
       mode_t        myMode;

static XrdOucCache *CacheR;
static XrdOucCache *CacheW;
static char        *sfSFX;
static int          sfSLN;

static const int realFD = 1;
static const int isStrm = 2;
static const int isUpdt = 4;

           XrdPosixFile(const char *path, XrdPosixCallBack *cbP=0, int Opts=0);
          ~XrdPosixFile();

private:

union {long long         currOffset;
       XrdPosixCallBack *theCB;
      };

char       *fPath;
int         cOpt;
char        isStream;
};
#endif
