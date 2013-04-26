//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
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

#include <memory>
#include <iostream>
#include <queue>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

namespace
{
  //----------------------------------------------------------------------------
  //! Check sum helper for stdio
  //----------------------------------------------------------------------------
  class CheckSumHelper
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      CheckSumHelper( const std::string &name,
                      const std::string &ckSumType ):
        pName( name ),
        pCkSumType( ckSumType ),
        pCksCalcObj( 0 )
      {};

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~CheckSumHelper()
      {
        delete pCksCalcObj;
      }

      //------------------------------------------------------------------------
      //! Initialize
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus Initialize()
      {
        using namespace XrdCl;
        if( pCkSumType.empty() )
          return XRootDStatus();

        Log             *log    = DefaultEnv::GetLog();
        CheckSumManager *cksMan = DefaultEnv::GetCheckSumManager();

        if( !cksMan )
        {
          log->Error( UtilityMsg, "Unable to get the checksum manager" );
          return XRootDStatus( stError, errInternal );
        }

        pCksCalcObj = cksMan->GetCalculator( pCkSumType );
        if( !pCksCalcObj )
        {
          log->Error( UtilityMsg, "Unable to get a calculator for %s",
                      pCkSumType.c_str() );
          return XRootDStatus( stError, errCheckSumError );
        }

        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      // Update the checksum
      //------------------------------------------------------------------------
      void Update( const void *buffer, uint32_t size )
      {
        if( pCksCalcObj )
          pCksCalcObj->Update( (const char *)buffer, size );
      }

      //------------------------------------------------------------------------
      // Get checksum
      //------------------------------------------------------------------------
      XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                       std::string &checkSumType )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        //----------------------------------------------------------------------
        // Sanity check
        //----------------------------------------------------------------------
        if( !pCksCalcObj )
        {
          log->Error( UtilityMsg, "Calculator for %s was not initialized",
                      pCkSumType.c_str() );
          return XRootDStatus( stError, errCheckSumError );
        }

        int          calcSize = 0;
        std::string  calcType = pCksCalcObj->Type( calcSize );

        if( calcType != checkSumType )
        {
          log->Error( UtilityMsg, "Calculated checksum: %s, requested "
                      "checksum: %s", pCkSumType.c_str(),
                      checkSumType.c_str() );
          return XRootDStatus( stError, errCheckSumError );
        }

        //----------------------------------------------------------------------
        // Response
        //----------------------------------------------------------------------
        XrdCksData ckSum;
        ckSum.Set( checkSumType.c_str() );
        ckSum.Set( (void*)pCksCalcObj->Final(), calcSize );
        char *cksBuffer = new char[265];
        ckSum.Get( cksBuffer, 256 );
        checkSum  = checkSumType + ":";
        checkSum += cksBuffer;
        delete [] cksBuffer;

        log->Dump( UtilityMsg, "Checksum for %s is: %s", pName.c_str(),
                   checkSum.c_str() );
        return XrdCl::XRootDStatus();
      }

    private:
      std::string  pName;
      std::string  pCkSumType;
      XrdCksCalc  *pCksCalcObj;
  };

  //----------------------------------------------------------------------------
  //! Abstract chunk source
  //----------------------------------------------------------------------------
  class Source
  {
    public:
      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      virtual ~Source() {};

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize() = 0;

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual int64_t GetSize() = 0;

      //------------------------------------------------------------------------
      //! Get a data chunk from the source
      //!
      //! @param  ci     chunk information
      //! @return        status of the operation
      //!                suContinue - there are some chunks left
      //!                suDone     - no chunks left
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::ChunkInfo &ci ) = 0;

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType ) = 0;
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
      Destination():
        pPosc( false ), pForce( false ), pCoerce( false ) {}

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~Destination() {}

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize() = 0;

      //------------------------------------------------------------------------
      //! Put a data chunk at a destination
      //!
      //! @param  ci     chunk information
      //! @return status of the operation
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus PutChunk( const XrdCl::ChunkInfo &ci ) = 0;

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType ) = 0;

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
      //! Set coerce
      //------------------------------------------------------------------------
      void SetCoerce( bool coerce )
      {
        pCoerce = coerce;
      }

    protected:
      bool pPosc;
      bool pForce;
      bool pCoerce;
  };

  //----------------------------------------------------------------------------
  //! Local source
  //----------------------------------------------------------------------------
  class LocalSource: public Source
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      LocalSource( const XrdCl::URL *url ):
        pPath( url->GetPath() ), pFD( -1 ), pSize( -1 ), pCurrentOffset( 0 ) {}

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~LocalSource()
      {
        if( pFD != -1 )
          close( pFD );
      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        //----------------------------------------------------------------------
        // Open the file for reading and get it's size
        //----------------------------------------------------------------------
        log->Debug( UtilityMsg, "Opening %s for reading", pPath.c_str() );

        int fd = open( pPath.c_str(), O_RDONLY );
        if( fd == -1 )
        {
          log->Debug( UtilityMsg, "Unable to open %s: %s",
                                  pPath.c_str(), strerror( errno ) );
          return XRootDStatus( stError, errOSError, errno );
        }

        struct stat st;
        if( fstat( fd, &st ) == -1 )
        {
          log->Debug( UtilityMsg, "Unable to stat %s: %s",
                                  pPath.c_str(), strerror( errno ) );
          close( fd );
          return XRootDStatus( stError, errOSError, errno );
        }
        pFD   = fd;
        pSize = st.st_size;

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
      //! Get a data chunk from the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::ChunkInfo &ci )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        if( pFD == -1 )
          return XRootDStatus( stError, errUninitialized );

        const uint32_t toRead = 2*1024*1024;
        char *buffer = new char[toRead];

        int64_t bytesRead = read( pFD, buffer, toRead );
        if( bytesRead == -1 )
        {
          log->Debug( UtilityMsg, "Unable to read from %s: %s",
                                  pPath.c_str(), strerror( errno ) );
          close( pFD );
          pFD = -1;
          delete [] buffer;
          return XRootDStatus( stError, errOSError, errno );
        }

        if( bytesRead == 0 )
        {
          delete [] buffer;
          return XRootDStatus( stOK, suDone );
        }

        ci.offset = pCurrentOffset;
        ci.length = bytesRead;
        ci.buffer = buffer;
        pCurrentOffset += bytesRead;
        return XRootDStatus( stOK, suContinue );
      }

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        return XrdCl::Utils::GetLocalCheckSum( checkSum, checkSumType, pPath );
      }


    private:
      std::string pPath;
      int         pFD;
      int64_t     pSize;
      uint64_t    pCurrentOffset;
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
      StdInSource( const std::string &ckSumType ):
        pCkSumHelper( "stdin", ckSumType ), pCurrentOffset(0) {}

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        return pCkSumHelper.Initialize();
      }

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual int64_t GetSize()
      {
        return -1;
      }

      //------------------------------------------------------------------------
      //! Get a data chunk from the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::ChunkInfo &ci )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        uint32_t toRead = 2*1024*1024;
        char *buffer = new char[toRead];

        int64_t  bytesRead = 0;
        uint32_t offset    = 0;
        while( toRead )
        {
          int64_t bRead = read( 0, buffer+offset, toRead );
          if( bRead == -1 )
          {
            log->Debug( UtilityMsg, "Unable to read from stdin: %s",
                        strerror( errno ) );
            delete [] buffer;
            return XRootDStatus( stError, errOSError, errno );
          }

          if( bRead == 0 )
          {
            delete [] buffer;
            return XRootDStatus( stOK, suDone );
          }

          bytesRead += bRead;
          offset    += bRead;
          toRead    -= bRead;
        }

        pCkSumHelper.Update( buffer, bytesRead );
        ci.offset = pCurrentOffset;
        ci.length = bytesRead;
        ci.buffer = buffer;
        pCurrentOffset += bytesRead;
        return XRootDStatus( stOK, suContinue );
      }

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        return pCkSumHelper.GetCheckSum( checkSum, checkSumType );
      }

    private:
      CheckSumHelper pCkSumHelper;
      uint64_t       pCurrentOffset;
  };

  //----------------------------------------------------------------------------
  //! XRootDSource
  //----------------------------------------------------------------------------
  class XRootDSource: public Source
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      XRootDSource( const XrdCl::URL *url,
                    uint32_t          chunkSize,
                    uint8_t           parallelChunks ):
        pUrl( url ), pFile( new XrdCl::File() ), pSize( -1 ),
        pCurrentOffset( 0 ), pChunkSize( chunkSize ),
        pParallel( parallelChunks )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~XRootDSource()
      {
        CleanUpChunks();
        pFile->Close();
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
                                pUrl->GetURL().c_str() );

        XRootDStatus st = pFile->Open( pUrl->GetURL(), OpenFlags::Read );
        if( !st.IsOK() )
          return st;

        StatInfo *statInfo;
        st = pFile->Stat( false, statInfo );
        if( !st.IsOK() )
          return st;

        pSize = statInfo->GetSize();
        delete statInfo;

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
      //! Get a data chunk from the source
      //!
      //! @param  buffer buffer for the data
      //! @param  ci     chunk information
      //! @return        status of the operation
      //!                suContinue - there are some chunks left
      //!                suDone     - no chunks left
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::ChunkInfo &ci )
      {
        //----------------------------------------------------------------------
        // Sanity check
        //----------------------------------------------------------------------
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        if( !pFile->IsOpen() )
          return XRootDStatus( stError, errUninitialized );

        //----------------------------------------------------------------------
        // Fill the queue
        //----------------------------------------------------------------------
        while( pChunks.size() < pParallel && pCurrentOffset < pSize )
        {
          char *buffer = new char[pChunkSize];
          ChunkHandler *ch = new ChunkHandler;
          ch->chunk.offset = pCurrentOffset;
          ch->chunk.length = pChunkSize;
          ch->chunk.buffer = buffer;
          ch->status = pFile->Read( pCurrentOffset, pChunkSize, buffer, ch );
          pChunks.push( ch );
          pCurrentOffset += pChunkSize;
          if( !ch->status.IsOK() )
          {
            ch->sem->Post();
            break;
          }
        }

        //----------------------------------------------------------------------
        // Pick up a chunk from the front and wait for status
        //----------------------------------------------------------------------
        if( pChunks.empty() )
          return XRootDStatus( stOK, suDone );

        std::auto_ptr<ChunkHandler> ch( pChunks.front() );
        pChunks.pop();
        ch->sem->Wait();

        if( !ch->status.IsOK() )
        {
          log->Debug( UtilityMsg, "Unable read %d bytes at %ld from %s: %s",
                      ch->chunk.length, ch->chunk.offset,
                      pUrl->GetURL().c_str(), ch->status.ToStr().c_str() );
          delete [] (char *)ch->chunk.buffer;
          CleanUpChunks();
          return ch->status;
        }

        ci = ch->chunk;
        return XRootDStatus( stOK, suContinue );
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
          delete [] (char *)ch->chunk.buffer;
          delete ch;
        }
      }

      //------------------------------------------------------------------------
      // Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        return XrdCl::Utils::GetRemoteCheckSum( checkSum, checkSumType,
                                                pFile->GetDataServer(),
                                                pUrl->GetPath() );
      }

    private:
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
              XrdCl::ChunkInfo *resp = 0;
              response->Get( resp );
              if( resp )
                chunk = *resp;
              delete response;
            }
            sem->Post();
          }

        XrdSysSemaphore     *sem;
        XrdCl::ChunkInfo     chunk;
        XrdCl::XRootDStatus  status;
      };
      const XrdCl::URL           *pUrl;
      XrdCl::File                *pFile;
      int64_t                     pSize;
      int64_t                     pCurrentOffset;
      uint32_t                    pChunkSize;
      uint8_t                     pParallel;
      std::queue<ChunkHandler *>  pChunks;
  };

  //----------------------------------------------------------------------------
  //! Local destination
  //----------------------------------------------------------------------------
  class LocalDestination: public Destination
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      LocalDestination( const XrdCl::URL *url ):
        pPath( url->GetPath() ), pFD( -1 )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~LocalDestination()
      {
        if( pFD != -1 )
          close( pFD );
      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        //----------------------------------------------------------------------
        // Open the file for reading and get it's size
        //----------------------------------------------------------------------
        log->Debug( UtilityMsg, "Opening %s for writing", pPath.c_str() );

        int flags = O_WRONLY|O_CREAT|O_TRUNC;
        if( !pForce )
          flags |= O_EXCL;

        int fd = open( pPath.c_str(), flags, 0644 );
        if( fd == -1 )
        {
          log->Debug( UtilityMsg, "Unable to open %s: %s",
                                  pPath.c_str(), strerror( errno ) );
          return XRootDStatus( stError, errOSError, errno );
        }

        pFD   = fd;
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Put a data chunk at a destination
      //!
      //! @param  buffer buffer containing the chunk data
      //! @param  ci     chunk information
      //! @return status of the operation
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus PutChunk( const XrdCl::ChunkInfo &ci )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        if( pFD == -1 )
          return XRootDStatus( stError, errUninitialized );

        int64_t wr = pwrite( pFD, ci.buffer, ci.length, ci.offset );
        if( wr == -1 || wr != ci.length )
        {
          log->Debug( UtilityMsg, "Unable write to %s: %s",
                                  pPath.c_str(), strerror( errno ) );
          close( pFD );
          pFD = -1;
          if( pPosc )
            unlink( pPath.c_str() );
          return XRootDStatus( stError, errOSError, errno );
        }
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        return XrdCl::Utils::GetLocalCheckSum( checkSum, checkSumType, pPath );
      }

    private:
      std::string pPath;
      int         pFD;
  };

  //----------------------------------------------------------------------------
  //! Local destination
  //----------------------------------------------------------------------------
  class StdOutDestination: public Destination
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      StdOutDestination( const std::string &ckSumType ):
        pCkSumHelper( "stdout", ckSumType ), pCurrentOffset(0)
      {
      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        return pCkSumHelper.Initialize();
      }

      //------------------------------------------------------------------------
      //! Put a data chunk at a destination
      //!
      //! @param  ci     chunk information
      //! @return status of the operation
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus PutChunk( const XrdCl::ChunkInfo &ci )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        if( pCurrentOffset != ci.offset )
        {
          log->Error( UtilityMsg, "Got out-of-bounds chunk, expected offset:"
                      " %ld, got %ld", pCurrentOffset, ci.offset );
          return XRootDStatus( stError, errInternal );
        }

        int64_t wr = write( 1, ci.buffer, ci.length );
        if( wr == -1 || wr != ci.length )
        {
          log->Debug( UtilityMsg, "Unable write to stdout: %s",
                      strerror( errno ) );
          return XRootDStatus( stError, errOSError, errno );
        }
        pCurrentOffset += ci.length;

        pCkSumHelper.Update( ci.buffer, ci.length );
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        return pCkSumHelper.GetCheckSum( checkSum, checkSumType );
      }

    private:
      CheckSumHelper pCkSumHelper;
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
      XRootDDestination( const XrdCl::URL *url ):
        pUrl( url ), pFile( new XrdCl::File() )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~XRootDDestination()
      {
        delete pFile;
      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();
        log->Debug( UtilityMsg, "Opening %s for writing",
                                pUrl->GetURL().c_str() );

        OpenFlags::Flags flags = OpenFlags::Update;
        if( pForce )
          flags |= OpenFlags::Delete;
        else
          flags |= OpenFlags::New;

        if( pPosc )
          flags |= OpenFlags::POSC;

        if( pCoerce )
          flags |= OpenFlags::Force;

        return pFile->Open( pUrl->GetURL(), flags, Access::UR|Access::UW);
      }

      //------------------------------------------------------------------------
      //! Put a data chunk at a destination
      //!
      //! @param  ci     chunk information
      //! @return status of the operation
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus PutChunk( const XrdCl::ChunkInfo &ci )
      {
        using namespace XrdCl;
        if( !pFile->IsOpen() )
          return XRootDStatus( stError, errUninitialized );

        return pFile->Write( ci.offset, ci.length, ci.buffer );
      }

      //------------------------------------------------------------------------
      //! Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        return XrdCl::Utils::GetRemoteCheckSum( checkSum, checkSumType,
                                                pFile->GetDataServer(),
                                                pUrl->GetPath() );
      }

    private:
      const XrdCl::URL *pUrl;
      XrdCl::File      *pFile;
  };
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  ClassicCopyJob::ClassicCopyJob( JobDescriptor *jobDesc ):
    CopyJob( jobDesc )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Creating a classic copy job, from %s to %s",
                pJob->source.GetURL().c_str(),
                pJob->target.GetURL().c_str() );
  }

  //----------------------------------------------------------------------------
  // Run the copy job
  //----------------------------------------------------------------------------
  XRootDStatus ClassicCopyJob::Run( CopyProgressHandler *progress )
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Initialize the source and the destination
    //--------------------------------------------------------------------------
    std::auto_ptr<Source> src;
    if( pJob->source.GetProtocol() == "file" )
      src.reset( new LocalSource( &pJob->source ) );
    else if( pJob->source.GetProtocol() == "stdio" )
      src.reset( new StdInSource( pJob->checkSumType ) );
    else
      src.reset( new XRootDSource( &pJob->source,
                                   pJob->chunkSize,
                                   pJob->parallelChunks ) );

    XRootDStatus st = src->Initialize();
    if( !st.IsOK() ) return st;

    std::auto_ptr<Destination> dest;
    URL newDestUrl( pJob->target );

    if( pJob->target.GetProtocol() == "file" )
      dest.reset( new LocalDestination( &pJob->target ) );
    else if( pJob->target.GetProtocol() == "stdio" )
      dest.reset( new StdOutDestination( pJob->checkSumType ) );
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
      }
      dest.reset( new XRootDDestination( &newDestUrl ) );
    }

    dest->SetForce( pJob->force );
    dest->SetPOSC( pJob->posc );
    dest->SetCoerce( pJob->coerce );
    st = dest->Initialize();
    if( !st.IsOK() ) return st;

    //--------------------------------------------------------------------------
    // Copy the chunks
    //--------------------------------------------------------------------------
    ChunkInfo chunkInfo;
    uint64_t  size      = src->GetSize() >= 0 ? src->GetSize() : 0;
    uint64_t  processed = 0;
    while( 1 )
    {
      st = src->GetChunk( chunkInfo );
      if( !st.IsOK() )
        return st;

      if( st.IsOK() && st.code == suDone )
        break;

      st = dest->PutChunk( chunkInfo );

      delete [] (char*)chunkInfo.buffer;
      chunkInfo.buffer = 0;

      if( !st.IsOK() )
        return st;

      processed += chunkInfo.length;
      if( progress ) progress->JobProgress( processed, size );
    }

    //--------------------------------------------------------------------------
    // Verify the checksums if needed
    //--------------------------------------------------------------------------
    if( !pJob->checkSumType.empty() )
    {
      log->Debug( UtilityMsg, "Attempting checksum calculation." );

      //------------------------------------------------------------------------
      // Get the check sum at source
      //------------------------------------------------------------------------
      timeval oStart, oEnd;
      XRootDStatus st;
      gettimeofday( &oStart, 0 );
      if( !pJob->checkSumPreset.empty() )
      {
        pJob->sourceCheckSum  = pJob->checkSumType + ":";
        pJob->sourceCheckSum += pJob->checkSumPreset;
      }
      else
      {
        st = src->GetCheckSum( pJob->sourceCheckSum, pJob->checkSumType );
      }
      gettimeofday( &oEnd, 0 );

      //------------------------------------------------------------------------
      // Print the checksum if so requested and exit
      //------------------------------------------------------------------------
      if( pJob->checkSumPrint )
      {
        std::cerr << std::endl << "CheckSum: ";
        if( !pJob->sourceCheckSum.empty() )
          std::cerr << pJob->sourceCheckSum << std::endl;
        else
           std::cerr << st.ToStr() << std::endl;
        return XRootDStatus();
      }

      if( !st.IsOK() )
        return st;

      //------------------------------------------------------------------------
      // Get the check sum at destination
      //------------------------------------------------------------------------
      timeval tStart, tEnd;
      gettimeofday( &tStart, 0 );
      st = dest->GetCheckSum( pJob->targetCheckSum, pJob->checkSumType );
      if( !st.IsOK() )
        return st;
      gettimeofday( &tEnd, 0 );

      //------------------------------------------------------------------------
      // Compare and inform monitoring
      //------------------------------------------------------------------------
      bool match = false;
      if( pJob->sourceCheckSum == pJob->targetCheckSum )
        match = true;

      Monitor *mon = DefaultEnv::GetMonitor();
      if( mon )
      {
        Monitor::CheckSumInfo i;
        i.transfer.origin = &pJob->source;
        i.transfer.target = &pJob->target;
        i.cksum           = pJob->sourceCheckSum;
        i.oTime           = Utils::GetElapsedMicroSecs( oStart, oEnd );
        i.tTime           = Utils::GetElapsedMicroSecs( tStart, tEnd );
        i.isOK            = match;
        mon->Event( Monitor::EvCheckSum, &i );
      }

      if( !match )
        return XRootDStatus( stError, errCheckSumError, 0 );
    }

    return XRootDStatus();
  }
}
