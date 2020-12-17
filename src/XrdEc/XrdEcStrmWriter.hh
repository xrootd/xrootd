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
#include "XrdCl/XrdClZipArchive.hh"

#include <random>
#include <chrono>
#include <future>
#include <atomic>
#include <memory>
#include <vector>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <iterator>

namespace XrdEc
{
  //---------------------------------------------------------------------------
  // A class implementing synchronous queue
  //---------------------------------------------------------------------------
  template<typename Element>
  struct sync_queue
  {
    //-------------------------------------------------------------------------
    // An internal exception used for interrupting the `dequeue` method
    //-------------------------------------------------------------------------
    struct wait_interrupted{ };

    //-------------------------------------------------------------------------
    // Default constructor
    //-------------------------------------------------------------------------
    sync_queue() : interrupted( false )
    {
    }

    //-------------------------------------------------------------------------
    // Enqueue new element into the queue
    //-------------------------------------------------------------------------
    inline void enqueue( Element && element )
    {
      std::unique_lock<std::mutex> lck( mtx );
      elements.push( std::move( element ) );
      cv.notify_all();
    }

    //-------------------------------------------------------------------------
    // Dequeue an element from the front of the queue
    // Note: if the queue is empty blocks until a new element is enqueued
    //-------------------------------------------------------------------------
    inline Element dequeue()
    {
      std::unique_lock<std::mutex> lck( mtx );
      while( elements.empty() )
      {
        cv.wait( lck );
        if( interrupted ) throw wait_interrupted();
      }
      Element element = std::move( elements.front() );
      elements.pop();
      return std::move( element );
    }

    //-------------------------------------------------------------------------
    // Dequeue an element from the front of the queue
    // Note: if the queue is empty returns false, true otherwise
    //-------------------------------------------------------------------------
    inline bool dequeue( Element &e )
    {
      std::unique_lock<std::mutex> lck( mtx );
      if( elements.empty() ) return false;
      e = std::move( elements.front() );
      elements.pop();
      return true;
    }

    //-------------------------------------------------------------------------
    // Checks if the queue is empty
    //-------------------------------------------------------------------------
    bool empty()
    {
      std::unique_lock<std::mutex> lck( mtx );
      return elements.empty();
    }

    //-------------------------------------------------------------------------
    // Interrupt all waiting `dequeue` routines
    //-------------------------------------------------------------------------
    inline void interrupt()
    {
      interrupted = true;
      cv.notify_all();
    }

    private:
      std::queue<Element>     elements;    //< the queue itself
      std::mutex              mtx;         //< mutex guarding the queue
      std::condition_variable cv;
      std::atomic<bool>       interrupted; //< a flag, true if all `dequeue` routines
                                           //< should be interrupted
  };

  //---------------------------------------------------------------------------
  // The Stream Writer objects, responsible for writing erasure coded data
  // into selected placement group.
  //---------------------------------------------------------------------------
  class StrmWriter
  {
    //-------------------------------------------------------------------------
    // Type for queue of buffers to be written
    //-------------------------------------------------------------------------
    typedef sync_queue<std::future<WrtBuff*>> buff_queue;

    public:

      //-----------------------------------------------------------------------
      // Constructor
      //-----------------------------------------------------------------------
      StrmWriter( const ObjCfg &objcfg ) : objcfg( objcfg ),
                                           writer_thread_stop( false ),
                                           writer_thread( writer_routine, this ),
                                           next_blknb( 0 ),
                                           global_status( this )
      {
      }

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
          archives.emplace_back( std::make_shared<XrdCl::ZipArchive>() );

        for( size_t i = 0; i < size; ++i )
        {
          std::string url = objcfg.plgr[i] + objcfg.obj + ".zip";
          XrdCl::Ctx<XrdCl::ZipArchive> zip( *archives[i] );
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
        std::vector<size_t> zipid( archives.size() );
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
          std::string fn       = objcfg.obj + '.' + std::to_string( blknb ) + '.' + std::to_string( strpnb );
          uint32_t    crc32c   = wrtbuff->GetCrc32c( strpnb );
          uint64_t    strpsize = wrtbuff->GetStrpSize( strpnb );
          char*       strpbuff = wrtbuff->GetStrpBuff( strpnb );
          if( strpnb < objcfg.nbdata ) blksize += strpsize;

          //-------------------------------------------------------------------
          // Find a server where we can append the next data chunk
          //-------------------------------------------------------------------
          XrdCl::Ctx<XrdCl::ZipArchive> zip;
          XrdCl::XRootDStatus st;
          do
          {
            size_t srvid;
            if( !servers->dequeue( srvid ) )
            {
              XrdCl::XRootDStatus err( XrdCl::stError, XrdCl::errNoMoreReplicas,
                                       0, "No more data servers to try." );
              global_status.report_wrt( err, strpsize );
              return;
            }

            zip = *archives[srvid];
            st = zip->OpenFile( fn, XrdCl::OpenFlags::New, strpsize, crc32c );
          }
          while( !st.IsOK() );

          //-------------------------------------------------------------------
          // Create the Write request
          //-------------------------------------------------------------------
          XrdCl::Pipeline p = XrdCl::Write( zip, strpsize, strpbuff ) >>
                              [=]( XrdCl::XRootDStatus &st ) mutable
                              {
                                //---------------------------------------------
                                // Try to recover from error
                                //---------------------------------------------
                                if( !st.IsOK() )
                                {
                                  //-------------------------------------------
                                  // First clean up the ZipArchive object
                                  //-------------------------------------------
                                  zip->CloseFile();
                                  //-------------------------------------------
                                  // Then select another server
                                  //-------------------------------------------
                                  XrdCl::XRootDStatus status;
                                  do
                                  {
                                    size_t srvid;
                                    if( !servers->dequeue( srvid ) ) return; // if there are no more servers we simply fail
                                    zip = *archives[srvid];
                                    st = zip->OpenFile( fn, XrdCl::OpenFlags::New, strpsize, crc32c );
                                  } while( !status.IsOK() );
                                  //-------------------------------------------
                                  // Retry this operation at different server
                                  //-------------------------------------------
                                  XrdCl::Pipeline::Repeat();
                                }
                              }
                            | XrdCl::Final(
                              [=]( const XrdCl::XRootDStatus &st ) mutable
                              {
                                zip->CloseFile();
                                wrtbuff.reset();
                              } );
          writes.emplace_back( std::move( p ) );
        }

        XrdCl::Async( XrdCl::Parallel( writes ) >> [=]( XrdCl::XRootDStatus &st ){ global_status.report_wrt( st, blksize ); } );
      }

      void CloseImpl( XrdCl::ResponseHandler *handler )
      {
        const size_t size = objcfg.plgr.size();

        std::vector<XrdCl::Pipeline> closes;
        closes.reserve( size );

        for( size_t i = 0; i < size; ++i )
        {
          closes.emplace_back( XrdCl::CloseArchive( *archives[i] ) );
        }

        XrdCl::Async( XrdCl::Parallel( closes ).AtLeast( objcfg.nbchunks ) >> handler );
      }

      const ObjCfg                                    &objcfg;
      std::unique_ptr<WrtBuff>                         wrtbuff;
      std::vector<std::shared_ptr<XrdCl::ZipArchive>>  archives;

      // queue of buffer being prepared (erasure encoded and checksummed) for write
      buff_queue                                       buffers;

      std::atomic<bool>                                writer_thread_stop;
      std::thread                                      writer_thread;
      size_t                                           next_blknb;

      global_status_t                                  global_status;
  };

}

#endif /* SRC_XRDEC_XRDECSTRMWRITER_HH_ */
