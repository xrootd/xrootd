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
   //! \brief Downloads original file into a single file on local disk.
   //! Handles read requests as they come along.
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
         //! Pass Read request to the corresponding Prefetch object.
         //!
         //! @param Buffer
         //! @param Offset
         //! @param Length
         //!
         //! @return number of bytes read
         //---------------------------------------------------------------------
         virtual int Read(char *Buffer, long long Offset, int Length);

         //---------------------------------------------------------------------
         //! Pass ReadV request to the corresponding Prefetch object.
         //!
         //! @param readV
         //! @param n number of XrdOucIOVecs
         //!
         //! @return total bytes read
         //---------------------------------------------------------------------
         virtual int ReadV(const XrdOucIOVec *readV, int n);

         //---------------------------------------------------------------------
         //! Detach itself from Cache. Note: this will delete the object.
         //!
         //! @return original source \ref XrdPosixFile
         //---------------------------------------------------------------------
         virtual XrdOucCacheIO* Detach();

         //! \brief Virtual method of XrdOucCacheIO. 
         //! Called to check if destruction needs to be done in a separate task.
         virtual bool ioActive();

   protected:
      //! Run prefetch outside constructor.
      virtual void StartPrefetch();

      private:
         Prefetch* m_prefetch;
   };

}
#endif
