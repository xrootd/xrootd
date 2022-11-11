//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef SRC_XRDEC_XRDECWRTBUFF_HH_
#define SRC_XRDEC_XRDECWRTBUFF_HH_

#include "XrdEc/XrdEcUtilities.hh"
#include "XrdEc/XrdEcObjCfg.hh"
#include "XrdEc/XrdEcConfig.hh"
#include "XrdEc/XrdEcThreadPool.hh"

#include "XrdCl/XrdClBuffer.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include "XrdOuc/XrdOucCRC32C.hh"

#include <vector>
#include <condition_variable>
#include <mutex>
#include <future>

namespace XrdEc
{
  //---------------------------------------------------------------------------
  //! Pool of buffer for caching writes
  //---------------------------------------------------------------------------
  class BufferPool
  {
    public:

      //-----------------------------------------------------------------------
      //! Singleton access to the object
      //-----------------------------------------------------------------------
      static BufferPool& Instance()
      {
        static BufferPool instance;
        return instance;
      }

      //-----------------------------------------------------------------------
      //! Create now buffer (or recycle existing one)
      //-----------------------------------------------------------------------
      XrdCl::Buffer Create( const ObjCfg &objcfg )
      {
        std::unique_lock<std::mutex> lck( mtx );
        //---------------------------------------------------------------------
        // If pool is not empty, recycle existing buffer
        //---------------------------------------------------------------------
        if( !pool.empty() )
        {
          XrdCl::Buffer buffer( std::move( pool.front() ) );
          pool.pop();
          return buffer;
        }
        //---------------------------------------------------------------------
        // Check if we can create a new buffer object without exceeding the
        // the maximum size of the pool
        //---------------------------------------------------------------------
        if( currentsize < totalsize )
        {
          XrdCl::Buffer buffer( objcfg.blksize );
          ++currentsize;
          return buffer;
        }
        //---------------------------------------------------------------------
        // If not, we have to wait until there is a buffer we can recycle
        //---------------------------------------------------------------------
        while( pool.empty() ) cv.wait( lck );
        XrdCl::Buffer buffer( std::move( pool.front() ) );
        pool.pop();
        return buffer;
      }

      //-----------------------------------------------------------------------
      //! Give back a buffer to the poool
      //-----------------------------------------------------------------------
      void Recycle( XrdCl::Buffer && buffer )
      {
        if( !buffer.GetBuffer() ) return;
        std::unique_lock<std::mutex> lck( mtx );
        buffer.SetCursor( 0 );
        pool.emplace( std::move( buffer ) );
        cv.notify_all();
      }

    private:

      //-----------------------------------------------------------------------
      // Default constructor
      //-----------------------------------------------------------------------
      BufferPool() : totalsize( 1024 ), currentsize( 0 )
      {
      }

      BufferPool( const BufferPool& ) = delete;            //< Copy constructor
      BufferPool( BufferPool&& ) = delete;                 //< Move constructor
      BufferPool& operator=( const BufferPool& ) = delete; //< Copy assigment operator
      BufferPool& operator=( BufferPool&& ) = delete;      //< Move assigment operator

      const size_t               totalsize;   //< maximum size of the pool
      size_t                     currentsize; //< current size of the pool
      std::condition_variable    cv;
      std::mutex                 mtx;
      std::queue<XrdCl::Buffer>  pool;        //< the pool itself
  };

  //---------------------------------------------------------------------------
  //! Write cache, accumulates full block and then calculates parity and
  //! all of it to the storage 
  //---------------------------------------------------------------------------
  class WrtBuff
  {
    public:
      //-----------------------------------------------------------------------
      //! Constructor
      //!
      //! @param objcfg : data object configuration
      //-----------------------------------------------------------------------
      WrtBuff( const ObjCfg &objcfg ) : objcfg( objcfg ),
                                        wrtbuff( BufferPool::Instance().Create( objcfg ) )
      {
        stripes.reserve( objcfg.nbchunks );
        memset( wrtbuff.GetBuffer(), 0, wrtbuff.GetSize() );
      }
      //-----------------------------------------------------------------------
      //! Move constructor
      //-----------------------------------------------------------------------
      WrtBuff( WrtBuff && wrtbuff ) : objcfg( wrtbuff.objcfg ),
                                      wrtbuff( std::move( wrtbuff.wrtbuff ) ),
                                      stripes( std::move( wrtbuff.stripes ) ),
                                      cksums( std::move( wrtbuff.cksums ) )
      {
      }
      //-----------------------------------------------------------------------
      // Destructor
      //-----------------------------------------------------------------------
      ~WrtBuff()
      {
        BufferPool::Instance().Recycle( std::move( wrtbuff ) );
      }
      //-----------------------------------------------------------------------
      //! Write data into the buffer
      //!
      //! @param size   : number of bytes to be written
      //! @param buffer : buffer with data to be written
      //! @return       : number of bytes accepted by the buffer
      //-----------------------------------------------------------------------
      uint32_t Write( uint32_t size, const char *buffer )
      {
        uint64_t bytesAccepted = size; // bytes accepted by the buffer
        if( wrtbuff.GetCursor() + bytesAccepted > objcfg.datasize )
          bytesAccepted = objcfg.datasize - wrtbuff.GetCursor();
        memcpy( wrtbuff.GetBufferAtCursor(), buffer, bytesAccepted );
        wrtbuff.AdvanceCursor( bytesAccepted );
        return bytesAccepted;
      }
      //-----------------------------------------------------------------------
      //! Pad the buffer with zeros.
      //!
      //! @param size : number of zeros to be written into the buffer
      //-----------------------------------------------------------------------
      void Pad( uint32_t size )
      {
        // if the buffer exist we only need to move the cursor
        if( wrtbuff.GetSize() != 0 )
        {
          wrtbuff.AdvanceCursor( size );
          return;
        }
        // otherwise we allocate the buffer and set the cursor
        wrtbuff.Allocate( objcfg.datasize );
        memset( wrtbuff.GetBuffer(), 0, wrtbuff.GetSize() );
        wrtbuff.SetCursor( size );
        return;
      }
      //-----------------------------------------------------------------------
      //! Return buffer corresponding to given stripe
      //!
      //! @param strpnb : number of the stripe
      //-----------------------------------------------------------------------
      inline char* GetStrpBuff( uint8_t strpnb )
      {
        return stripes[strpnb].buffer;
      }
      //-----------------------------------------------------------------------
      //! Return size of the data in the given stripe
      //!
      //! @param strp : number of the stripe
      //-----------------------------------------------------------------------
      uint32_t GetStrpSize( uint8_t strp )
      {
        // Check if it is a data chunk?
        if( strp < objcfg.nbdata )
        {
          // If the cursor is at least at the expected size
          // it means we have the full chunk.
          uint64_t expsize = ( strp + 1) * objcfg.chunksize;
          if( expsize <= wrtbuff.GetCursor() )
            return objcfg.chunksize;
          // If the cursor is of by less than the chunk size
          // it means we have a partial chunk
          uint64_t delta = expsize - wrtbuff.GetCursor();
          if( delta < objcfg.chunksize )
            return objcfg.chunksize - delta;
          // otherwise we are handling an empty chunk
          return 0;
        }
        // It is a parity chunk so its size has  to be equal
        // to the size of the first chunk
        return GetStrpSize( 0 );
      }
      //-----------------------------------------------------------------------
      //! Get size of the data in the buffer
      //-----------------------------------------------------------------------
      inline uint32_t GetBlkSize()
      {
        return wrtbuff.GetCursor();
      }
      //-----------------------------------------------------------------------
      //! True if the buffer if full, false otherwise
      //-----------------------------------------------------------------------
      inline bool Complete()
      {
        return wrtbuff.GetCursor() == objcfg.datasize;
      }
      //-----------------------------------------------------------------------
      //! True if there are no data in the buffer, false otherwise
      //-----------------------------------------------------------------------
      inline bool Empty()
      {
        return ( wrtbuff.GetSize() == 0 || wrtbuff.GetCursor() == 0 );
      }
      //-----------------------------------------------------------------------
      //! Calculate the parity for the data stripes and the crc32cs
      //-----------------------------------------------------------------------
      inline void Encode()
      {
        // first calculate the parity
        uint8_t i ;
        for( i = 0; i < objcfg.nbchunks; ++i )
          stripes.emplace_back( wrtbuff.GetBuffer( i * objcfg.chunksize ), i < objcfg.nbdata );
        Config &cfg = Config::Instance();
        cfg.GetRedundancy( objcfg ).compute( stripes );
        // then calculate the checksums
        cksums.reserve( objcfg.nbchunks );
        for( uint8_t strpnb = 0; strpnb < objcfg.nbchunks; ++strpnb )
        {
          size_t chunksize = GetStrpSize( strpnb );
          std::future<uint32_t> ftr = ThreadPool::Instance().Execute( objcfg.digest, 0, stripes[strpnb].buffer, chunksize );
          cksums.emplace_back( std::move( ftr ) );
        }
      }
      //-----------------------------------------------------------------------
      //! Calculate the crc32c for given data stripe
      //!
      //! @param strpnb : number of the stripe
      //! @return       : the crc32c of the data stripe
      //-----------------------------------------------------------------------
      inline uint32_t GetCrc32c( size_t strpnb )
      {
        return cksums[strpnb].get();
      }

    private:

      ObjCfg                             objcfg;  //< configuration for the data object
      XrdCl::Buffer                      wrtbuff; //< the buffer for the data
      stripes_t                          stripes; //< data stripes
      std::vector<std::future<uint32_t>> cksums;  //< crc32cs for the data stripes
  };


} /* namespace XrdEc */

#endif /* SRC_XRDEC_XRDECWRTBUFF_HH_ */
