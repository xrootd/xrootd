#ifndef __XRDRMC_HH__
#define __XRDRMC_HH__
/******************************************************************************/
/*                                                                            */
/*                             X r d R m c . h h                              */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOuc/XrdOucCache.hh"
  
/*! The class defined here implements a general memory cache for data from an
    arbitrary source (e.g. files, sockets, etc). It is based on the abstract
    definition of a cache. Use the Create() method to create instances of a
    cache. There can be many such instances. Each instance is associated with
    one or more XrdOucCacheIO objects (see the XrdOucCache::Attach() method).

    Notes: 1. The minimum PageSize is 4096 (4k) and must be a power of 2.
              The maximum PageSize is 16MB.
           2. The size of the cache is forced to be a multiple PageSize and
              have a minimum size of PageSize * 256.
           3. The minimum external read size is equal to PageSize.
           4. Currently, only write-through caches are supported.
           5. The Max2Cache value avoids placing data in the cache when a read
              exceeds the specified value. The minimum allowed is PageSize, which
              is also the default.
           6. Structured file optimization allows pages whose bytes have been
              fully referenced to be discarded; effectively increasing the cache.
           7. A structured cache treats all files as structured. By default, the
              cache treats files as unstructured. You can over-ride the settings
              on an individual file basis when the file's I/O object is attached
              by passing the XrdOucCache::optFIS option, if needed.
           8. Write-in caches are only supported for files attached with the
              XrdOucCache::optWIN setting. Otherwise, updates are handled
              with write-through operations.
           9. A cache object may be deleted. However, the deletion is delayed
              until all CacheIO objects attached to the cache are detached.
          10. The default maximum attached files is set to 8192 when isServer
              has been specified. Otherwise, it is set at 256.
          11. When canPreRead is specified, the cache asynchronously handles
              preread requests (see XrdOucCacheIO::Preread()) using 9 threads
              when isServer is in effect. Otherwise, 3 threads are used.
          12. The max queue depth for prereads is 8. When the max is exceeded
              the oldest preread is discarded to make room for the newest one.
          13. If you specify the canPreRead option when creating the cache you
              can also enable automatic prereads if the algorithm is workable.
              Otherwise, you will need to implement your own algorithm and
              issue prereads manually using the XrdOucCacheIO::Preread() method.
          14. The automatic preread algorithm is (see aprParms):
              a) A preread operation occurs when all of the following conditions
                 are satisfied:
                 o The cache CanPreRead option is in effect.
                 o The read length < 'miniRead'
                    ||(read length < 'maxiRead' && Offset == next maxi offset)
              b) The preread page count is set to be readlen/pagesize and the
                 preread occurs at the page after read_offset+readlen. The page
                 is adjusted, as follows:
                 o If the count is < minPages, it is set to minPages.
                 o The count must be > 0 at this point.
              c) Normally, pre-read pages participate in the LRU scheme. However,
                 if the preread was triggered using 'maxiRead' then the pages are
                 marked for single use only. This means that the moment data is
                 delivered from the page, the page is recycled.
          15. Invalid options silently force the use of the default.
*/

class XrdRmc
{
public:

//-----------------------------------------------------------------------------
//! Parameters for a newly created memory cache.
//-----------------------------------------------------------------------------

struct Parms
      {long long CacheSize; //!< Size of cache in bytes     (default 100MB)
       int       PageSize;  //!< Size of each page in bytes (default 32KB)
       int       Max2Cache; //!< Largest read to cache      (default PageSize)
       int       MaxFiles;  //!< Maximum number of files    (default 256 or 8K)
       int       Options;   //!< Options as defined below   (default r/o cache)
       short     minPages;  //!< Minimum number of pages    (default 256)
       short     Reserve1;  //!< Reserved for future use
       int       Reserve2;  //!< Reserved for future use

                 Parms() : CacheSize(104857600), PageSize(32768),
                           Max2Cache(0), MaxFiles(0), Options(0),
                           minPages(0), Reserve1(0),  Reserve2(0) {}
      };

// Valid option values in Parms::Options
//
static const int
isServer     = 0x0010; //!< This is server application; not a user application.
                       // Appropriate internal optimizations will be used.
static const int
isStructured = 0x0020; // Optimize for structured files (e.g. root).

static const int
canPreRead   = 0x0040; //!< Enable pre-read operations (o/w ignored)

static const int
logStats     = 0x0080; //!< Display statistics upon detach

static const int
Serialized   = 0x0004; //!< Caller ensures MRSW semantics

static const int
ioMTSafe     = 0x0008; //!< CacheIO object is MT-safe

static const int
Debug        = 0x0003; //!< Produce some debug messages (levels 0, 1, 2, or 3)

//-----------------------------------------------------------------------------
//! Create an instance of a memory cache.
//!
//! @param  Reference to mandatory cache parameters.
//! @param  Optional pointer to default automatic preread parameters.
//!
//! @return Success: a pointer to a new instance of the cache.
//!         Failure: a null pointer is returned and errno set to the reason.
//-----------------------------------------------------------------------------

static XrdOucCache *Create(Parms &Params, XrdOucCacheIO::aprParms *aprP=0);

                    XrdRmc() {}
                   ~XrdRmc() {}
};
#endif
