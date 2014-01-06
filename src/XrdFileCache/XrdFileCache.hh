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

#ifndef __XRDFILECACHE_CACHE_HH__
#define __XRDFILECACHE_CACHE_HH__
/******************************************************************************/
/*                                                                            */
/* (c) 2012 University of Nebraksa-Lincoln                                    */
/*     by Brian Bockelman                                                     */
/*                                                                            */
/******************************************************************************/

#include <XrdSys/XrdSysPthread.hh>
#include <XrdOuc/XrdOucCache.hh>


namespace XrdFileCache
{

extern const char* InfoExt;
extern const int InfoExtLen;
extern const bool IODisablePrefetch;
extern const long long PrefetchDefaultBufferSize;

class Cache : public XrdOucCache
{

    friend class IOEntire;
    friend class IOBlocks;
    friend class Factory;

public:

    XrdOucCacheIO *Attach(XrdOucCacheIO *, int Options=0);

    int isAttached();

    virtual XrdOucCache*
    Create(XrdOucCache::Parms&, XrdOucCacheIO::aprParms*) {return NULL; }


    Cache(XrdOucCacheStats&);

private:

    void Detach(XrdOucCacheIO *);

    static Cache *m_cache;
    static XrdSysMutex m_cache_mutex;

    XrdSysMutex m_io_mutex;
    unsigned int m_attached;

    XrdOucCacheStats & m_stats;
};

}

#endif
