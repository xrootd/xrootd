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

#ifndef SRC_XRDEC_XRDECSTRMWRITER_HH_
#define SRC_XRDEC_XRDECSTRMWRITER_HH_

#include "XrdEc/XrdEcWrtBuff.hh"
#include "XrdEc/XrdEcThreadPool.hh"

#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClParallelOperation.hh"
#include "XrdCl/XrdClZipOperations.hh"

#include <random>
#include <chrono>
#include <future>
#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <iterator>

#include <sys/stat.h>

namespace XrdEc
{
  //---------------------------------------------------------------------------
  //! The Stream Writer objects, responsible for writing erasure coded data
  //! into selected placement group.
  //---------------------------------------------------------------------------
  class StrmWriter
  {
    //-------------------------------------------------------------------------
    // Type for queue of buffers to be written
    //-------------------------------------------------------------------------
    typedef sync_queue<std::future<WrtBuff*>> buff_queue;

    public:

      //-----------------------------------------------------------------------
      //! Constructor
      //-----------------------------------------------------------------------
      StrmWriter( const ObjCfg &objcfg ) : objcfg( objcfg ),
                                           writer_thread_stop( false ),
                                           writer_thread( writer_routine, this ),
                                           next_blknb( 0 ),
                                           global_status( this )
      {
      }

      //-----------------------------------------------------------------------
      //! Destructor
      //-----------------------------------------------------------------------
      virtual ~StrmWriter()
      {
        writer_thread_stop = true;
        buffers.interrupt();
        writer_thread.join();
      }

      //-----------------------------------------------------------------------
      //! Open the data object for writting
      //!
      //! @param handler : user callback
      //-----------------------------------------------------------------------
      void Open( XrdCl::ResponseHandler *handler, time_t timeout = 0 );

      //-----------------------------------------------------------------------
      //! Write data to the data object
      //!
      //! @param size    : number of bytes to be written
      //! @param buff    : buffer with data to be written
      //! @param handler : user callback
      //-----------------------------------------------------------------------
      void Write( uint32_t size, const void *buff, XrdCl::ResponseHandler *handler );

      //-----------------------------------------------------------------------
      //! Close the data object
      //!
      //! @param handler : user callback
      //-----------------------------------------------------------------------
      void Close( XrdCl::ResponseHandler *handler, time_t timeout = 0 );

      //-----------------------------------------------------------------------
      //! @return : get file size
      //-----------------------------------------------------------------------
      uint64_t GetSize()
      {
        return global_status.get_btswritten();
      }

    private:

      //-----------------------------------------------------------------------
      // Global status of the StrmWriter
      //-----------------------------------------------------------------------
      struct global_status_t
      {
        //---------------------------------------------------------------------
        // Constructor
        //---------------------------------------------------------------------
        global_status_t( StrmWriter *writer ) : writer( writer ),
                                                btsleft( 0 ),
                                                btswritten( 0 ),
                                                stopped_writing( false ),
                                                closeHandler( 0 )
        {
        }

        //---------------------------------------------------------------------
        // Report status of write operation
        //---------------------------------------------------------------------
        void report_wrt( const XrdCl::XRootDStatus &st, uint64_t wrtsize )
        {
          std::unique_lock<std::recursive_mutex> lck( mtx );
          //-------------------------------------------------------------------
          // Update the global status
          //-------------------------------------------------------------------
          btsleft -= wrtsize;
          if( !st.IsOK() ) status = st;
          else btswritten += wrtsize;

          //-------------------------------------------------------------------
          // check if we are done, and if yes call the close implementation
          //-------------------------------------------------------------------
          if( btsleft == 0 && stopped_writing )
          {
            lck.unlock();
            writer->CloseImpl( closeHandler );
          }
        }

        //---------------------------------------------------------------------
        // Report status of open operation
        //---------------------------------------------------------------------
        inline void report_open( const XrdCl::XRootDStatus &st )
        {
          report_wrt( st, 0 );
        }

        //---------------------------------------------------------------------
        // Indicate that the user issued close
        //---------------------------------------------------------------------
        void issue_close( XrdCl::ResponseHandler *handler, time_t timeout )
        {
          std::unique_lock<std::recursive_mutex> lck( mtx );
          //-------------------------------------------------------------------
          // There will be no more new write requests
          //-------------------------------------------------------------------
          stopped_writing = true;
          //-------------------------------------------------------------------
          // If there are no outstanding writes, we can simply call the close
          // routine
          //-------------------------------------------------------------------
          if( btsleft == 0 ) return writer->CloseImpl( handler, timeout );
          //-------------------------------------------------------------------
          // Otherwise we save the handler for later
          //-------------------------------------------------------------------
          closeHandler = handler;
        }

        //---------------------------------------------------------------------
        // get the global status value
        //---------------------------------------------------------------------
        inline const XrdCl::XRootDStatus& get() const
        {
          std::unique_lock<std::recursive_mutex> lck( mtx );
          return status;
        }

        inline void issue_write( uint64_t wrtsize )
        {
          std::unique_lock<std::recursive_mutex> lck( mtx );
          btsleft += wrtsize;
        }

        inline uint64_t get_btswritten()
        {
          return btswritten;
        }

        private:
          mutable std::recursive_mutex  mtx;
          StrmWriter                   *writer;          //> pointer to the StrmWriter
          uint64_t                      btsleft;         //> bytes left to be written
          uint64_t                      btswritten;      //> total number of bytes written
          bool                          stopped_writing; //> true, if user called close
          XrdCl::XRootDStatus           status;          //> the global status
          XrdCl::ResponseHandler       *closeHandler;    //> user close handler
      };

      //-----------------------------------------------------------------------
      //! Enqueue the write buffer for calculating parity and crc32c
      //!
      //! @param wrtbuff : the write buffer
      //-----------------------------------------------------------------------
      inline void EnqueueBuff( std::unique_ptr<WrtBuff> wrtbuff )
      {
        // the routine to be called in the thread-pool
        // - does erasure coding
        // - calculates crc32cs
        static auto prepare_buff = []( WrtBuff *wrtbuff )
        {
          std::unique_ptr<WrtBuff> ptr( wrtbuff );
          ptr->Encode();
          return ptr.release();
        };
        buffers.enqueue( ThreadPool::Instance().Execute( prepare_buff, wrtbuff.release() ) );
      }

      //-----------------------------------------------------------------------
      //! Dequeue a write buffer after it has been erasure coded and checksumed
      //!
      //! @return : the write buffer, ready for writing
      //-----------------------------------------------------------------------
      inline std::unique_ptr<WrtBuff> DequeueBuff()
      {
        std::future<WrtBuff*> ftr = buffers.dequeue();
        std::unique_ptr<WrtBuff> result( ftr.get() );
        return result;
      }

      //-----------------------------------------------------------------------
      //! The writing routine running in a dedicated thread.
      //!
      //! @param me : the StrmWriter object
      //-----------------------------------------------------------------------
      static void writer_routine( StrmWriter *me )
      {
        try
        {
          while( !me->writer_thread_stop )
          {
            std::unique_ptr<WrtBuff> wrtbuff( me->DequeueBuff() );
            if( !wrtbuff ) continue;
            me->WriteBuff( std::move( wrtbuff ) );
          }
        }
        catch( const buff_queue::wait_interrupted& ){ }
      }

      //-----------------------------------------------------------------------
      //! Issue the write requests for the given write buffer
      //!
      //! @param buff : the buffer to be written
      //-----------------------------------------------------------------------
      void WriteBuff( std::unique_ptr<WrtBuff> buff );

      //-----------------------------------------------------------------------
      //! Get a buffer with metadata (CDFH and EOCD records)
      //!
      //! @return : the buffer with metadata
      //-----------------------------------------------------------------------
      std::vector<char> GetMetadataBuffer();

      //-----------------------------------------------------------------------
      //! Close the data object (implementation)
      //!
      //! @param handler : user callback
      //-----------------------------------------------------------------------
      void CloseImpl( XrdCl::ResponseHandler *handler, time_t timeout = 0 );

      const ObjCfg                                    &objcfg;
      std::unique_ptr<WrtBuff>                         wrtbuff;            //< current write buffer
      std::vector<std::shared_ptr<XrdCl::ZipArchive>>  dataarchs;          //< ZIP archives with data
      std::vector<std::shared_ptr<XrdCl::File>>        metadataarchs;      //< ZIP archives with metadata
      std::vector<std::vector<char>>                   cdbuffs;            //< buffers with CDs
      buff_queue                                       buffers;            //< queue of buffer for writing
                                                                           //< (waiting to be erasure coded)
      std::atomic<bool>                                writer_thread_stop; //< true if the writer thread should be stopped,
                                                                           //< flase otherwise
      std::thread                                      writer_thread;      //< handle to the writer thread
      size_t                                           next_blknb;         //< number of the next block to be created
      global_status_t                                  global_status;      //< global status of the writer
  };

}

#endif /* SRC_XRDEC_XRDECSTRMWRITER_HH_ */
