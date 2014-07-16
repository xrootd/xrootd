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
#include "XrdCl/XrdClUglyHacks.hh"

#include <memory>
#include <iostream>
#include <queue>
#include <algorithm>

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
        pPosc( false ), pForce( false ), pCoerce( false ), pMakeDir( false ) {}

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~Destination() {}

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

      //------------------------------------------------------------------------
      //! Set makedir
      //------------------------------------------------------------------------
      void SetMakeDir( bool makedir )
      {
        pMakeDir = makedir;
      }

    protected:
      bool pPosc;
      bool pForce;
      bool pCoerce;
      bool pMakeDir;
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
      LocalSource( const XrdCl::URL *url, const std::string &ckSumType ):
        pPath( url->GetPath() ), pFD( -1 ), pSize( -1 ), pCurrentOffset( 0 ),
        pCkSumHelper(0)
      {
        if( !ckSumType.empty() )
          pCkSumHelper = new CheckSumHelper( url->GetPath(), ckSumType );
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~LocalSource()
      {
        if( pFD != -1 )
          close( pFD );
        delete pCkSumHelper;
      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        if( pCkSumHelper )
        {
          XRootDStatus st = pCkSumHelper->Initialize();
          if( !st.IsOK() )
            return st;
        }

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

        if( pCkSumHelper )
          pCkSumHelper->Update( buffer, bytesRead );

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
        using namespace XrdCl;
        if( pCkSumHelper )
          return pCkSumHelper->GetCheckSum( checkSum, checkSumType );
        return XRootDStatus( stError, errCheckSumError );
      }

    private:
      std::string     pPath;
      int             pFD;
      int64_t         pSize;
      uint64_t        pCurrentOffset;
      CheckSumHelper *pCkSumHelper;
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
        pCkSumHelper(0), pCurrentOffset(0)
      {
        if( !ckSumType.empty() )
          pCkSumHelper = new CheckSumHelper( "stdin", ckSumType );
      }

      //------------------------------------------------------------------------
      //! Initialize the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        if( pCkSumHelper )
          return pCkSumHelper->Initialize();
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
        using namespace XrdCl;
        if( pCkSumHelper )
          return pCkSumHelper->GetCheckSum( checkSum, checkSumType );
        return XRootDStatus( stError, errCheckSumError );
      }

    private:
      CheckSumHelper *pCkSumHelper;
      uint64_t        pCurrentOffset;
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
          uint64_t chunkSize = pChunkSize;
          if( pCurrentOffset + chunkSize > (uint64_t)pSize )
            chunkSize = pSize - pCurrentOffset;

          char *buffer = new char[chunkSize];
          ChunkHandler *ch = new ChunkHandler;
          ch->chunk.offset = pCurrentOffset;
          ch->chunk.length = chunkSize;
          ch->chunk.buffer = buffer;
          ch->status = pFile->Read( pCurrentOffset, chunkSize, buffer, ch );
          pChunks.push( ch );
          pCurrentOffset += chunkSize;
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

        XRDCL_SMART_PTR_T<ChunkHandler> ch( pChunks.front() );
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
        std::string dataServer; pFile->GetProperty( "DataServer", dataServer );
        return XrdCl::Utils::GetRemoteCheckSum( checkSum, checkSumType,
                                                dataServer, pUrl->GetPath() );
      }

    private:
      //------------------------------------------------------------------------
      // Asynchronous chunk handler
      //------------------------------------------------------------------------
      class ChunkHandler: public XrdCl::ResponseHandler
      {
        public:
          ChunkHandler(): sem( new XrdCl::Semaphore(0) ) {}
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

        XrdCl::Semaphore    *sem;
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
  //! XRootDSourceDynamic
  //----------------------------------------------------------------------------
  class XRootDSourceDynamic: public Source
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      XRootDSourceDynamic( const XrdCl::URL *url,
                           uint32_t          chunkSize ):
        pUrl( url ), pFile( new XrdCl::File() ), pCurrentOffset( 0 ),
        pChunkSize( chunkSize ), pDone( false )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~XRootDSourceDynamic()
      {
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

        if( !pFile->IsOpen() )
          return XRootDStatus( stError, errUninitialized );

        if( pDone )
          return XRootDStatus( stOK, suDone );

        //----------------------------------------------------------------------
        // Fill the queue
        //----------------------------------------------------------------------
        char     *buffer = new char[pChunkSize];
        uint32_t  bytesRead = 0;

        XRootDStatus st = pFile->Read( pCurrentOffset, pChunkSize, buffer,
                                       bytesRead );

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

        ci.offset = pCurrentOffset;
        ci.length = bytesRead;
        ci.buffer = buffer;

        pCurrentOffset += bytesRead;

        return XRootDStatus( stOK, suContinue );
      }

      //------------------------------------------------------------------------
      // Get check sum
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetCheckSum( std::string &checkSum,
                                               std::string &checkSumType )
      {
        std::string dataServer; pFile->GetProperty( "DataServer", dataServer );
        return XrdCl::Utils::GetRemoteCheckSum( checkSum, checkSumType,
                                                dataServer, pUrl->GetPath() );
      }

    private:
      const XrdCl::URL           *pUrl;
      XrdCl::File                *pFile;
      int64_t                     pCurrentOffset;
      uint32_t                    pChunkSize;
      bool                        pDone;
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
          Finalize();
      }

      //------------------------------------------------------------------------
      //! Initialize the destination
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        //----------------------------------------------------------------------
        // Make the directory path if necessary
        //----------------------------------------------------------------------
        if( pMakeDir )
        {
          std::string dirpath = pPath.substr(0, pPath.find_last_of("/"));
          XRootDStatus st = MkPath( dirpath );
          if( !st.IsOK() )
            return st;
        }

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
      //! Finalize the destination
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Finalize()
      {
        using namespace XrdCl;
        if( pFD != -1 )
        {
          int fd = pFD; pFD = -1;
          if( close( fd ) != 0 )
            return XRootDStatus( stError, errOSError, errno );
        }
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

        int64_t   wr     = 0;
        uint64_t  offset = ci.offset;
        uint32_t  length = ci.length;
        char     *cursor = (char*)ci.buffer;
        do
        {
          wr = pwrite( pFD, cursor, length, offset );
          if( wr == -1 )
          {
            log->Debug( UtilityMsg, "Unable to write to %s: %s", pPath.c_str(),
                        strerror( errno ) );
            close( pFD );
            pFD = -1;
            if( pPosc )
              unlink( pPath.c_str() );
            return XRootDStatus( stError, errOSError, errno );
          }
          offset += wr;
          cursor += wr;
          length -= wr;
        }
        while( length );
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

      //------------------------------------------------------------------------
      //! Create a directory path
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus MkPath( std::string &path )
      {
        using namespace XrdCl;
        Log   *log = DefaultEnv::GetLog();
        struct stat st;
        std::vector<std::string> pathElements;
        std::vector<std::string>::iterator it;
        std::string fullPath;

        if( path.empty() )
          return XRootDStatus();

        log->Dump( UtilityMsg, "Attempting to create %s", path.c_str() );

        if( path[0] == '/' )
          fullPath = "/";

        XrdCl::Utils::splitString( pathElements, path, "/" );

        for( it = pathElements.begin(); it != pathElements.end(); ++it )
        {
          fullPath += *it;
          fullPath += "/";
          if( stat( fullPath.c_str(), &st ) != 0 )
          {
            if( errno == ENOENT )
            {
              if( mkdir( fullPath.c_str(), 0755 ) != 0 )
              {
                log->Error( UtilityMsg, "Cannot create directory %s: %s",
                            fullPath.c_str(), strerror( errno ) );
                return XRootDStatus( stError, errOSError, errno );
              }
            }
            else
            {
              log->Error( UtilityMsg, "Unable to stat %s: %s",
                          fullPath.c_str(), strerror( errno ) );
              return XRootDStatus( stError, errOSError, errno );
            }
          }
          else if( !S_ISDIR( st.st_mode ) )
          {
            log->Error( UtilityMsg, "Path %s not a directory: %s",
                        fullPath.c_str(), strerror( ENOTDIR ) );
            return XRootDStatus( stError, errOSError, ENOTDIR );
          }
          else
            log->Dump( UtilityMsg, "Path %s already exists", fullPath.c_str() );
        }
        return XRootDStatus();
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
      //! Initialize the destination
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus Initialize()
      {
        return pCkSumHelper.Initialize();
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

        int64_t   wr     = 0;
        uint32_t  length = ci.length;
        char     *cursor = (char*)ci.buffer;
        do
        {
          wr = write( 1, cursor, length );
          if( wr == -1 )
          {
            log->Debug( UtilityMsg, "Unable to write to stdout: %s",
                        strerror( errno ) );
            return XRootDStatus( stError, errOSError, errno );
          }
          pCurrentOffset += wr;
          cursor         += wr;
          length         -= wr;
        }
        while( length );

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
      //! Initialize the destination
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

        Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;

        return pFile->Open( pUrl->GetURL(), flags, mode );
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
        std::string dataServer; pFile->GetProperty( "DataServer", dataServer );
        return XrdCl::Utils::GetRemoteCheckSum( checkSum, checkSumType,
                                                dataServer, pUrl->GetPath() );
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
  ClassicCopyJob::ClassicCopyJob( uint16_t      jobId,
                                  PropertyList *jobProperties,
                                  PropertyList *jobResults ):
    CopyJob( jobId, jobProperties, jobResults )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Creating a classic copy job, from %s to %s",
                GetSource().GetURL().c_str(), GetTarget().GetURL().c_str() );
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
    uint16_t    parallelChunks;
    uint32_t    chunkSize;
    bool        posc, force, coerce, makeDir, dynamicSource;

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

    //--------------------------------------------------------------------------
    // Initialize the source and the destination
    //--------------------------------------------------------------------------
    XRDCL_SMART_PTR_T<Source> src;
    if( GetSource().GetProtocol() == "file" )
      src.reset( new LocalSource( &GetSource(), checkSumType ) );
    else if( GetSource().GetProtocol() == "stdio" )
      src.reset( new StdInSource( checkSumType ) );
    else
    {
      if( dynamicSource )
        src.reset( new XRootDSourceDynamic( &GetSource(), chunkSize ) );
      else
        src.reset( new XRootDSource( &GetSource(), chunkSize, parallelChunks ) );
    }

    XRootDStatus st = src->Initialize();
    if( !st.IsOK() ) return st;

    XRDCL_SMART_PTR_T<Destination> dest;
    URL newDestUrl( GetTarget() );

    if( GetTarget().GetProtocol() == "file" )
      dest.reset( new LocalDestination( &GetTarget() ) );
    else if( GetTarget().GetProtocol() == "stdio" )
      dest.reset( new StdOutDestination( checkSumType ) );
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

    dest->SetForce( force );
    dest->SetPOSC(  posc );
    dest->SetCoerce( coerce );
    dest->SetMakeDir( makeDir );
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
      if( progress ) progress->JobProgress( pJobId, processed, size );
    }

    //--------------------------------------------------------------------------
    // The size of the source is known and not enough data has been transfered
    // to the destination
    //--------------------------------------------------------------------------
    if( src->GetSize() >= 0 && size != processed )
    {
      log->Error( UtilityMsg, "The declared source size is %ld bytes, but "
                  "received %ld bytes.", size, processed );
      return XRootDStatus( stError, errDataError );
    }
    pResults->Set( "size", processed );

    //--------------------------------------------------------------------------
    // Finalize the destination
    //--------------------------------------------------------------------------
    st = dest->Finalize();
    if( !st.IsOK() )
      return st;

    //--------------------------------------------------------------------------
    // Verify the checksums if needed
    //--------------------------------------------------------------------------
    if( checkSumMode != "none" )
    {
      log->Debug( UtilityMsg, "Attempting checksum calculation, mode: %s.",
                  checkSumMode.c_str() );
      std::string sourceCheckSum;
      std::string targetCheckSum;

      //------------------------------------------------------------------------
      // Get the check sum at source
      //------------------------------------------------------------------------
      timeval oStart, oEnd;
      XRootDStatus st;

      if( checkSumMode == "end2end" || checkSumMode == "source" )
      {
        gettimeofday( &oStart, 0 );
        if( !checkSumPreset.empty() )
        {
          sourceCheckSum  = checkSumType + ":";
          sourceCheckSum += checkSumPreset;
        }
        else
        {
          st = src->GetCheckSum( sourceCheckSum, checkSumType );
        }
        gettimeofday( &oEnd, 0 );

        if( !st.IsOK() )
          return st;

        pResults->Set( "sourceCheckSum", sourceCheckSum );
      }

      //------------------------------------------------------------------------
      // Get the check sum at destination
      //------------------------------------------------------------------------
      timeval tStart, tEnd;

      if( checkSumMode == "end2end" || checkSumMode == "target" )
      {
        gettimeofday( &tStart, 0 );
        st = dest->GetCheckSum( targetCheckSum, checkSumType );
        if( !st.IsOK() )
          return st;
        gettimeofday( &tEnd, 0 );
        pResults->Set( "targetCheckSum", targetCheckSum );
      }

      //------------------------------------------------------------------------
      // Compare and inform monitoring
      //------------------------------------------------------------------------
      if( checkSumMode == "end2end" )
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
          return XRootDStatus( stError, errCheckSumError, 0 );
      }
    }
    return XRootDStatus();
  }
}
