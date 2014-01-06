#ifndef __XRDFILECACHE_IO_ENTIRE_HH__
#define __XRDFILECACHE_IO_ENTIRE_HH__
//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University  
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman           
//----------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------------------

/*
 * The XrdFileCacheIOEntire object is used as a proxy for the original source
 */

#include <string>

#include <XrdOuc/XrdOucCache.hh>
#include "XrdSys/XrdSysPthread.hh"

#include "XrdFileCachePrefetch.hh"
#include "XrdFileCache.hh"

class XrdSysError;
class XrdOssDF;
class XfcStats;

namespace XrdFileCache
{
class IOEntire : public XrdOucCacheIO
{
    friend class Cache;

public:

    XrdOucCacheIO *
    Base() {return &m_io; }

    virtual XrdOucCacheIO *Detach();

    long long
    FSize() {return m_io.FSize(); }

    const char *
    Path() {return m_io.Path(); }

    int Read (char  *Buffer, long long Offset, int Length);

#if defined(HAVE_READV)
    virtual int  ReadV (const XrdOucIOEntireVec *readV, int n);

#endif

   // AMT i guess this are virtuals ...
    int Sync() {return 0; }

    int Trunc(long long Offset) { errno = ENOTSUP; return -1; }

    int Write(char *Buffer, long long Offset, int Length) { errno = ENOTSUP; return -1; }
 
protected:
    IOEntire(XrdOucCacheIO &io, XrdOucCacheStats &stats, Cache &cache);

private:
   ~IOEntire();
 
    XrdOucCacheIO & m_io;
    XrdOucCacheStats & m_statsGlobal;
    Cache & m_cache;
    Prefetch* m_prefetch;
};

}
#endif
