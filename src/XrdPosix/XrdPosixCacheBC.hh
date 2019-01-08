#ifndef __XRDPOSIXCACHEBC_HH__
#define __XRDPOSIXCACHEBC_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d P o s i x C a c h e B C . h h                     */
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

#include "XrdOuc/XrdOucCache2.hh"

/******************************************************************************/
/*                     X r d P o s i x C a c h e B C I O                      */
/******************************************************************************/
  
class XrdPosixCacheBCIO : public XrdOucCacheIO2
{
public:

virtual
XrdOucCacheIO2  *Base()   {return cacheIO2;}

virtual
XrdOucCacheIO2 *Detach() {XrdOucCacheIO2 *theCIO = cacheIO2;
                          cacheIO1->Detach();
                          delete this;
                          return theCIO;
                         }

virtual
long long       FSize() {return cacheIO1->FSize();}

virtual int     Fstat(struct stat &buf) {return cacheIO2->Fstat(buf);}

virtual
const char     *Location() {return cacheIO2->Location();}

virtual
const char     *Path()  {return cacheIO1->Path();}

using XrdOucCacheIO2::Read;

virtual int     Read (char *Buffer, long long Offset, int Length)
                      {return cacheIO1->Read(Buffer, Offset, Length);}

using XrdOucCacheIO2::ReadV;

virtual int     ReadV(const XrdOucIOVec *readV, int n)
                      {return cacheIO1->ReadV(readV, n);}

using XrdOucCacheIO2::Sync;

virtual int     Sync() {return cacheIO1->Sync();}

using XrdOucCacheIO2::Trunc;

virtual int     Trunc(long long Offset) {return cacheIO1->Trunc(Offset);}

using XrdOucCacheIO2::Write;

virtual int     Write(char *Buffer, long long Offset, int Length)
                     {return cacheIO1->Write(Buffer, Offset, Length);}

virtual bool    ioActive() { return cacheIO1->ioActive();}

virtual void    Preread (long long Offset, int Length, int Opts=0)
                        {return cacheIO1->Preread(Offset, Length, Opts);}

virtual void    Preread(aprParms &Parms) { cacheIO1->Preread(Parms);}

                XrdPosixCacheBCIO(XrdOucCacheIO *urCIO, XrdOucCacheIO2 *myCIO)
                                 : cacheIO1(urCIO), cacheIO2(myCIO) {}
virtual        ~XrdPosixCacheBCIO() {}

private:
XrdOucCacheIO  *cacheIO1;
XrdOucCacheIO2 *cacheIO2;
};

/******************************************************************************/
/*                       X r d P o s i x C a c h e B C                        */
/******************************************************************************/
  
class XrdPosixCacheBC : public XrdOucCache2
{
public:
using XrdOucCache2::Attach;

virtual
XrdOucCacheIO2 *Attach(XrdOucCacheIO2 *ioP, int opts=0)
                      {XrdOucCacheIO *newIOP = v1Cache->Attach(ioP, opts);
                       if (newIOP == (XrdOucCacheIO *)ioP) return ioP;
                       return new XrdPosixCacheBCIO(newIOP, ioP);
                      }

virtual int     isAttached() {return v1Cache->isAttached();}

virtual int     Rmdir(const char* path) {return v1Cache->Rmdir(path);}

virtual int     Rename(const char* pathO, const char* pathN)
                      {return v1Cache->Rename(pathO, pathN);}

virtual int     Truncate(const char* path, off_t size)
                        {return v1Cache->Truncate(path, size);}

virtual int     Unlink(const char* path) {return v1Cache->Unlink(path);}

                XrdPosixCacheBC(XrdOucCache *cP) : v1Cache(cP) {}
virtual        ~XrdPosixCacheBC() {}
private:
XrdOucCache    *v1Cache;
};
#endif
