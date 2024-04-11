//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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

#include "XrdCl/XrdClClassicCopyJob.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClMonitor.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClCheckSumManager.hh"
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCl/XrdClRedirectorRegistry.hh"
#include "XrdCl/XrdClZipArchive.hh"
#include "XrdCl/XrdClZipOperations.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdClXCpCtx.hh"
#include "XrdCl/XrdClCheckSumHelper.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <memory>
#include <mutex>
#include <queue>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>

#if __cplusplus < 201103L
#include <ctime>
#endif

namespace
{
  //----------------------------------------------------------------------------
  //! Helper timer class
  //----------------------------------------------------------------------------
  template<typename U = std::ratio<1, 1>>
  class mytimer_t
  {
    public:
      mytimer_t() : start( clock_t::now() ){ }
      void reset(){ start = clock_t::now(); }
      uint64_t elapsed() const
      {
        return std::chrono::duration_cast<unit_t>( clock_t::now() - start ).count();
      }
    private:
      typedef std::chrono::high_resolution_clock clock_t;
      typedef std::chrono::duration<uint64_t, U> unit_t;
      std::chrono::time_point<clock_t> start;
  };

  using timer_sec_t  = mytimer_t<>;
  using timer_nsec_t = mytimer_t<std::nano>;


  inline XrdCl::XRootDStatus Translate( std::vector<XrdCl::XAttr>   &in,
                                           std::vector<XrdCl::xattr_t> &out )
  {
    std::vector<XrdCl::xattr_t> ret;
    ret.reserve( in.size() );
    std::vector<XrdCl::XAttr>::iterator itr = in.begin();
    for( ; itr != in.end() ; ++itr )
    {
      if( !itr->status.IsOK() ) return itr->status;
      XrdCl::xattr_t xa( itr->name, itr->value );
      ret.push_back( std::move( xa ) );
    }
    out.swap( ret );
    return XrdCl::XRootDStatus();
  }

  //----------------------------------------------------------------------------
  //! Helper function for retrieving extended-attributes
  //----------------------------------------------------------------------------
  inline XrdCl::XRootDStatus GetXAttr( XrdCl::File                  &file,
                                       std::vector<XrdCl::xattr_t>  &xattrs )
  {
    std::vector<XrdCl::XAttr> rsp;
    XrdCl::XRootDStatus st = file.ListXAttr( rsp );
    if( !st.IsOK() ) return st;
    return Translate( rsp, xattrs );
  }

  //----------------------------------------------------------------------------
  //! Helper function for retrieving extended-attributes
  //----------------------------------------------------------------------------
  inline XrdCl::XRootDStatus GetXAttr( const std::string            &url,
                                       std::vector<XrdCl::xattr_t>  &xattrs )
  {
    XrdCl::URL u( url );
    XrdCl::FileSystem fs( u );
    std::vector<XrdCl::XAttr> rsp;
    XrdCl::XRootDStatus st = fs.ListXAttr( u.GetPath(), rsp );
    if( !st.IsOK() ) return st;
    return Translate( rsp, xattrs );
  }

  inline XrdCl::XRootDStatus SetXAttr( XrdCl::File                       &file,
                                const std::vector<XrdCl::xattr_t> &xattrs )
  {
    std::vector<XrdCl::XAttrStatus> rsp;
    file.SetXAttr( xattrs, rsp );
    std::vector<XrdCl::XAttrStatus>::iterator itr = rsp.begin();
    for( ; itr != rsp.end() ; ++itr )
      if( !itr->status.IsOK() ) return itr->status;
    return XrdCl::XRootDStatus();
  }

  //----------------------------------------------------------------------------
  //! Abstract chunk source
  //----------------------------------------------------------------------------
  class Source
  {
    public:
      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      Source( const std::string &checkSumType = "",
              const std::vector<std::string> &addcks = std::vector<std::string>() ) :
                pCkSumHelper( 0 ),
                pContinue( false )
      {
        if( !checkSumType.empty() )
          pCkSumHelper = new XrdCl::CheckSumHelper( "source", checkSumType );

        for( auto &type : addcks )
          pAddCksHelpers.push_back( new XrdCl::CheckSumHelper( "source", type ) );
      };

      virtual ~Source()
      {
        delete pCkSumHelper;
        for( auto ptr : pAddCksHelpers )
          delete ptr;
      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize() = 0;

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual int64_t GetSize() = 0;

      //------------------------------------------------------------------------
      //! Start reading from the source at given offset
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus StartAt( uint64_t offset ) = 0;

      //------------------------------------------------------------------------
      //! Get a data chunk from the source
      //!
      //! @param  ci     chunk information
      //! @return        status of the operation
      //!                suContinue - there are some chunks left
      //!                suDone     - no chunks left
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::PageInfo &ci ) = 0;

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType ) = 0;

      //------------------------------------------------------------------------
      //! Get additional checksums
      //------------------------------------------------------------------------
      virtual std::vector<std::string> GetAddCks() = 0;

      //------------------------------------------------------------------------
      //! Get extended attributes
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetXAttr( std::vector<XrdCl::xattr_t> &xattrs ) = 0;

      //------------------------------------------------------------------------
      //! Try different server
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus TryOtherServer()
      {
        return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotImplemented );
      }

    protected:

      XrdCl::CheckSumHelper               *pCkSumHelper;
      std::vector<XrdCl::CheckSumHelper*>  pAddCksHelpers;
      bool                                 pContinue;
  };

  //----------------------------------------------------------------------------
  //! Abstract chunk destination
  //----------------------------------------------------------------------------
  class Destination
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      Destination( const std::string &checkSumType = "" ):
        pPosc( false ), pForce( false ), pCoerce( false ), pMakeDir( false ),
        pContinue( false ), pCkSumHelper( 0 )
      {
        if( !checkSumType.empty() )
          pCkSumHelper = new XrdCl::CheckSumHelper( "destination", checkSumType );
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~Destination()
      {
        delete pCkSumHelper;
      }

      //------------------------------------------------------------------------
      //! Initialize the destination
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize() = 0;

      //------------------------------------------------------------------------
      //! Finalize the destination
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Finalize() = 0;

      //------------------------------------------------------------------------
      //! Put a data chunk at a destination
      //!
      //! @param  ci     chunk information
      //! @return status of the operation
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus PutChunk( XrdCl::PageInfo &&ci ) = 0;

      //------------------------------------------------------------------------
      //! Flush chunks that might have been queues
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Flush() = 0;

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType ) = 0;

      //------------------------------------------------------------------------
      //! Set extended attributes
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus SetXAttr( const std::vector<XrdCl::xattr_t> &xattrs ) = 0;

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual int64_t GetSize() = 0;

      //------------------------------------------------------------------------
      //! Set POSC
      //------------------------------------------------------------------------
      void SetPOSC( bool posc )
      {
        pPosc = posc;
      }

      //------------------------------------------------------------------------
      //! Set force
      //------------------------------------------------------------------------
      void SetForce( bool force )
      {
        pForce = force;
      }

      //------------------------------------------------------------------------
      //! Set continue
      //------------------------------------------------------------------------
      void SetContinue( bool continue_ )
      {
        pContinue = continue_;
      }

      //------------------------------------------------------------------------
      //! Set coerce
      //------------------------------------------------------------------------
      void SetCoerce( bool coerce )
      {
        pCoerce = coerce;
      }

      //------------------------------------------------------------------------
      //! Set makedir
      //------------------------------------------------------------------------
      void SetMakeDir( bool makedir )
      {
        pMakeDir = makedir;
      }

      //------------------------------------------------------------------------
      //! Get last URL
      //------------------------------------------------------------------------
      virtual const std::string& GetLastURL() const
      {
        static const std::string empty;
        return empty;
      }

      //------------------------------------------------------------------------
      //! Get write-recovery redirector
      //------------------------------------------------------------------------
      virtual const std::string& GetWrtRecoveryRedir() const
      {
        static const std::string empty;
        return empty;
      }

    protected:
      bool pPosc;
      bool pForce;
      bool pCoerce;
      bool pMakeDir;
      bool pContinue;

      XrdCl::CheckSumHelper    *pCkSumHelper;
  };

  //----------------------------------------------------------------------------
  //! StdIn source
  //----------------------------------------------------------------------------
  class StdInSource: public Source
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      StdInSource( const std::string &ckSumType, uint32_t chunkSize, const std::vector<std::string> &addcks ):
        Source( ckSumType, addcks ),
        pCurrentOffset(0),
        pChunkSize( chunkSize )
      {

      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~StdInSource()
      {

      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        if( pCkSumHelper )
        {
          auto st = pCkSumHelper->Initialize();
          if( !st.IsOK() ) return st;
          for( auto cksHelper : pAddCksHelpers )
          {
            st = cksHelper->Initialize();
            if( !st.IsOK() ) return st;
          }
        }
        return XrdCl::XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual int64_t GetSize()
      {
        return -1;
      }

      //------------------------------------------------------------------------
      //! Start reading from the source at given offset
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus StartAt( uint64_t )
      {
        return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported, ENOTSUP,
                                    "Cannot continue from stdin!" );
      }

      //------------------------------------------------------------------------
      //! Get a data chunk from the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::PageInfo &ci )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        uint32_t toRead = pChunkSize;
        char *buffer = new char[toRead];

        int64_t  bytesRead = 0;
        uint32_t offset    = 0;
        while( toRead )
        {
          int64_t bRead = read( 0, buffer+offset, toRead );
          if( bRead == -1 )
          {
            log->Debug( UtilityMsg, "Unable to read from stdin: %s",
                        XrdSysE2T( errno ) );
            delete [] buffer;
            return XRootDStatus( stError, errOSError, errno );
          }

          if( bRead == 0 )
            break;

          bytesRead += bRead;
          offset    += bRead;
          toRead    -= bRead;
        }

        if( bytesRead == 0 )
        {
          delete [] buffer;
          return XRootDStatus( stOK, suDone );
        }

        if( pCkSumHelper )
          pCkSumHelper->Update( buffer, bytesRead );

        for( auto cksHelper : pAddCksHelpers )
          cksHelper->Update( buffer, bytesRead );

        ci = XrdCl::PageInfo( pCurrentOffset, bytesRead, buffer );
        pCurrentOffset += bytesRead;
        return XRootDStatus( stOK, suContinue );
      }

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSumImpl( XrdCl::CheckSumHelper *cksHelper,
                                                   std::string           &checkSum,
                                                   std::string           &checkSumType )
      {
        using namespace XrdCl;
        if( cksHelper )
          return cksHelper->GetCheckSum( checkSum, checkSumType );
        return XRootDStatus( stError, errCheckSumError );
      }

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        return GetCheckSumImpl( pCkSumHelper, checkSum, checkSumType );
      }

      //------------------------------------------------------------------------
      //! Get additional checksums
      //------------------------------------------------------------------------
      std::vector<std::string> GetAddCks()
      {
        std::vector<std::string> ret;
        for( auto cksHelper : pAddCksHelpers )
        {
          std::string type = cksHelper->GetType();
          std::string cks;
          GetCheckSumImpl( cksHelper, cks, type );
          ret.push_back( type + ":" + cks );
        }
        return ret;
      }

      //------------------------------------------------------------------------
      //! Get extended attributes
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetXAttr( std::vector<XrdCl::xattr_t> &xattrs )
      {
        return XrdCl::XRootDStatus();
      }

    private:
      StdInSource(const StdInSource &other);
      StdInSource &operator = (const StdInSource &other);

      uint64_t        pCurrentOffset;
      uint32_t        pChunkSize;
  };

  //----------------------------------------------------------------------------
  //! XRootDSource
  //----------------------------------------------------------------------------
  class XRootDSource: public Source
  {
    struct CancellableJob : public XrdCl::Job
    {
      virtual void Cancel() = 0;

      std::mutex mtx;
    };

    //----------------------------------------------------------------------------
    // On-connect callback job, a lambda would be more elegant, but we still have
    // to support SLC6
    //----------------------------------------------------------------------------
    template<typename READER>
    struct OnConnJob : public CancellableJob
    {
        OnConnJob( XRootDSource *self, READER *reader ) : self( self ), reader( reader )
        {
        }

        void Run( void* )
        {
          std::unique_lock<std::mutex> lck( mtx );
          if( !self || !reader ) return;
          // add new chunks to the queue
          if( self->pNbConn < self->pMaxNbConn )
            self->FillQueue( reader );
        }

        void Cancel()
        {
          std::unique_lock<std::mutex> lck( mtx );
          self   = 0;
          reader = 0;
        }

      private:
        XRootDSource *self;
        READER       *reader;

    };

    public:

      //------------------------------------------------------------------------
      //! Try different server
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus TryOtherServer()
      {
        return pFile->TryOtherServer();
      }

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      XRootDSource( const XrdCl::URL *url,
                    uint32_t          chunkSize,
                    uint8_t           parallelChunks,
                    const std::string &ckSumType,
                    const std::vector<std::string> &addcks,
                    bool doserver ):
        Source( ckSumType, addcks ),
        pUrl( url ), pFile( new XrdCl::File() ), pSize( -1 ),
        pCurrentOffset( 0 ), pChunkSize( chunkSize ),
        pParallel( parallelChunks ),
        pNbConn( 0 ), pUsePgRead( false ),
        pDoServer( doserver )
      {
        int val = XrdCl::DefaultSubStreamsPerChannel;
        XrdCl::DefaultEnv::GetEnv()->GetInt( "SubStreamsPerChannel", val );
        pMaxNbConn = val - 1; // account for the control stream
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~XRootDSource()
      {
        if( pDataConnCB )
          pDataConnCB->Cancel();

        CleanUpChunks();
        if( pFile->IsOpen() )
          XrdCl::XRootDStatus status = pFile->Close();
        delete pFile;
      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();
        log->Debug( UtilityMsg, "Opening %s for reading",
                                pUrl->GetObfuscatedURL().c_str() );

        std::string value;
        DefaultEnv::GetEnv()->GetString( "ReadRecovery", value );
        pFile->SetProperty( "ReadRecovery", value );

        XRootDStatus st = pFile->Open( pUrl->GetURL(), OpenFlags::Read );
        if( !st.IsOK() )
          return st;

        StatInfo *statInfo;
        st = pFile->Stat( false, statInfo );
        if( !st.IsOK() )
          return st;

        pSize = statInfo->GetSize();
        delete statInfo;

        if( pUrl->IsLocalFile() && !pUrl->IsMetalink() && pCkSumHelper && !pContinue )
        {
          st = pCkSumHelper->Initialize();
          if( !st.IsOK() ) return st;

          for( auto cksHelper : pAddCksHelpers )
          {
            st = cksHelper->Initialize();
            if( !st.IsOK() ) return st;
          }
        }

        //----------------------------------------------------------------------
        // Figere out the actual data server we are talking to
        //----------------------------------------------------------------------
        if( !pUrl->IsLocalFile() ||
           ( pUrl->IsLocalFile() && pUrl->IsMetalink() ) )
        {
          pFile->GetProperty( "LastURL", pDataServer );
        }


        if( ( !pUrl->IsLocalFile() && !pFile->IsSecure() ) ||
            ( pUrl->IsLocalFile() && pUrl->IsMetalink() ) )
        {
          //--------------------------------------------------------------------
          // Decide whether we can use PgRead
          //--------------------------------------------------------------------
          int val = XrdCl::DefaultCpUsePgWrtRd;
          XrdCl::DefaultEnv::GetEnv()->GetInt( "CpUsePgWrtRd", val );
          pUsePgRead = XrdCl::Utils::HasPgRW( pDataServer ) && ( val == 1 );
        }

        //----------------------------------------------------------------------
        // Print the IPv4/IPv6 stack to the stderr if we are running in server
        // mode
        //----------------------------------------------------------------------
        if( pDoServer && !pUrl->IsLocalFile() )
        {
          AnyObject obj;
          DefaultEnv::GetPostMaster()->QueryTransport( pDataServer, StreamQuery::IpStack, obj );
          std::string *ipstack = nullptr;
          obj.Get( ipstack );
          std::cerr << "!-!" << *ipstack << std::endl;
          delete ipstack;
        }

        SetOnDataConnectHandler( pFile );

        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual int64_t GetSize()
      {
        return pSize;
      }

      //------------------------------------------------------------------------
      //! Start reading from the source at given offset
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus StartAt( uint64_t offset )
      {
        pCurrentOffset = offset;
        pContinue      = true;
        return XrdCl::XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get a data chunk from the source
      //!
      //! @param  ci     chunk information
      //! @return        status of the operation
      //!                suContinue - there are some chunks left
      //!                suDone     - no chunks left
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::PageInfo &ci )
      {
        return GetChunkImpl( pFile, ci );
      }

      //------------------------------------------------------------------------
      //! Get extended attributes
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetXAttr( std::vector<XrdCl::xattr_t> &xattrs )
      {
        return ::GetXAttr( *pFile, xattrs );
      }

      //------------------------------------------------------------------------
      // Clean up the chunks that are flying
      //------------------------------------------------------------------------
      void CleanUpChunks()
      {
        while( !pChunks.empty() )
        {
          ChunkHandler *ch = pChunks.front();
          pChunks.pop();
          ch->sem->Wait();
          delete [] (char *)ch->chunk.GetBuffer();
          delete ch;
        }
      }

      //------------------------------------------------------------------------
      // Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        return GetCheckSumImpl( pCkSumHelper, checkSum, checkSumType );
      }

      XrdCl::XRootDStatus GetCheckSumImpl( XrdCl::CheckSumHelper *cksHelper,
                                           std::string           &checkSum,
                                           std::string           &checkSumType )
      {
        if( pUrl->IsMetalink() )
        {
          XrdCl::RedirectorRegistry &registry   = XrdCl::RedirectorRegistry::Instance();
          XrdCl::VirtualRedirector  *redirector = registry.Get( *pUrl );
          checkSum = redirector->GetCheckSum( checkSumType );
          if( !checkSum.empty() ) return XrdCl::XRootDStatus();
        }

        if( pUrl->IsLocalFile() )
        {
          if( pContinue )
            // in case of --continue option we have to calculate the checksum from scratch
            return XrdCl::Utils::GetLocalCheckSum( checkSum, checkSumType, pUrl->GetPath() );

          if( cksHelper )
            return cksHelper->GetCheckSum( checkSum, checkSumType );

          return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errCheckSumError );
        }

        std::string dataServer; pFile->GetProperty( "DataServer", dataServer );
        std::string lastUrl;    pFile->GetProperty( "LastURL",    lastUrl );
        return XrdCl::Utils::GetRemoteCheckSum( checkSum, checkSumType, XrdCl::URL( lastUrl ) );
      }

      //------------------------------------------------------------------------
      //! Get additional checksums
      //------------------------------------------------------------------------
      std::vector<std::string> GetAddCks()
      {
        std::vector<std::string> ret;
        for( auto cksHelper : pAddCksHelpers )
        {
          std::string type = cksHelper->GetType();
          std::string cks;
          GetCheckSumImpl( cksHelper, cks, type );
          ret.push_back( cks );
        }
        return ret;
      }

    private:
      XRootDSource(const XRootDSource &other);
      XRootDSource &operator = (const XRootDSource &other);

    protected:

      //------------------------------------------------------------------------
      // Fill the queue with in-the-fly read requests
      //------------------------------------------------------------------------
      template<typename READER>
      inline void FillQueue( READER *reader )
      {
        //----------------------------------------------------------------------
        // Get the number of connected streams
        //----------------------------------------------------------------------
        uint16_t parallel = pParallel;
        if( pNbConn < pMaxNbConn )
        {
          pNbConn = XrdCl::DefaultEnv::GetPostMaster()->
                                                 NbConnectedStrm( pDataServer );
        }
        if( pNbConn ) parallel *= pNbConn;

        while( pChunks.size() < parallel && pCurrentOffset < pSize )
        {
          uint64_t chunkSize = pChunkSize;
          if( pCurrentOffset + chunkSize > (uint64_t)pSize )
            chunkSize = pSize - pCurrentOffset;

          char *buffer = new char[chunkSize];
          ChunkHandler *ch = new ChunkHandler();
          auto st = pUsePgRead
                     ? reader->PgRead( pCurrentOffset, chunkSize, buffer, ch )
                     : reader->Read( pCurrentOffset, chunkSize, buffer, ch );
          pChunks.push( ch );
          pCurrentOffset += chunkSize;
          if( !st.IsOK() )
          {
            ch->status = st;
            ch->sem->Post();
            break;
          }
        }
      }

      //------------------------------------------------------------------------
      // Set the on-connect handler for data streams
      //------------------------------------------------------------------------
      template<typename READER>
      void SetOnDataConnectHandler( READER *reader )
      {
        // we need to create the object anyway as it contains our mutex now
        pDataConnCB.reset( new OnConnJob<READER>( this, reader ) );

        // check if it is a local file
        if( pDataServer.empty() ) return;

        XrdCl::DefaultEnv::GetPostMaster()->SetOnDataConnectHandler( pDataServer, pDataConnCB );
      }

      //------------------------------------------------------------------------
      //! Get a data chunk from the source
      //!
      //! @param  reader  :  reader to read data from
      //! @param  ci      :  chunk information
      //! @return         :  status of the operation
      //!                    suContinue - there are some chunks left
      //!                    suDone     - no chunks left
      //------------------------------------------------------------------------
      template<typename READER>
      XrdCl::XRootDStatus GetChunkImpl( READER *reader, XrdCl::PageInfo &ci )
      {
        //----------------------------------------------------------------------
        // Sanity check
        //----------------------------------------------------------------------
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        //----------------------------------------------------------------------
        // Fill the queue
        //----------------------------------------------------------------------
        std::unique_lock<std::mutex> lck( pDataConnCB->mtx );
        FillQueue( reader );

        //----------------------------------------------------------------------
        // Pick up a chunk from the front and wait for status
        //----------------------------------------------------------------------
        if( pChunks.empty() )
          return XRootDStatus( stOK, suDone );

        std::unique_ptr<ChunkHandler> ch( pChunks.front() );
        pChunks.pop();
        lck.unlock();

        ch->sem->Wait();

        if( !ch->status.IsOK() )
        {
          log->Debug( UtilityMsg, "Unable read %d bytes at %llu from %s: %s",
                      ch->chunk.GetLength(), (unsigned long long) ch->chunk.GetOffset(),
                      pUrl->GetObfuscatedURL().c_str(), ch->status.ToStr().c_str() );
          delete [] (char *)ch->chunk.GetBuffer();
          CleanUpChunks();
          return ch->status;
        }

        ci = std::move( ch->chunk );
        // if it is a local file update the checksum
        if( pUrl->IsLocalFile() && !pUrl->IsMetalink() && !pContinue )
        {
          if( pCkSumHelper )
            pCkSumHelper->Update( ci.GetBuffer(), ci.GetLength() );

          for( auto cksHelper : pAddCksHelpers )
            cksHelper->Update( ci.GetBuffer(), ci.GetLength() );
        }

        return XRootDStatus( stOK, suContinue );
      }

      //------------------------------------------------------------------------
      // Asynchronous chunk handler
      //------------------------------------------------------------------------
      class ChunkHandler: public XrdCl::ResponseHandler
      {
        public:
          ChunkHandler(): sem( new XrdSysSemaphore(0) ) {}
          virtual ~ChunkHandler() { delete sem; }
          virtual void HandleResponse( XrdCl::XRootDStatus *statusval,
                                       XrdCl::AnyObject    *response )
          {
            this->status = *statusval;
            delete statusval;
            if( response )
            {
              chunk = ToChunk( response );
              delete response;
            }
            sem->Post();
          }

          XrdCl::PageInfo ToChunk( XrdCl::AnyObject *response )
          {
            if( response->Has<XrdCl::PageInfo>() )
            {
              XrdCl::PageInfo *resp = nullptr;
              response->Get( resp );
              return std::move( *resp );
            }
            else
            {
              XrdCl::ChunkInfo *resp = nullptr;
              response->Get( resp );
              return XrdCl::PageInfo( resp->GetOffset(), resp->GetLength(),
                                      resp->GetBuffer() );
            }
          }

        XrdSysSemaphore     *sem;
        XrdCl::PageInfo      chunk;
        XrdCl::XRootDStatus  status;
      };

      const XrdCl::URL          *pUrl;
      XrdCl::File               *pFile;
      int64_t                    pSize;
      int64_t                    pCurrentOffset;
      uint32_t                   pChunkSize;
      uint16_t                   pParallel;
      std::queue<ChunkHandler*>  pChunks;
      std::string                pDataServer;
      uint16_t                   pNbConn;
      uint16_t                   pMaxNbConn;
      bool                       pUsePgRead;
      bool                       pDoServer;

      std::shared_ptr<CancellableJob> pDataConnCB;
  };

  //----------------------------------------------------------------------------
  //! XRootDSourceZip
  //----------------------------------------------------------------------------
  class XRootDSourceZip: public XRootDSource
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      XRootDSourceZip( const std::string &filename,
                       const XrdCl::URL  *archive,
                       uint32_t           chunkSize,
                       uint8_t            parallelChunks,
                       const std::string &ckSumType,
                       const std::vector<std::string> &addcks,
                       bool doserver ):
                      XRootDSource( archive, chunkSize, parallelChunks, ckSumType,
                                    addcks, doserver ),
                      pFilename( filename ),
                      pZipArchive( new XrdCl::ZipArchive() )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~XRootDSourceZip()
      {
        CleanUpChunks();

        XrdCl::WaitFor( XrdCl::CloseArchive( pZipArchive ) );
        delete pZipArchive;
      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();
        log->Debug( UtilityMsg, "Opening %s for reading",
                                pUrl->GetObfuscatedURL().c_str() );

        std::string value;
        DefaultEnv::GetEnv()->GetString( "ReadRecovery", value );
        pZipArchive->SetProperty( "ReadRecovery", value );

        XRootDStatus st = XrdCl::WaitFor( XrdCl::OpenArchive( pZipArchive, pUrl->GetURL(), XrdCl::OpenFlags::Read ) );
        if( !st.IsOK() )
          return st;

        st = pZipArchive->OpenFile( pFilename );
        if( !st.IsOK() )
          return st;

        XrdCl::StatInfo *info = 0;
        st = pZipArchive->Stat( info );
        if( st.IsOK() )
        {
          pSize = info->GetSize();
          delete info;
        }
        else
          return st;

        if( pUrl->IsLocalFile() && !pUrl->IsMetalink() && pCkSumHelper )
        {
          auto st = pCkSumHelper->Initialize();
          if( !st.IsOK() ) return st;
          for( auto cksHelper : pAddCksHelpers )
          {
            st = cksHelper->Initialize();
            if( !st.IsOK() ) return st;
          }
        }

        if( ( !pUrl->IsLocalFile() && !pZipArchive->IsSecure() ) ||
            ( pUrl->IsLocalFile() && pUrl->IsMetalink() ) )
        {
          pZipArchive->GetProperty( "DataServer", pDataServer );
          //--------------------------------------------------------------------
          // Decide whether we can use PgRead
          //--------------------------------------------------------------------
          int val = XrdCl::DefaultCpUsePgWrtRd;
          XrdCl::DefaultEnv::GetEnv()->GetInt( "CpUsePgWrtRd", val );
          pUsePgRead = XrdCl::Utils::HasPgRW( pDataServer ) && ( val == 1 );
        }

        SetOnDataConnectHandler( pZipArchive );

        return XrdCl::XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get a data chunk from the source
      //!
      //! @param  buffer buffer for the data
      //! @param  ci     chunk information
      //! @return        status of the operation
      //!                suContinue - there are some chunks left
      //!                suDone     - no chunks left
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::PageInfo &ci )
      {
        return GetChunkImpl( pZipArchive, ci );
      }

      //------------------------------------------------------------------------
      // Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        return GetCheckSumImpl( checkSum, checkSumType, pCkSumHelper );
      }

      //------------------------------------------------------------------------
      // Get check sum implementation
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSumImpl( std::string           &checkSum,
                                                   std::string           &checkSumType,
                                                   XrdCl::CheckSumHelper *cksHelper )
      {
        // The ZIP archive by default contains a ZCRC32 checksum
        if( checkSumType == "zcrc32" )
        {
          uint32_t cksum = 0;
          auto st = pZipArchive->GetCRC32( pFilename, cksum );
          if( !st.IsOK() ) return st;

          XrdCksData ckSum;
          ckSum.Set( "zcrc32" );
          ckSum.Set( reinterpret_cast<void*>( &cksum ), sizeof( uint32_t ) );
          char cksBuffer[265];
          ckSum.Get( cksBuffer, 256 );
          checkSum  = "zcrc32:";
          checkSum += XrdCl::Utils::NormalizeChecksum( "zcrc32", cksBuffer );
          return st;
        }

        int useMtlnCksum = XrdCl::DefaultZipMtlnCksum;
        XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
        env->GetInt( "ZipMtlnCksum", useMtlnCksum );
        if( useMtlnCksum && pUrl->IsMetalink() )
        {
          XrdCl::RedirectorRegistry &registry   = XrdCl::RedirectorRegistry::Instance();
          XrdCl::VirtualRedirector  *redirector = registry.Get( *pUrl );
          checkSum = redirector->GetCheckSum( checkSumType );
          if( !checkSum.empty() ) return XrdCl::XRootDStatus();
        }

        // if it is a local file we can calculate the checksum ourself
        if( pUrl->IsLocalFile() && !pUrl->IsMetalink() && cksHelper && !pContinue )
          return cksHelper->GetCheckSum( checkSum, checkSumType );

        // if it is a remote file other types of checksum are not supported
        return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported );
      }

      //------------------------------------------------------------------------
      //! Get additional checksums
      //------------------------------------------------------------------------
      std::vector<std::string> GetAddCks()
      {
        std::vector<std::string> ret;
        for( auto cksHelper : pAddCksHelpers )
        {
          std::string type = cksHelper->GetType();
          std::string cks;
          GetCheckSumImpl( cks, type, cksHelper );
          ret.push_back( cks );
        }
        return ret;
      }

      //------------------------------------------------------------------------
      //! Get extended attributes
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetXAttr( std::vector<XrdCl::xattr_t> &xattrs )
      {
        return XrdCl::XRootDStatus();
      }

    private:

      XRootDSourceZip(const XRootDSourceZip &other);
      XRootDSourceZip &operator = (const XRootDSourceZip &other);

      const std::string         pFilename;
      XrdCl::ZipArchive        *pZipArchive;
  };

  //----------------------------------------------------------------------------
  //! XRootDSourceDynamic
  //----------------------------------------------------------------------------
  class XRootDSourceDynamic: public Source
  {
    public:

      //------------------------------------------------------------------------
      //! Try different server
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus TryOtherServer()
      {
        return pFile->TryOtherServer();
      }

      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      XRootDSourceDynamic( const XrdCl::URL *url,
                           uint32_t          chunkSize,
                           const std::string &ckSumType,
                           const std::vector<std::string> &addcks ):
        Source( ckSumType, addcks ),
        pUrl( url ), pFile( new XrdCl::File() ), pCurrentOffset( 0 ),
        pChunkSize( chunkSize ), pDone( false ), pUsePgRead( false )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~XRootDSourceDynamic()
      {
        XrdCl::XRootDStatus status = pFile->Close();
        delete pFile;
      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();
        log->Debug( UtilityMsg, "Opening %s for reading",
                                pUrl->GetObfuscatedURL().c_str() );

        std::string value;
        DefaultEnv::GetEnv()->GetString( "ReadRecovery", value );
        pFile->SetProperty( "ReadRecovery", value );

        XRootDStatus st = pFile->Open( pUrl->GetURL(), OpenFlags::Read );
        if( !st.IsOK() )
          return st;

        if( pUrl->IsLocalFile() && !pUrl->IsMetalink() && pCkSumHelper && !pContinue )
        {
          auto st = pCkSumHelper->Initialize();
          if( !st.IsOK() ) return st;
          for( auto cksHelper : pAddCksHelpers )
          {
            st = cksHelper->Initialize();
            if( !st.IsOK() ) return st;
          }
        }

        if( ( !pUrl->IsLocalFile() && !pFile->IsSecure() ) ||
            ( pUrl->IsLocalFile() && pUrl->IsMetalink() ) )
        {
          std::string datasrv;
          pFile->GetProperty( "DataServer", datasrv );
          //--------------------------------------------------------------------
          // Decide whether we can use PgRead
          //--------------------------------------------------------------------
          int val = XrdCl::DefaultCpUsePgWrtRd;
          XrdCl::DefaultEnv::GetEnv()->GetInt( "CpUsePgWrtRd", val );
          pUsePgRead = XrdCl::Utils::HasPgRW( datasrv ) && ( val == 1 );
        }

        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual int64_t GetSize()
      {
        return -1;
      }

      //------------------------------------------------------------------------
      //! Start reading from the source at given offset
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus StartAt( uint64_t offset )
      {
        pCurrentOffset = offset;
        pContinue      = true;
        return XrdCl::XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get a data chunk from the source
      //!
      //! @param  buffer buffer for the data
      //! @param  ci     chunk information
      //! @return        status of the operation
      //!                suContinue - there are some chunks left
      //!                suDone     - no chunks left
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::PageInfo &ci )
      {
        //----------------------------------------------------------------------
        // Sanity check
        //----------------------------------------------------------------------
        using namespace XrdCl;

        if( pDone )
          return XRootDStatus( stOK, suDone );

        //----------------------------------------------------------------------
        // Fill the queue
        //----------------------------------------------------------------------
        char     *buffer = new char[pChunkSize];
        uint32_t  bytesRead = 0;

        std::vector<uint32_t> cksums;
        XRootDStatus st = pUsePgRead
                        ? pFile->PgRead( pCurrentOffset, pChunkSize, buffer, cksums, bytesRead )
                        : pFile->Read( pCurrentOffset, pChunkSize, buffer, bytesRead );

        if( !st.IsOK() )
        {
          delete [] buffer;
          return st;
        }

        if( !bytesRead )
        {
          delete [] buffer;
          return XRootDStatus( stOK, suDone );
        }

        if( bytesRead < pChunkSize )
          pDone = true;

        // if it is a local file update the checksum
        if( pUrl->IsLocalFile() && !pUrl->IsMetalink() && !pContinue )
        {
          if( pCkSumHelper )
            pCkSumHelper->Update( buffer, bytesRead );

          for( auto cksHelper : pAddCksHelpers )
            cksHelper->Update( buffer, bytesRead );
        }

        ci = XrdCl::PageInfo( pCurrentOffset, bytesRead, buffer );
        pCurrentOffset += bytesRead;

        return XRootDStatus( stOK, suContinue );
      }

      //------------------------------------------------------------------------
      // Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        return GetCheckSumImpl( pCkSumHelper, checkSum, checkSumType );
      }

      XrdCl::XRootDStatus GetCheckSumImpl( XrdCl::CheckSumHelper *cksHelper,
                                           std::string           &checkSum,
                                           std::string           &checkSumType )
      {
        if( pUrl->IsMetalink() )
        {
          XrdCl::RedirectorRegistry &registry   = XrdCl::RedirectorRegistry::Instance();
          XrdCl::VirtualRedirector  *redirector = registry.Get( *pUrl );
          checkSum = redirector->GetCheckSum( checkSumType );
          if( !checkSum.empty() ) return XrdCl::XRootDStatus();
        }

        if( pUrl->IsLocalFile() )
        {
          if( pContinue)
            // in case of --continue option we have to calculate the checksum from scratch
            return XrdCl::Utils::GetLocalCheckSum( checkSum, checkSumType, pUrl->GetPath() );

          if( cksHelper )
            return cksHelper->GetCheckSum( checkSum, checkSumType );

          return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errCheckSumError );
        }

        std::string dataServer; pFile->GetProperty( "DataServer", dataServer );
        std::string lastUrl;    pFile->GetProperty( "LastURL",    lastUrl );
        return XrdCl::Utils::GetRemoteCheckSum( checkSum, checkSumType, XrdCl::URL( lastUrl ) );
      }

      //------------------------------------------------------------------------
      //! Get additional checksums
      //------------------------------------------------------------------------
      std::vector<std::string> GetAddCks()
      {
        std::vector<std::string> ret;
        for( auto cksHelper : pAddCksHelpers )
        {
          std::string type = cksHelper->GetType();
          std::string cks;
          GetCheckSumImpl( cksHelper, cks, type );
          ret.push_back( cks );
        }
        return ret;
      }

      //------------------------------------------------------------------------
      //! Get extended attributes
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetXAttr( std::vector<XrdCl::xattr_t> &xattrs )
      {
        return ::GetXAttr( *pFile, xattrs );
      }

    private:
      XRootDSourceDynamic(const XRootDSourceDynamic &other);
      XRootDSourceDynamic &operator = (const XRootDSourceDynamic &other);
      const XrdCl::URL           *pUrl;
      XrdCl::File                *pFile;
      int64_t                     pCurrentOffset;
      uint32_t                    pChunkSize;
      bool                        pDone;
      bool                        pUsePgRead;
  };

  //----------------------------------------------------------------------------
  //! XRootDSourceDynamic
  //----------------------------------------------------------------------------
  class XRootDSourceXCp: public Source
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      XRootDSourceXCp( const XrdCl::URL* url, uint32_t chunkSize, uint16_t parallelChunks, int32_t nbSrc, uint64_t blockSize ):
        pXCpCtx( 0 ), pUrl( url ), pChunkSize( chunkSize ), pParallelChunks( parallelChunks ), pNbSrc( nbSrc ), pBlockSize( blockSize )
      {
      }

      ~XRootDSourceXCp()
      {
        if( pXCpCtx )
          pXCpCtx->Delete();
      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        XrdCl::Log *log = XrdCl::DefaultEnv::GetLog();
        int64_t fileSize = -1;

        if( pUrl->IsMetalink() )
        {
          XrdCl::RedirectorRegistry &registry = XrdCl::RedirectorRegistry::Instance();
          XrdCl::VirtualRedirector *redirector = registry.Get( *pUrl );
          fileSize = redirector->GetSize();
          pReplicas = redirector->GetReplicas();
        }
        else
        {
          XrdCl::LocationInfo *li = 0;
          XrdCl::FileSystem fs( *pUrl );
          XrdCl::XRootDStatus st = fs.DeepLocate( pUrl->GetPath(), XrdCl::OpenFlags::Compress | XrdCl::OpenFlags::PrefName, li );
          if( !st.IsOK() ) return st;

          XrdCl::LocationInfo::Iterator itr;
          for( itr = li->Begin(); itr != li->End(); ++itr)
          {
            std::string url = "root://" + itr->GetAddress() + "/" + pUrl->GetPath();
            pReplicas.push_back( url );
          }

          delete li;
        }

        std::stringstream ss;
        ss << "XCp sources: ";

        std::vector<std::string>::iterator itr;
        for( itr = pReplicas.begin() ; itr != pReplicas.end() ; ++itr )
        {
          ss << *itr << ", ";
        }
        log->Debug( XrdCl::UtilityMsg, "%s", ss.str().c_str() );

        pXCpCtx = new XrdCl::XCpCtx( pReplicas, pBlockSize, pNbSrc, pChunkSize, pParallelChunks, fileSize );

        return pXCpCtx->Initialize();
      }

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual int64_t GetSize()
      {
        return pXCpCtx->GetSize();
      }

      //------------------------------------------------------------------------
      //! Start reading from the source at given offset
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus StartAt( uint64_t offset )
      {
        return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotImplemented );
      }

      //------------------------------------------------------------------------
      //! Get a data chunk from the source
      //!
      //! @param  buffer buffer for the data
      //! @param  ci     chunk information
      //! @return        status of the operation
      //!                suContinue - there are some chunks left
      //!                suDone     - no chunks left
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::PageInfo &ci )
      {
        XrdCl::XRootDStatus st;
        do
        {
          st = pXCpCtx->GetChunk( ci );
        }
        while( st.IsOK() && st.code == XrdCl::suRetry );
        return st;
      }

      //------------------------------------------------------------------------
      // Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        if( pUrl->IsMetalink() )
        {
          XrdCl::RedirectorRegistry &registry   = XrdCl::RedirectorRegistry::Instance();
          XrdCl::VirtualRedirector  *redirector = registry.Get( *pUrl );
          checkSum = redirector->GetCheckSum( checkSumType );
          if( !checkSum.empty() ) return XrdCl::XRootDStatus();
        }

        std::vector<std::string>::iterator itr;
        for( itr = pReplicas.begin() ; itr != pReplicas.end() ; ++itr )
        {
          XrdCl::URL url( *itr );
          XrdCl::XRootDStatus st = XrdCl::Utils::GetRemoteCheckSum( checkSum,
                              checkSumType, url );
          if( st.IsOK() ) return st;
        }

        return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNoMoreReplicas );
      }

      //------------------------------------------------------------------------
      //! Get additional checksums
      //------------------------------------------------------------------------
      std::vector<std::string> GetAddCks()
      {
        return std::vector<std::string>();
      }

      //------------------------------------------------------------------------
      //! Get extended attributes
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetXAttr( std::vector<XrdCl::xattr_t> &xattrs )
      {
        XrdCl::XRootDStatus st;
        std::vector<std::string>::iterator itr = pReplicas.begin();
        for( ; itr < pReplicas.end() ; ++itr )
        {
          st = ::GetXAttr( *itr, xattrs );
          if( st.IsOK() ) return st;
        }
        return st;
      }

    private:


      XrdCl::XCpCtx            *pXCpCtx;
      const XrdCl::URL         *pUrl;
      std::vector<std::string>  pReplicas;
      uint32_t                  pChunkSize;
      uint16_t                  pParallelChunks;
      int32_t                   pNbSrc;
      uint64_t                  pBlockSize;
  };

  //----------------------------------------------------------------------------
  //! SrdOut destination
  //----------------------------------------------------------------------------
  class StdOutDestination: public Destination
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      StdOutDestination( const std::string &ckSumType ):
        Destination( ckSumType ), pCurrentOffset(0)
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~StdOutDestination()
      {
      }

      //------------------------------------------------------------------------
      //! Initialize the destination
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        if( pContinue )
          return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported,
                                      ENOTSUP, "Cannot continue to stdout." );

        if( pCkSumHelper )
          return pCkSumHelper->Initialize();
        return XrdCl::XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Finalize the destination
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Finalize()
      {
        return XrdCl::XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Put a data chunk at a destination
      //!
      //! @param  ci     chunk information
      //! @return status of the operation
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus PutChunk( XrdCl::PageInfo &&ci )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        if( pCurrentOffset != ci.GetOffset() )
        {
          log->Error( UtilityMsg, "Got out-of-bounds chunk, expected offset:"
                      " %llu, got %llu", (unsigned long long) pCurrentOffset, (unsigned long long) ci.GetOffset() );
          return XRootDStatus( stError, errInternal );
        }

        int64_t   wr     = 0;
        uint32_t  length = ci.GetLength();
        char     *cursor = (char*)ci.GetBuffer();
        do
        {
          wr = write( 1, cursor, length );
          if( wr == -1 )
          {
            log->Debug( UtilityMsg, "Unable to write to stdout: %s",
                        XrdSysE2T( errno ) );
            delete [] (char*)ci.GetBuffer();
            return XRootDStatus( stError, errOSError, errno );
          }
          pCurrentOffset += wr;
          cursor         += wr;
          length         -= wr;
        }
        while( length );

        if( pCkSumHelper )
          pCkSumHelper->Update( ci.GetBuffer(), ci.GetLength() );
        delete [] (char*)ci.GetBuffer();
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Flush chunks that might have been queues
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Flush()
      {
        return XrdCl::XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        if( pCkSumHelper )
          return pCkSumHelper->GetCheckSum( checkSum, checkSumType );
        return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errCheckSumError );
      }

      //------------------------------------------------------------------------
      //! Set extended attributes
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus SetXAttr( const std::vector<XrdCl::xattr_t> &xattrs )
      {
        return XrdCl::XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual int64_t GetSize()
      {
        return -1;
      }

    private:
      StdOutDestination(const StdOutDestination &other);
      StdOutDestination &operator = (const StdOutDestination &other);
      uint64_t       pCurrentOffset;
  };

  //----------------------------------------------------------------------------
  //! XRootD destination
  //----------------------------------------------------------------------------
  class XRootDDestination: public Destination
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      XRootDDestination( const XrdCl::URL &url, uint8_t parallelChunks,
                         const std::string &ckSumType, const XrdCl::ClassicCopyJob &cpjob ):
        Destination( ckSumType ),
        pUrl( url ), pFile( new XrdCl::File( XrdCl::File::DisableVirtRedirect ) ),
        pParallel( parallelChunks ), pSize( -1 ), pUsePgWrt( false ), cpjob( cpjob )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~XRootDDestination()
      {
        CleanUpChunks();
        delete pFile;

        XrdCl::Log *log = XrdCl::DefaultEnv::GetLog();

        //----------------------------------------------------------------------
        // Make sure we clean up the cp-target symlink
        //----------------------------------------------------------------------
        std::string cptarget = XrdCl::DefaultCpTarget;
        XrdCl::DefaultEnv::GetEnv()->GetString( "CpTarget", cptarget );
        if( !cptarget.empty() )
        {
          XrdCl::FileSystem fs( "file://localhost" );
          XrdCl::XRootDStatus st = fs.Rm( cptarget );
          if( !st.IsOK() )
            log->Warning( XrdCl::UtilityMsg, "Could not delete cp-target symlink: %s",
                          st.ToString().c_str() );
        }

        //----------------------------------------------------------------------
        // If the copy failed and user requested posc and we are dealing with
        // a local destination, remove the file
        //----------------------------------------------------------------------
        if( pUrl.IsLocalFile() && pPosc && !cpjob.GetResult().IsOK() )
        {
          XrdCl::FileSystem fs( pUrl );
          XrdCl::XRootDStatus st = fs.Rm( pUrl.GetPath() );
          if( !st.IsOK() )
            log->Error( XrdCl::UtilityMsg, "Failed to remove local destination"
                        " on failure: %s", st.ToString().c_str() );
        }
      }

      //------------------------------------------------------------------------
      //! Initialize the destination
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();
        log->Debug( UtilityMsg, "Opening %s for writing",
                                pUrl.GetObfuscatedURL().c_str() );

        std::string value;
        DefaultEnv::GetEnv()->GetString( "WriteRecovery", value );
        pFile->SetProperty( "WriteRecovery", value );

        OpenFlags::Flags flags = OpenFlags::Update;
        if( pForce )
          flags |= OpenFlags::Delete;
        else if( !pContinue )
          flags |= OpenFlags::New;

        if( pPosc )
          flags |= OpenFlags::POSC;

        if( pCoerce )
          flags |= OpenFlags::Force;

        if( pMakeDir)
          flags |= OpenFlags::MakePath;

        Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;

        XrdCl::XRootDStatus st = pFile->Open( pUrl.GetURL(), flags, mode );
        if( !st.IsOK() )
          return st;

        if( ( !pUrl.IsLocalFile() && !pFile->IsSecure() ) ||
            ( pUrl.IsLocalFile() && pUrl.IsMetalink() ) )
        {
          std::string datasrv;
          pFile->GetProperty( "DataServer", datasrv );
          //--------------------------------------------------------------------
          // Decide whether we can use PgRead
          //--------------------------------------------------------------------
          int val = XrdCl::DefaultCpUsePgWrtRd;
          XrdCl::DefaultEnv::GetEnv()->GetInt( "CpUsePgWrtRd", val );
          pUsePgWrt = XrdCl::Utils::HasPgRW( datasrv ) && ( val == 1 );
        }

        std::string cptarget = XrdCl::DefaultCpTarget;
        XrdCl::DefaultEnv::GetEnv()->GetString( "CpTarget", cptarget );
        if( !cptarget.empty() )
        {
          std::string targeturl;
          pFile->GetProperty( "LastURL", targeturl );
          targeturl = URL( targeturl ).GetLocation();
          if( symlink( targeturl.c_str(), cptarget.c_str() ) == -1 )
            log->Warning( UtilityMsg, "Could not create cp-target symlink: %s",
                          XrdSysE2T( errno ) );
          else
            log->Info( UtilityMsg, "Created cp-target symlink: %s -> %s",
                       cptarget.c_str(), targeturl.c_str() );
        }

        StatInfo *info = 0;
        st = pFile->Stat( false, info );
        if( !st.IsOK() )
          return st;
        pSize = info->GetSize();
        delete info;

        if( pUrl.IsLocalFile() && pCkSumHelper && !pContinue )
          return pCkSumHelper->Initialize();

        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Finalize the destination
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Finalize()
      {
        return pFile->Close();
      }

      //------------------------------------------------------------------------
      //! Put a data chunk at a destination
      //!
      //! @param  ci     chunk information
      //! @return status of the operation
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus PutChunk( XrdCl::PageInfo &&ci )
      {
        using namespace XrdCl;
        if( !pFile->IsOpen() )
        {
          delete[] (char*)ci.GetBuffer(); // we took the ownership of the buffer
          return XRootDStatus( stError, errUninitialized );
        }

        //----------------------------------------------------------------------
        // If there is still place for this chunk to be sent send it
        //----------------------------------------------------------------------
        if( pChunks.size() < pParallel )
          return QueueChunk( std::move( ci ) );

        //----------------------------------------------------------------------
        // We wait for a chunk to be sent so that we have space for the current
        // one
        //----------------------------------------------------------------------
        std::unique_ptr<ChunkHandler> ch( pChunks.front() );
        pChunks.pop();
        ch->sem->Wait();
        delete [] (char*)ch->chunk.GetBuffer();
        if( !ch->status.IsOK() )
        {
          Log *log = DefaultEnv::GetLog();
          log->Debug( UtilityMsg, "Unable write %d bytes at %llu from %s: %s",
                      ch->chunk.GetLength(), (unsigned long long) ch->chunk.GetOffset(),
                      pUrl.GetObfuscatedURL().c_str(), ch->status.ToStr().c_str() );
          delete[] (char*)ci.GetBuffer(); // we took the ownership of the buffer
          CleanUpChunks();

          //--------------------------------------------------------------------
          // Check if we should re-try the transfer from scratch at a different
          // data server
          //--------------------------------------------------------------------
          return CheckIfRetriable( ch->status );
        }

        return QueueChunk( std::move( ci ) );
      }

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual int64_t GetSize()
      {
        return pSize;
      }

      //------------------------------------------------------------------------
      //! Clean up the chunks that are flying
      //------------------------------------------------------------------------
      void CleanUpChunks()
      {
        while( !pChunks.empty() )
        {
          ChunkHandler *ch = pChunks.front();
          pChunks.pop();
          ch->sem->Wait();
          delete [] (char *)ch->chunk.GetBuffer();
          delete ch;
        }
      }

      //------------------------------------------------------------------------
      //! Queue a chunk
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus QueueChunk( XrdCl::PageInfo &&ci )
      {
        // we are writing chunks in order so we can calc the checksum
        // in case of local files
        if( pUrl.IsLocalFile() && pCkSumHelper && !pContinue )
          pCkSumHelper->Update( ci.GetBuffer(), ci.GetLength() );

        ChunkHandler *ch = new ChunkHandler( std::move( ci ) );
        XrdCl::XRootDStatus st;
        st = pUsePgWrt
           ? pFile->PgWrite(ch->chunk.GetOffset(), ch->chunk.GetLength(), ch->chunk.GetBuffer(), ch->chunk.GetCksums(), ch)
           : pFile->Write( ch->chunk.GetOffset(), ch->chunk.GetLength(), ch->chunk.GetBuffer(), ch );
        if( !st.IsOK() )
        {
          CleanUpChunks();
          delete [] (char*)ch->chunk.GetBuffer();
          delete ch;
          return st;
        }
        pChunks.push( ch );
        return XrdCl::XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Flush chunks that might have been queues
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Flush()
      {
        XrdCl::XRootDStatus st;
        while( !pChunks.empty() )
        {
          ChunkHandler *ch = pChunks.front();
          pChunks.pop();
          ch->sem->Wait();
          if( !ch->status.IsOK() )
          {
            //--------------------------------------------------------------------
            // Check if we should re-try the transfer from scratch at a different
            // data server
            //--------------------------------------------------------------------
            st = CheckIfRetriable( ch->status );
          }
          delete [] (char *)ch->chunk.GetBuffer();
          delete ch;
        }
        return st;
      }

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        if( pUrl.IsLocalFile() )
        {
          if( pContinue )
            // in case of --continue option we have to calculate the checksum from scratch
            return XrdCl::Utils::GetLocalCheckSum( checkSum, checkSumType, pUrl.GetPath() );

          if( pCkSumHelper )
            return pCkSumHelper->GetCheckSum( checkSum, checkSumType );

          return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errCheckSumError );
        }

        std::string lastUrl; pFile->GetProperty( "LastURL", lastUrl );
        return XrdCl::Utils::GetRemoteCheckSum( checkSum, checkSumType,
                                                XrdCl::URL( lastUrl ) );
      }

      //------------------------------------------------------------------------
      //! Set extended attributes
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus SetXAttr( const std::vector<XrdCl::xattr_t> &xattrs )
      {
        return ::SetXAttr( *pFile, xattrs );
      }

      //------------------------------------------------------------------------
      //! Get last URL
      //------------------------------------------------------------------------
      const std::string& GetLastURL() const
      {
        return pLastURL;
      }

      //------------------------------------------------------------------------
      //! Get write-recovery redirector
      //------------------------------------------------------------------------
      const std::string& GetWrtRecoveryRedir() const
      {
        return pWrtRecoveryRedir;
      }

    private:
      XRootDDestination(const XRootDDestination &other);
      XRootDDestination &operator = (const XRootDDestination &other);

      //------------------------------------------------------------------------
      // Asynchronous chunk handler
      //------------------------------------------------------------------------
      class ChunkHandler: public XrdCl::ResponseHandler
      {
        public:
          ChunkHandler( XrdCl::PageInfo &&ci ):
            sem( new XrdSysSemaphore(0) ),
            chunk(std::move( ci ) ) {}
          virtual ~ChunkHandler() { delete sem; }
          virtual void HandleResponse( XrdCl::XRootDStatus *statusval,
                                       XrdCl::AnyObject    */*response*/ )
          {
            this->status = *statusval;
            delete statusval;
            sem->Post();
          }

          XrdSysSemaphore        *sem;
          XrdCl::PageInfo         chunk;
          XrdCl::XRootDStatus     status;
      };

      inline XrdCl::XRootDStatus CheckIfRetriable( XrdCl::XRootDStatus &status )
      {
        if( status.IsOK() ) return status;

        //--------------------------------------------------------------------
        // Check if we should re-try the transfer from scratch at a different
        // data server
        //--------------------------------------------------------------------
        std::string value;
        if( pFile->GetProperty( "WrtRecoveryRedir", value ) )
        {
          pWrtRecoveryRedir = value;
          if( pFile->GetProperty( "LastURL", value ) ) pLastURL = value;
          return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errRetry );
        }

        return status;
      }

      const XrdCl::URL             pUrl;
      XrdCl::File                 *pFile;
      uint8_t                      pParallel;
      std::queue<ChunkHandler *>   pChunks;
      int64_t                      pSize;

      std::string                  pWrtRecoveryRedir;
      std::string                  pLastURL;
      bool                         pUsePgWrt;
      const XrdCl::ClassicCopyJob &cpjob;
  };

  //----------------------------------------------------------------------------
  //! XRootD destination
  //----------------------------------------------------------------------------
  class XRootDZipDestination: public Destination
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      XRootDZipDestination( const XrdCl::URL &url, const std::string &fn,
                            int64_t size, uint8_t parallelChunks, XrdCl::ClassicCopyJob &cpjob ):
        Destination( "zcrc32" ),
        pUrl( url ), pFilename( fn ), pZip( new XrdCl::ZipArchive() ),
        pParallel( parallelChunks ), pSize( size ), cpjob( cpjob )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~XRootDZipDestination()
      {
        CleanUpChunks();
        delete pZip;

        //----------------------------------------------------------------------
        // If the copy failed and user requested posc and we are dealing with
        // a local destination, remove the file
        //----------------------------------------------------------------------
        if( pUrl.IsLocalFile() && pPosc && !cpjob.GetResult().IsOK() )
        {
          XrdCl::FileSystem fs( pUrl );
          XrdCl::XRootDStatus st = fs.Rm( pUrl.GetPath() );
          if( !st.IsOK() )
          {
            XrdCl::Log *log = XrdCl::DefaultEnv::GetLog();
            log->Error( XrdCl::UtilityMsg, "Failed to remove local destination"
                        " on failure: %s", st.ToString().c_str() );
          }
        }
      }

      //------------------------------------------------------------------------
      //! Initialize the destination
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();
        log->Debug( UtilityMsg, "Opening %s for writing",
                                pUrl.GetObfuscatedURL().c_str() );

        std::string value;
        DefaultEnv::GetEnv()->GetString( "WriteRecovery", value );
        pZip->SetProperty( "WriteRecovery", value );

        OpenFlags::Flags flags = OpenFlags::Update;

        FileSystem fs( pUrl );
        StatInfo *info = nullptr;
        auto st = fs.Stat( pUrl.GetPath(), info );
        if( !st.IsOK() && st.code == errErrorResponse && st.errNo == kXR_NotFound )
          flags |= OpenFlags::New;

        if( pPosc )
          flags |= OpenFlags::POSC;

        if( pCoerce )
          flags |= OpenFlags::Force;

        if( pMakeDir)
          flags |= OpenFlags::MakePath;

        st = XrdCl::WaitFor( XrdCl::OpenArchive( pZip, pUrl.GetURL(), flags ) );
        if( !st.IsOK() )
          return st;

        std::string cptarget = XrdCl::DefaultCpTarget;
        XrdCl::DefaultEnv::GetEnv()->GetString( "CpTarget", cptarget );
        if( !cptarget.empty() )
        {
          std::string targeturl;
          pZip->GetProperty( "LastURL", targeturl );
          if( symlink( targeturl.c_str(), cptarget.c_str() ) == -1 )
            log->Warning( UtilityMsg, "Could not create cp-target symlink: %s",
                          XrdSysE2T( errno ) );
        }

        st = pZip->OpenFile( pFilename, XrdCl::OpenFlags::New | XrdCl::OpenFlags::Write, pSize );
        if( !st.IsOK() )
          return st;

        return pCkSumHelper->Initialize();
      }

      //------------------------------------------------------------------------
      //! Finalize the destination
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Finalize()
      {
        uint32_t crc32 = 0;
        auto st = pCkSumHelper->GetRawCheckSum( "zcrc32", crc32 );
        if( !st.IsOK() ) return st;
        pZip->UpdateMetadata( crc32 );
        pZip->CloseFile();
        return XrdCl::WaitFor( XrdCl::CloseArchive( pZip ) );
      }

      //------------------------------------------------------------------------
      //! Put a data chunk at a destination
      //!
      //! @param  ci     chunk information
      //! @return status of the operation
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus PutChunk( XrdCl::PageInfo &&ci )
      {
        using namespace XrdCl;

        //----------------------------------------------------------------------
        // If there is still place for this chunk to be sent send it
        //----------------------------------------------------------------------
        if( pChunks.size() < pParallel )
          return QueueChunk( std::move( ci ) );

        //----------------------------------------------------------------------
        // We wait for a chunk to be sent so that we have space for the current
        // one
        //----------------------------------------------------------------------
        std::unique_ptr<ChunkHandler> ch( pChunks.front() );
        pChunks.pop();
        ch->sem->Wait();
        delete [] (char*)ch->chunk.GetBuffer();
        if( !ch->status.IsOK() )
        {
          Log *log = DefaultEnv::GetLog();
          log->Debug( UtilityMsg, "Unable write %d bytes at %llu from %s: %s",
                      ch->chunk.GetLength(), (unsigned long long) ch->chunk.GetOffset(),
                      pUrl.GetObfuscatedURL().c_str(), ch->status.ToStr().c_str() );
          CleanUpChunks();

          //--------------------------------------------------------------------
          // Check if we should re-try the transfer from scratch at a different
          // data server
          //--------------------------------------------------------------------
          return CheckIfRetriable( ch->status );
        }

        return QueueChunk( std::move( ci ) );
      }

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual int64_t GetSize()
      {
        return -1;
      }

      //------------------------------------------------------------------------
      //! Clean up the chunks that are flying
      //------------------------------------------------------------------------
      void CleanUpChunks()
      {
        while( !pChunks.empty() )
        {
          ChunkHandler *ch = pChunks.front();
          pChunks.pop();
          ch->sem->Wait();
          delete [] (char *)ch->chunk.GetBuffer();
          delete ch;
        }
      }

      //------------------------------------------------------------------------
      // Queue a chunk
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus QueueChunk( XrdCl::PageInfo &&ci )
      {
        // we are writing chunks in order so we can calc the checksum
        // in case of local files
        if( pCkSumHelper ) pCkSumHelper->Update( ci.GetBuffer(), ci.GetLength() );

        ChunkHandler *ch = new ChunkHandler( std::move( ci ) );
        XrdCl::XRootDStatus st;

        //----------------------------------------------------------------------
        // TODO
        // In order to use PgWrite with ZIP append we need first to implement
        // PgWriteV!!!
        //----------------------------------------------------------------------
        st = pZip->Write( ch->chunk.GetLength(), ch->chunk.GetBuffer(), ch );
        if( !st.IsOK() )
        {
          CleanUpChunks();
          delete [] (char*)ch->chunk.GetBuffer();
          delete ch;
          return st;
        }
        pChunks.push( ch );
        return XrdCl::XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Flush chunks that might have been queues
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Flush()
      {
        XrdCl::XRootDStatus st;
        while( !pChunks.empty() )
        {
          ChunkHandler *ch = pChunks.front();
          pChunks.pop();
          ch->sem->Wait();
          if( !ch->status.IsOK() )
          {
            //--------------------------------------------------------------------
            // Check if we should re-try the transfer from scratch at a different
            // data server
            //--------------------------------------------------------------------
            st = CheckIfRetriable( ch->status );
          }
          delete [] (char *)ch->chunk.GetBuffer();
          delete ch;
        }
        return st;
      }

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported );
      }

      //------------------------------------------------------------------------
      //! Set extended attributes
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus SetXAttr( const std::vector<XrdCl::xattr_t> &xattrs )
      {
        return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported );
      }

      //------------------------------------------------------------------------
      //! Get last URL
      //------------------------------------------------------------------------
      const std::string& GetLastURL() const
      {
        return pLastURL;
      }

      //------------------------------------------------------------------------
      //! Get write-recovery redirector
      //------------------------------------------------------------------------
      const std::string& GetWrtRecoveryRedir() const
      {
        return pWrtRecoveryRedir;
      }

    private:
      XRootDZipDestination(const XRootDDestination &other);
      XRootDZipDestination &operator = (const XRootDDestination &other);

      //------------------------------------------------------------------------
      // Asynchronous chunk handler
      //------------------------------------------------------------------------
      class ChunkHandler: public XrdCl::ResponseHandler
      {
        public:
          ChunkHandler( XrdCl::PageInfo &&ci ):
            sem( new XrdSysSemaphore(0) ),
            chunk( std::move( ci ) ) {}
          virtual ~ChunkHandler() { delete sem; }
          virtual void HandleResponse( XrdCl::XRootDStatus *statusval,
                                       XrdCl::AnyObject    */*response*/ )
          {
            this->status = *statusval;
            delete statusval;
            sem->Post();
          }

          XrdSysSemaphore        *sem;
          XrdCl::PageInfo         chunk;
          XrdCl::XRootDStatus     status;
      };

      inline XrdCl::XRootDStatus CheckIfRetriable( XrdCl::XRootDStatus &status )
      {
        if( status.IsOK() ) return status;

        //--------------------------------------------------------------------
        // Check if we should re-try the transfer from scratch at a different
        // data server
        //--------------------------------------------------------------------
        std::string value;
        if( pZip->GetProperty( "WrtRecoveryRedir", value ) )
        {
          pWrtRecoveryRedir = value;
          if( pZip->GetProperty( "LastURL", value ) ) pLastURL = value;
          return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errRetry );
        }

        return status;
      }

      const XrdCl::URL            pUrl;
      std::string                 pFilename;
      XrdCl::ZipArchive          *pZip;
      uint8_t                     pParallel;
      std::queue<ChunkHandler *>  pChunks;
      int64_t                     pSize;

      std::string                 pWrtRecoveryRedir;
      std::string                 pLastURL;
      XrdCl::ClassicCopyJob      &cpjob;
  };
}

//------------------------------------------------------------------------------
// Get current time in nanoseconds
//------------------------------------------------------------------------------
inline std::chrono::nanoseconds time_nsec()
{
  using namespace std::chrono;
  auto since_epoch = high_resolution_clock::now().time_since_epoch();
  return duration_cast<nanoseconds>( since_epoch );
}

//------------------------------------------------------------------------------
// Convert seconds to nanoseconds
//------------------------------------------------------------------------------
inline long long to_nsec( long long sec )
{
  return sec * 1000000000;
}

//------------------------------------------------------------------------------
// Sleep for # nanoseconds
//------------------------------------------------------------------------------
inline void sleep_nsec( long long nsec )
{
#if __cplusplus >= 201103L
  using namespace std::chrono;
  std::this_thread::sleep_for( nanoseconds( nsec ) );
#else
  timespec req;
  req.tv_sec  = nsec / to_nsec( 1 );
  req.tv_nsec = nsec % to_nsec( 1 );
  nanosleep( &req, 0 );
#endif
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  ClassicCopyJob::ClassicCopyJob( uint32_t      jobId,
                                  PropertyList *jobProperties,
                                  PropertyList *jobResults ):
    CopyJob( jobId, jobProperties, jobResults )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Creating a classic copy job, from %s to %s",
                GetSource().GetObfuscatedURL().c_str(), GetTarget().GetObfuscatedURL().c_str() );
  }

  //----------------------------------------------------------------------------
  // Run the copy job
  //----------------------------------------------------------------------------
  XRootDStatus ClassicCopyJob::Run( CopyProgressHandler *progress )
  {
    Log *log = DefaultEnv::GetLog();

    std::string checkSumMode;
    std::string checkSumType;
    std::string checkSumPreset;
    std::string zipSource;
    uint16_t    parallelChunks;
    uint32_t    chunkSize;
    uint64_t    blockSize;
    bool        posc, force, coerce, makeDir, dynamicSource, zip, xcp, preserveXAttr,
                rmOnBadCksum, continue_, zipappend, doserver;
    int32_t     nbXcpSources;
    long long   xRate;
    long long   xRateThreshold;
    uint16_t    cpTimeout;
    std::vector<std::string> addcksums;

    pProperties->Get( "checkSumMode",    checkSumMode );
    pProperties->Get( "checkSumType",    checkSumType );
    pProperties->Get( "checkSumPreset",  checkSumPreset );
    pProperties->Get( "parallelChunks",  parallelChunks );
    pProperties->Get( "chunkSize",       chunkSize );
    pProperties->Get( "posc",            posc );
    pProperties->Get( "force",           force );
    pProperties->Get( "coerce",          coerce );
    pProperties->Get( "makeDir",         makeDir );
    pProperties->Get( "dynamicSource",   dynamicSource );
    pProperties->Get( "zipArchive",      zip );
    pProperties->Get( "xcp",             xcp );
    pProperties->Get( "xcpBlockSize",    blockSize );
    pProperties->Get( "preserveXAttr",   preserveXAttr );
    pProperties->Get( "xrate",           xRate );
    pProperties->Get( "xrateThreshold",  xRateThreshold );
    pProperties->Get( "rmOnBadCksum",    rmOnBadCksum );
    pProperties->Get( "continue",        continue_ );
    pProperties->Get( "cpTimeout",       cpTimeout );
    pProperties->Get( "zipAppend",       zipappend );
    pProperties->Get( "addcksums",       addcksums );
    pProperties->Get( "doServer",        doserver );

    if( zip )
      pProperties->Get( "zipSource",     zipSource );

    if( xcp )
      pProperties->Get( "nbXcpSources",  nbXcpSources );

    if( force && continue_ )
      return SetResult( stError, errInvalidArgs, EINVAL,
                     "Invalid argument combination: continue + force." );

    if( zipappend && ( continue_ || force ) )
      return SetResult( stError, errInvalidArgs, EINVAL,
                     "Invalid argument combination: ( continue | force ) + zip-append." );

    //--------------------------------------------------------------------------
    // Start the cp t/o timer if necessary
    //--------------------------------------------------------------------------
    std::unique_ptr<timer_sec_t> cptimer;
    if( cpTimeout ) cptimer.reset( new timer_sec_t() );

    //--------------------------------------------------------------------------
    // Remove on bad checksum implies that POSC semantics has to be enabled
    //--------------------------------------------------------------------------
    if( rmOnBadCksum ) posc = true;

    //--------------------------------------------------------------------------
    // Resolve the 'auto' checksum type.
    //--------------------------------------------------------------------------
    if( checkSumType == "auto" )
    {
      checkSumType = Utils::InferChecksumType( GetSource(), GetTarget(), zip );
      if( checkSumType.empty() )
        return SetResult( stError, errCheckSumError, ENOTSUP, "Could not infer checksum type." );
      else
        log->Info( UtilityMsg, "Using inferred checksum type: %s.", checkSumType.c_str() );
    }

    if( cptimer && cptimer->elapsed() > cpTimeout ) // check the CP timeout
      return SetResult( stError, errOperationExpired, 0, "CPTimeout exceeded." );

    //--------------------------------------------------------------------------
    // Initialize the source and the destination
    //--------------------------------------------------------------------------
    std::unique_ptr<Source> src;
    if( xcp )
      src.reset( new XRootDSourceXCp( &GetSource(), chunkSize, parallelChunks, nbXcpSources, blockSize ) );
    else if( zip ) // TODO make zip work for xcp
      src.reset( new XRootDSourceZip( zipSource, &GetSource(), chunkSize, parallelChunks,
                                      checkSumType, addcksums , doserver) );
    else if( GetSource().GetProtocol() == "stdio" )
      src.reset( new StdInSource( checkSumType, chunkSize, addcksums ) );
    else
    {
      if( dynamicSource )
        src.reset( new XRootDSourceDynamic( &GetSource(), chunkSize, checkSumType, addcksums ) );
      else
        src.reset( new XRootDSource( &GetSource(), chunkSize, parallelChunks, checkSumType, addcksums, doserver ) );
    }

    XRootDStatus st = src->Initialize();
    if( !st.IsOK() ) return SourceError( st );
    uint64_t size = src->GetSize() >= 0 ? src->GetSize() : 0;

    if( cptimer && cptimer->elapsed() > cpTimeout ) // check the CP timeout
      return SetResult( stError, errOperationExpired, 0, "CPTimeout exceeded." );

    std::unique_ptr<Destination> dest;
    URL newDestUrl( GetTarget() );

    if( GetTarget().GetProtocol() == "stdio" )
      dest.reset( new StdOutDestination( checkSumType ) );
    else if( zipappend )
    {
      std::string fn = GetSource().GetPath();
      size_t pos = fn.rfind( '/' );
      if( pos != std::string::npos )
        fn = fn.substr( pos + 1 );
      int64_t size = src->GetSize();
      dest.reset( new XRootDZipDestination( newDestUrl, fn, size, parallelChunks, *this ) );
    }
    //--------------------------------------------------------------------------
    // For xrootd destination build the oss.asize hint
    //--------------------------------------------------------------------------
    else
    {
      if( src->GetSize() >= 0 )
      {
        URL::ParamsMap params = newDestUrl.GetParams();
        std::ostringstream o; o << src->GetSize();
        params["oss.asize"] = o.str();
        newDestUrl.SetParams( params );
 //     makeDir = true; // Backward compatibility for xroot destinations!!!
      }
      dest.reset( new XRootDDestination( newDestUrl, parallelChunks, checkSumType, *this ) );
    }

    dest->SetForce( force );
    dest->SetPOSC(  posc );
    dest->SetCoerce( coerce );
    dest->SetMakeDir( makeDir );
    dest->SetContinue( continue_ );
    st = dest->Initialize();
    if( !st.IsOK() ) return DestinationError( st );

    if( cptimer && cptimer->elapsed() > cpTimeout ) // check the CP timeout
      return SetResult( stError, errOperationExpired, 0, "CPTimeout exceeded." );

    //--------------------------------------------------------------------------
    // Copy the chunks
    //--------------------------------------------------------------------------
    if( continue_ )
    {
      size -= dest->GetSize();
      XrdCl::XRootDStatus st = src->StartAt( dest->GetSize() );
      if( !st.IsOK() ) return SetResult( st );
    }

    PageInfo  pageInfo;
    uint64_t  total_processed = 0;
    uint64_t  processed = 0;
    auto      start = time_nsec();
    uint16_t  threshold_interval = parallelChunks;
    bool      threshold_draining = false;
    timer_nsec_t threshold_timer;
    while( 1 )
    {
      st = src->GetChunk( pageInfo );
      if( !st.IsOK() )
        return SourceError( st);

      if( st.IsOK() && st.code == suDone )
        break;

      if( cptimer && cptimer->elapsed() > cpTimeout ) // check the CP timeout
        return SetResult( stError, errOperationExpired, 0, "CPTimeout exceeded." );

      if( xRate )
      {
        auto   elapsed     = ( time_nsec() - start ).count();
        double transferred = total_processed + pageInfo.GetLength();
        double expected    = double( xRate ) / to_nsec( 1 ) * elapsed;
        //----------------------------------------------------------------------
        // check if our transfer rate didn't exceeded the limit
        // (we are too fast)
        //----------------------------------------------------------------------
        if( elapsed && // make sure elapsed time is greater than 0
            transferred > expected )
        {
          auto nsec = ( transferred / xRate * to_nsec( 1 ) ) - elapsed;
          sleep_nsec( nsec );
        }
      }

      if( xRateThreshold )
      {
        auto   elapsed     = threshold_timer.elapsed();
        double transferred = processed + pageInfo.GetLength();
        double expected    = double( xRateThreshold ) / to_nsec( 1 ) * elapsed;
        //----------------------------------------------------------------------
        // check if our transfer rate dropped below the threshold
        // (we are too slow)
        //----------------------------------------------------------------------
        if( elapsed && // make sure elapsed time is greater than 0
            transferred < expected &&
            threshold_interval == 0 ) // we check every # parallelChunks
        {
          if( !threshold_draining )
          {
            log->Warning( UtilityMsg, "Transfer rate dropped below requested ehreshold,"
                                      " trying different source!" );
            XRootDStatus st = src->TryOtherServer();
            if( !st.IsOK() ) return SetResult( stError, errThresholdExceeded, 0,
                                            "The transfer rate dropped below "
                                            "requested threshold!" );
            threshold_draining = true; // before the next measurement we need to drain
                                       // all the chunks that will come from the old server
          }
          else // now that all the chunks from the old server have
          {    // been received we can start another measurement
            processed = 0;
            threshold_timer.reset();
            threshold_interval = parallelChunks;
            threshold_draining = false;
          }
        }

        threshold_interval = threshold_interval > 0 ? threshold_interval - 1 : parallelChunks;
      }

      total_processed += pageInfo.GetLength();
      processed       += pageInfo.GetLength();

      st = dest->PutChunk( std::move( pageInfo ) );
      if( !st.IsOK() )
      {
        if( st.code == errRetry )
        {
          pResults->Set( "LastURL", dest->GetLastURL() );
          pResults->Set( "WrtRecoveryRedir", dest->GetWrtRecoveryRedir() );
          return SetResult( st );
        }
        return DestinationError( st );
      }

      if( progress )
      {
        progress->JobProgress( pJobId, total_processed, size );
        if( progress->ShouldCancel( pJobId ) )
          return SetResult( stError, errOperationInterrupted, kXR_Cancelled, "The copy-job has been cancelled!" );
      }
    }

    st = dest->Flush();
    if( !st.IsOK() )
      return DestinationError( st );

    //--------------------------------------------------------------------------
    // Copy extended attributes
    //--------------------------------------------------------------------------
    if( preserveXAttr && Utils::HasXAttr( GetSource() ) && Utils::HasXAttr( GetTarget() ) )
    {
      std::vector<xattr_t> xattrs;
      st = src->GetXAttr( xattrs );
      if( !st.IsOK() ) return SourceError( st );
      st = dest->SetXAttr( xattrs );
      if( !st.IsOK() ) return DestinationError( st );
    }

    //--------------------------------------------------------------------------
    // The size of the source is known and not enough data has been transferred
    // to the destination
    //--------------------------------------------------------------------------
    if( src->GetSize() >= 0 && size != total_processed )
    {
      log->Error( UtilityMsg, "The declared source size is %llu bytes, but "
                  "received %llu bytes.", (unsigned long long) size, (unsigned long long) total_processed );
      return SetResult( stError, errDataError );
    }
    pResults->Set( "size", total_processed );

    //--------------------------------------------------------------------------
    // Finalize the destination
    //--------------------------------------------------------------------------
    st = dest->Finalize();
    if( !st.IsOK() )
      return DestinationError( st );

    //--------------------------------------------------------------------------
    // Verify the checksums if needed
    //--------------------------------------------------------------------------
    if( checkSumMode != "none" )
    {
      log->Debug( UtilityMsg, "Attempting checksum calculation, mode: %s.",
                  checkSumMode.c_str() );
      std::string sourceCheckSum;
      std::string targetCheckSum;

      if( cptimer && cptimer->elapsed() > cpTimeout ) // check the CP timeout
        return SetResult( stError, errOperationExpired, 0, "CPTimeout exceeded." );

      //------------------------------------------------------------------------
      // Get the check sum at source
      //------------------------------------------------------------------------
      timeval oStart, oEnd;
      XRootDStatus st;

      if( checkSumMode == "end2end" || checkSumMode == "source" ||
          !checkSumPreset.empty() )
      {
        gettimeofday( &oStart, 0 );
        if( !checkSumPreset.empty() )
        {
          sourceCheckSum  = checkSumType + ":";
          sourceCheckSum += Utils::NormalizeChecksum( checkSumType,
                                                      checkSumPreset );
        }
        else
        {
          st = src->GetCheckSum( sourceCheckSum, checkSumType );
        }
        gettimeofday( &oEnd, 0 );

        if( !st.IsOK() )
          return SourceError( st );

        pResults->Set( "sourceCheckSum", sourceCheckSum );
      }

      if( !addcksums.empty() )
        pResults->Set( "additionalCkeckSum", src->GetAddCks() );

      if( cptimer && cptimer->elapsed() > cpTimeout ) // check the CP timeout
        return SetResult( stError, errOperationExpired, 0, "CPTimeout exceeded." );

      //------------------------------------------------------------------------
      // Get the check sum at destination
      //------------------------------------------------------------------------
      timeval tStart, tEnd;

      if( checkSumMode == "end2end" || checkSumMode == "target" )
      {
        gettimeofday( &tStart, 0 );
        st = dest->GetCheckSum( targetCheckSum, checkSumType );
        if( !st.IsOK() )
          return DestinationError( st );
        gettimeofday( &tEnd, 0 );
        pResults->Set( "targetCheckSum", targetCheckSum );
      }

      if( cptimer && cptimer->elapsed() > cpTimeout ) // check the CP timeout
        return SetResult( stError, errOperationExpired, 0, "CPTimeout exceeded." );

      //------------------------------------------------------------------------
      // Make sure the checksums are both lower case
      //------------------------------------------------------------------------
      auto sanitize_cksum = []( char c )
                            {
                              std::locale loc;
                              if( std::isalpha( c ) ) return std::tolower( c, loc );
                              return c;
                            };

      std::transform( sourceCheckSum.begin(), sourceCheckSum.end(),
                      sourceCheckSum.begin(), sanitize_cksum );

      std::transform( targetCheckSum.begin(), targetCheckSum.end(),
                      targetCheckSum.begin(), sanitize_cksum );

      //------------------------------------------------------------------------
      // Compare and inform monitoring
      //------------------------------------------------------------------------
      if( !sourceCheckSum.empty() && !targetCheckSum.empty() )
      {
        bool match = false;
        if( sourceCheckSum == targetCheckSum )
          match = true;

        Monitor *mon = DefaultEnv::GetMonitor();
        if( mon )
        {
          Monitor::CheckSumInfo i;
          i.transfer.origin = &GetSource();
          i.transfer.target = &GetTarget();
          i.cksum           = sourceCheckSum;
          i.oTime           = Utils::GetElapsedMicroSecs( oStart, oEnd );
          i.tTime           = Utils::GetElapsedMicroSecs( tStart, tEnd );
          i.isOK            = match;
          mon->Event( Monitor::EvCheckSum, &i );
        }

        if( !match )
        {
          if( rmOnBadCksum )
          {
            FileSystem fs( newDestUrl );
            st = fs.Rm( newDestUrl.GetPath() );
            if( !st.IsOK() )
              log->Error( UtilityMsg, "Invalid checksum: failed to remove the target file: %s", st.ToString().c_str() );
            else
              log->Info( UtilityMsg, "Target file removed due to bad checksum!" );
          }

          st = dest->Finalize();
          if( !st.IsOK() )
            log->Error( UtilityMsg, "Failed to finalize the destination: %s", st.ToString().c_str() );

          return SetResult( stError, errCheckSumError, 0 );
        }

        log->Info( UtilityMsg, "Checksum verification: succeeded." );
      }
    }

    return SetResult();
  }
}
