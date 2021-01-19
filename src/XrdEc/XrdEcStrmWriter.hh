/*
 * XrdEcStrmWriter.hh
 *
 *  Created on: 5 May 2020
 *      Author: simonm
 */

#ifndef SRC_XRDEC_XRDECSTRMWRITER_HH_
#define SRC_XRDEC_XRDECSTRMWRITER_HH_

#include "XrdEc/XrdEcWrtBuff.hh"
#include "XrdEc/XrdEcThreadPool.hh"

#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClParallelOperation.hh"
#include "XrdCl/XrdClZipOperations.hh"

#include "XrdZip/XrdZipLFH.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include "XrdZip/XrdZipEOCD.hh"
#include "XrdZip/XrdZipUtils.hh"

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

      void Open( XrdCl::ResponseHandler *handler )
      {
        const size_t size = objcfg.plgr.size();

        std::vector<XrdCl::Pipeline> opens;
        opens.reserve( size );
        // initialize all zip archive objects
        for( size_t i = 0; i < size; ++i )
          dataarchs.emplace_back( std::make_shared<XrdCl::ZipArchive>() );

        for( size_t i = 0; i < size; ++i )
        {
          std::string url = objcfg.GetDataUrl( i );
          XrdCl::Ctx<XrdCl::ZipArchive> zip( *dataarchs[i] );
          opens.emplace_back( XrdCl::OpenArchive( zip, url, XrdCl::OpenFlags::New | XrdCl::OpenFlags::Write ) );
        }

        XrdCl::Async( XrdCl::Parallel( opens ).AtLeast( objcfg.nbchunks ) >>
                      [=]( XrdCl::XRootDStatus &st )
                      {
                        if( !st.IsOK() ) global_status.report_open( st );
                        handler->HandleResponse( new XrdCl::XRootDStatus( st ), nullptr );
                      } );
      }

      void Write( uint32_t size, const void *buff, XrdCl::ResponseHandler *handler )
      {
        //---------------------------------------------------------------------
        // First, check the global status, if we are in an error state just
        // fail the request.
        //---------------------------------------------------------------------
        XrdCl::XRootDStatus gst = global_status.get();
        if( !gst.IsOK() ) return ScheduleHandler( handler, gst );

        //---------------------------------------------------------------------
        // Update the number of bytes left to be written
        //---------------------------------------------------------------------
        global_status.issue_write( size );

        const char* buffer = reinterpret_cast<const char*>( buff );
        uint32_t wrtsize = size;
        while( wrtsize > 0 )
        {
          if( !wrtbuff ) wrtbuff.reset( new WrtBuff( objcfg ) );
          uint64_t written = wrtbuff->Write( wrtsize, buffer );
          buffer  += written;
          wrtsize -= written;
          if( wrtbuff->Complete() ) EnqueueBuff( std::move( wrtbuff ) );
        }

        //---------------------------------------------------------------------
        // We can tell the user it's done as we have the date cached in the
        // buffer
        //---------------------------------------------------------------------
        ScheduleHandler( handler );
      }

      void Close( XrdCl::ResponseHandler *handler )
      {
        //---------------------------------------------------------------------
        // First, check the global status, if we are in an error state just
        // fail the request.
        //---------------------------------------------------------------------
        XrdCl::XRootDStatus gst = global_status.get();
        if( !gst.IsOK() ) return ScheduleHandler( handler, gst );
        //---------------------------------------------------------------------
        // Take care of the left-over data ...
        //---------------------------------------------------------------------
        if( wrtbuff && !wrtbuff->Empty() ) EnqueueBuff( std::move( wrtbuff ) );
        //---------------------------------------------------------------------
        // Let the global status handle the close
        //---------------------------------------------------------------------
        global_status.issue_close( handler );
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
                                                bytesleft( 0 ),
                                                stopped_writing( false ),
                                                closeHandler( 0 )
        {
        }

        //---------------------------------------------------------------------
        // Report status of write operation
        //---------------------------------------------------------------------
        void report_wrt( const XrdCl::XRootDStatus &st, uint64_t wrtsize )
        {
          std::unique_lock<std::mutex> lck( mtx );
          //-------------------------------------------------------------------
          // Update the global status
          //-------------------------------------------------------------------
          bytesleft -= wrtsize;
          if( !st.IsOK() ) status = st;

          //-------------------------------------------------------------------
          // check if we are done, and if yes call the close implementation
          //-------------------------------------------------------------------
          if( bytesleft == 0 && stopped_writing )
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
        void issue_close( XrdCl::ResponseHandler *handler )
        {
          std::unique_lock<std::mutex> lck( mtx );
          //-------------------------------------------------------------------
          // There will be no more new write requests
          //-------------------------------------------------------------------
          stopped_writing = true;
          //-------------------------------------------------------------------
          // If there are no outstanding writes, we can simply call the close
          // routine
          //-------------------------------------------------------------------
          if( bytesleft == 0 ) return writer->CloseImpl( handler );
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
          std::unique_lock<std::mutex> lck( mtx );
          return status;
        }

        inline void issue_write( uint64_t wrtsize )
        {
          std::unique_lock<std::mutex> lck( mtx );
          bytesleft += wrtsize;
        }

        private:
          mutable std::mutex      mtx;
          StrmWriter             *writer;          //> pointer to the StrmWriter
          uint64_t                bytesleft;       //> bytes left to be written
          bool                    stopped_writing; //> true, if user called close
          XrdCl::XRootDStatus     status;          //> the global status
          XrdCl::ResponseHandler *closeHandler;    //> user close handler
      };

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

      inline std::unique_ptr<WrtBuff> DequeueBuff()
      {
        std::future<WrtBuff*> ftr = buffers.dequeue();
        std::unique_ptr<WrtBuff> result( ftr.get() );
        return std::move( result );
      }

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

      void WriteBuff( std::unique_ptr<WrtBuff> buff )
      {
        //---------------------------------------------------------------------
        // Our buffer with the data block, will be shared between all pipelines
        // writing to different servers.
        //---------------------------------------------------------------------
        std::shared_ptr<WrtBuff> wrtbuff( std::move( buff ) );

        //---------------------------------------------------------------------
        // Shuffle the servers so every block has a different placement
        //---------------------------------------------------------------------
        static std::default_random_engine random_engine( std::chrono::system_clock::now().time_since_epoch().count() );
        std::shared_ptr<sync_queue<size_t>> servers = std::make_shared<sync_queue<size_t>>();
        std::vector<size_t> zipid( dataarchs.size() );
        std::iota( zipid.begin(), zipid.end(), 0 );
        std::shuffle( zipid.begin(), zipid.end(), random_engine );
        auto itr = zipid.begin();
        for( ; itr != zipid.end() ; ++itr ) servers->enqueue( std::move( *itr ) );

        //---------------------------------------------------------------------
        // Create the write pipelines for updating stripes
        //---------------------------------------------------------------------
        const size_t nbchunks = objcfg.nbchunks;
        std::vector<XrdCl::Pipeline> writes;
        writes.reserve( nbchunks );
        size_t   blknb = next_blknb++;
        uint64_t blksize = 0;
        for( size_t strpnb = 0; strpnb < nbchunks; ++strpnb )
        {
          std::string fn       = objcfg.GetFileName( blknb, strpnb );
          uint32_t    crc32c   = wrtbuff->GetCrc32c( strpnb );
          uint64_t    strpsize = wrtbuff->GetStrpSize( strpnb );
          char*       strpbuff = wrtbuff->GetStrpBuff( strpnb );
          if( strpnb < objcfg.nbdata ) blksize += strpsize;

          //-------------------------------------------------------------------
          // Find a server where we can append the next data chunk
          //-------------------------------------------------------------------
          XrdCl::Ctx<XrdCl::ZipArchive> zip;
          size_t srvid;
          if( !servers->dequeue( srvid ) )
          {
            XrdCl::XRootDStatus err( XrdCl::stError, XrdCl::errNoMoreReplicas,
                                     0, "No more data servers to try." );
            //-----------------------------------------------------------------
            // calculate the full block size, otherwise the user handler
            // will be never called
            //-----------------------------------------------------------------
            for( size_t i = strpnb + 1; i < objcfg.nbdata; ++i )
              blksize += wrtbuff->GetStrpSize( i );
            global_status.report_wrt( err, blksize );
            return;
          }
          zip = *dataarchs[srvid];

          //-------------------------------------------------------------------
          // Create the Write request
          //-------------------------------------------------------------------
          XrdCl::Pipeline p = XrdCl::AppendFile( zip, fn, crc32c, strpsize, strpbuff ) >>
                               [=]( XrdCl::XRootDStatus &st ) mutable
                               {
                                 //---------------------------------------------
                                 // Try to recover from error
                                 //---------------------------------------------
                                 if( !st.IsOK() )
                                 {
                                   //-------------------------------------------
                                   // Select another server
                                   //-------------------------------------------
                                   size_t srvid;
                                   if( !servers->dequeue( srvid ) ) return; // if there are no more servers we simply fail
                                   zip = *dataarchs[srvid];
                                   //-------------------------------------------
                                   // Retry this operation at different server
                                   //-------------------------------------------
                                   XrdCl::Pipeline::Repeat();
                                 }
                                 //---------------------------------------------
                                 // Make sure the buffer is only deallocated
                                 // after the handler is called
                                 //---------------------------------------------
                                 wrtbuff.reset();
                               };
          writes.emplace_back( std::move( p ) );
        }

        XrdCl::Async( XrdCl::Parallel( writes ) >> [=]( XrdCl::XRootDStatus &st ){ global_status.report_wrt( st, blksize ); } );
      }

      XrdZip::buffer_t GetMetadataBuffer()
      {
        using namespace XrdZip;

        const size_t cdcnt = objcfg.plgr.size();
        std::vector<buffer_t> buffs; buffs.reserve( cdcnt ); // buffers with raw data
        std::vector<LFH> lfhs; lfhs.reserve( cdcnt );        // LFH records
        std::vector<CDFH> cdfhs; cdfhs.reserve( cdcnt );     // CDFH records

        //---------------------------------------------------------------------
        // prepare data structures (LFH and CDFH records)
        //---------------------------------------------------------------------
        uint64_t offset = 0;
        uint64_t cdsize = 0;
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        for( size_t i = 0; i < cdcnt; ++i )
        {
          std::string fn = objcfg.GetDataUrl( i );                  // file name (URL of the data archive)
          buffer_t buff( dataarchs[i]->GetCD() );                   // raw data buffer (central directory of the data archive)
          uint32_t cksum = crc32c( 0, buff.data(), buff.size() );   // crc32c of the buffer
          lfhs.emplace_back( fn, cksum, buff.size(), time( 0 ) );   // LFH record for the buffer
          LFH &lfh = lfhs.back();
          cdfhs.emplace_back( &lfh, mode, offset );                 // CDFH record for the buffer
          offset += LFH::lfhBaseSize + fn.size() + buff.size();     // shift the offset
          cdsize += cdfhs.back().cdfhSize;                          // update central directory size
          buffs.emplace_back( std::move( buff ) );                  // keep the buffer for later
        }

        uint64_t zipsize = offset + cdsize + EOCD::eocdBaseSize;
        buffer_t zipbuff; zipbuff.reserve( zipsize );

        //---------------------------------------------------------------------
        // write into the final buffer LFH records + raw data
        //---------------------------------------------------------------------
        for( size_t i = 0; i < cdcnt; ++i )
        {
          lfhs[i].Serialize( zipbuff );
          std::copy( buffs[i].begin(), buffs[i].end(), std::back_inserter( zipbuff ) );
        }
        //---------------------------------------------------------------------
        // write into the final buffer CDFH records
        //---------------------------------------------------------------------
        for( size_t i = 0; i < cdcnt; ++i )
          cdfhs[i].Serialize( zipbuff );
        //---------------------------------------------------------------------
        // prepare and write into the final buffer the EOCD record
        //---------------------------------------------------------------------
        EOCD eocd( offset, cdcnt, cdsize );
        eocd.Serialize( zipbuff );

        return zipbuff;
      }

      void CloseImpl( XrdCl::ResponseHandler *handler )
      {
        const size_t size = objcfg.plgr.size();
        //---------------------------------------------------------------------
        // prepare the metadata (the Central Directory of each data ZIP)
        //---------------------------------------------------------------------
        auto zipbuff = std::make_shared<XrdZip::buffer_t>( GetMetadataBuffer() );
        //---------------------------------------------------------------------
        // prepare the pipelines ...
        //---------------------------------------------------------------------
        std::vector<XrdCl::Pipeline> closes;
        std::vector<XrdCl::Pipeline> save_metadata;
        closes.reserve( size );
        for( size_t i = 0; i < size; ++i )
        {
          //-------------------------------------------------------------------
          // close ZIP archives with data
          //-------------------------------------------------------------------
          closes.emplace_back( XrdCl::CloseArchive( *dataarchs[i] ) );
          //-------------------------------------------------------------------
          // replicate the metadata
          //-------------------------------------------------------------------
          std::string url = objcfg.GetMetadataUrl( i );
          metadataarchs.emplace_back( std::make_shared<XrdCl::File>() );
          XrdCl::Pipeline p = XrdCl::Open( *metadataarchs[i], url, XrdCl::OpenFlags::New | XrdCl::OpenFlags::Write )
                            | XrdCl::Write( *metadataarchs[i], 0, zipbuff->size(), zipbuff->data() )
                            | XrdCl::Close( *metadataarchs[i] )
                            | XrdCl::Final( [zipbuff]( const XrdCl::XRootDStatus& ){ } );

          save_metadata.emplace_back( std::move( p ) );
        }

        //---------------------------------------------------------------------
        // compose closes & save_metadata:
        //  - closes must be successful at least for #data + #parity
        //  - save_metadata must be successful at least for #parity + 1
        //---------------------------------------------------------------------
        XrdCl::Pipeline p = XrdCl::Parallel(
            XrdCl::Parallel( closes ).AtLeast( objcfg.nbchunks ),
            XrdCl::Parallel( save_metadata ).AtLeast( objcfg.nbparity + 1 )
          ) >> handler;
        XrdCl::Async( std::move( p ) );
      }

      const ObjCfg                                    &objcfg;
      std::unique_ptr<WrtBuff>                         wrtbuff;
      std::vector<std::shared_ptr<XrdCl::ZipArchive>>  dataarchs;
      std::vector<std::shared_ptr<XrdCl::File>>        metadataarchs;
      std::vector<XrdZip::buffer_t>                    cdbuffs;

      // queue of buffer being prepared (erasure encoded and checksummed) for write
      buff_queue                                       buffers;

      std::atomic<bool>                                writer_thread_stop;
      std::thread                                      writer_thread;
      size_t                                           next_blknb;

      global_status_t                                  global_status;
  };

}

#endif /* SRC_XRDEC_XRDECSTRMWRITER_HH_ */
