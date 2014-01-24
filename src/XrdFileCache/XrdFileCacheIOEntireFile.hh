#ifndef __XRDFILECACHE_IO_ENTIRE_FILE_HH__
#define __XRDFILECACHE_IO_ENTIRE_FILE_HH__
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

#include "XrdSys/XrdSysPthread.hh"
#include "XrdFileCache.hh"
#include "XrdFileCacheStats.hh"
#include "XrdFileCachePrefetch.hh"

class XrdSysError;
class XrdOssDF;
class XfcStats;
class XrdOucIOVec;

namespace XrdFileCache
{
   //----------------------------------------------------------------------------
   //! Downloads original data into single file. Handles read requests.
   //----------------------------------------------------------------------------
   class IOEntireFile : public IO
   {
      public:
         //------------------------------------------------------------------------
         //! Constructor
         //------------------------------------------------------------------------      
         IOEntireFile(XrdOucCacheIO &io, XrdOucCacheStats &stats, Cache &cache);

         //------------------------------------------------------------------------
         //! Destructor
         //------------------------------------------------------------------------ 
         ~IOEntireFile();

         //---------------------------------------------------------------------
         //!\brief Read Pass read request to Prefetch and wait task is completed
         //!
         //! @param Buffer
         //! @param Offset
         //! @param Length 
         //!
         //! @return number of bytes read
         //---------------------------------------------------------------------
         virtual int Read (char  *Buffer, long long Offset, int Length);

         //---------------------------------------------------------------------
         //!\brief ReadV pass vector reads to Prefetch.
         //!
         //! @param readV
         //! @param n number of XrdOucIOVecs
         //!
         //! @return total bytes read
         //---------------------------------------------------------------------
         virtual int  ReadV (const XrdOucIOVec *readV, int n);

         //---------------------------------------------------------------------
         //!\brief Detach itself from Cache. Note this will delete this object.
         //!
         //!
         //! @return original source \ref XrdPosixFile
         //---------------------------------------------------------------------
         virtual XrdOucCacheIO* Detach();

      private:
         Prefetch* m_prefetch;
   };

}
#endif
