#ifndef __XRDFILECACHE_CACHE_HH__
#define __XRDFILECACHE_CACHE_HH__
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
#include <string>
#include <XrdSys/XrdSysPthread.hh>
#include <XrdOuc/XrdOucCache.hh>

namespace XrdFileCache
{
class Cache : public XrdOucCache
{
    friend class IOEntire;
    friend class IOBlocks;

public:
    XrdOucCacheIO *Attach(XrdOucCacheIO *, int Options=0);

    int isAttached();

    virtual XrdOucCache*
    Create(XrdOucCache::Parms&, XrdOucCacheIO::aprParms*) {return NULL; }
    Cache(XrdOucCacheStats&);

    void TempDirCleanup();

private:

    void Detach(XrdOucCacheIO *);
    bool getFilePathFromURL(const char* url, std::string& res) const;

    XrdSysMutex m_io_mutex;
    unsigned int m_attached;

    XrdOucCacheStats & m_stats;

    bool m_disablePrefetch;
};

}

#endif
