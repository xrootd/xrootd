//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClClassicCopyJob.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClFile.hh"

#include <memory>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

namespace
{
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
      virtual uint64_t GetSize() = 0;

      //------------------------------------------------------------------------
      //! Get a data chunk from the source
      //!
      //! @param  buffer buffer for the data
      //! @param  ci     chunk information
      //! @return        status of the operation
      //!                suContinue - there are some chunks left
      //!                suDone     - no chunks left
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::Buffer    &buffer,
                                            XrdCl::ChunkInfo &ci ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Abstract chunk destination
  //----------------------------------------------------------------------------
  class Destination
  {
    public:
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
      //! @param  buffer buffer containing the chunk data
      //! @param  ci     chunk information
      //! @return status of the operation
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus PutChunk( const XrdCl::Buffer    &buffer,
                                            const XrdCl::ChunkInfo &ci ) = 0;
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
        pPath( url->GetPath() ), pFD( -1 ), pSize( 0 ), pCurrentOffset( 0 ) {}

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
        log->Debug( UtilityMsg, "Openning %s for reading", pPath.c_str() );

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
      virtual uint64_t GetSize()
      {
        return pSize;
      }

      //------------------------------------------------------------------------
      //! Get a data chunk from the source
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::Buffer    &buffer,
                                            XrdCl::ChunkInfo &ci )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        if( pFD == -1 )
          return XRootDStatus( stError, errUninitialized );

        const uint32_t toRead = 2*1024*1024;
        if( buffer.GetSize() != toRead )
          buffer.ReAllocate( toRead );

        int64_t bytesRead = read( pFD, buffer.GetBuffer(), toRead );
        if( bytesRead == -1 )
        {
          log->Debug( UtilityMsg, "Unable read from %s: %s",
                                  pPath.c_str(), strerror( errno ) );
          close( pFD );
          pFD = -1;
          return XRootDStatus( stError, errOSError, errno );
        }

        if( bytesRead == 0 )
          return XRootDStatus( stOK, suDone );

        ci.offset = pCurrentOffset;
        ci.length = bytesRead;
        pCurrentOffset += bytesRead;

        return XRootDStatus( stOK, suContinue );
      }

    private:
      std::string pPath;
      int         pFD;
      uint64_t    pSize;
      uint64_t    pCurrentOffset;
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
      XRootDSource( const XrdCl::URL *url ):
        pUrl( url ), pFile( new XrdCl::File() ), pSize( 0 ), pCurrentOffset( 0 )
      {
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

        XRootDStatus st = pFile->Open( pUrl->GetURL(), OpenFlags::Read, 0 );
        if( !st.IsOK() )
          return st;

        StatInfo *statInfo;
        st = pFile->Stat( false, statInfo );
        if( !st.IsOK() )
          return st;

        pSize = statInfo->GetSize();

        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get size
      //------------------------------------------------------------------------
      virtual uint64_t GetSize()
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
      virtual XrdCl::XRootDStatus GetChunk( XrdCl::Buffer    &buffer,
                                            XrdCl::ChunkInfo &ci )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        if( !pFile->IsOpen() )
          return XRootDStatus( stError, errUninitialized );

        if( pCurrentOffset == pSize )
          return XRootDStatus( stOK, suDone );

        const uint32_t toRead = 2*1024*1024;
        if( buffer.GetSize() != toRead )
          buffer.ReAllocate( toRead );

        uint32_t bytesRead;
        XRootDStatus st = pFile->Read( pCurrentOffset,     toRead,
                                       buffer.GetBuffer(), bytesRead );
        if( !st.IsOK() )
        {
          log->Debug( UtilityMsg, "Unable read from %s: %s",
                                  pUrl->GetURL().c_str(), st.ToStr().c_str() );
          pFile->Close();
          return st;
        }

        ci.offset = pCurrentOffset;
        ci.length = bytesRead;
        pCurrentOffset += bytesRead;

        return XRootDStatus( stOK, suContinue );
      }
    private:
      const XrdCl::URL *pUrl;
      XrdCl::File      *pFile;
      uint64_t          pSize;
      uint64_t          pCurrentOffset;
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
        log->Debug( UtilityMsg, "Openning %s for writing", pPath.c_str() );

        int fd = open( pPath.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0644 );
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
      virtual XrdCl::XRootDStatus PutChunk( const XrdCl::Buffer    &buffer,
                                            const XrdCl::ChunkInfo &ci )
      {
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();

        if( pFD == -1 )
          return XRootDStatus( stError, errUninitialized );

        int64_t wr = pwrite( pFD, buffer.GetBuffer(), ci.length, ci.offset );
        if( wr == -1 || wr != ci.length )
        {
          log->Debug( UtilityMsg, "Unable write to %s: %s",
                                  pPath.c_str(), strerror( errno ) );
          close( pFD );
          pFD = -1;
          return XRootDStatus( stError, errOSError, errno );
        }
        return XRootDStatus();
      }

    private:
      std::string pPath;
      int         pFD;
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

        return pFile->Open( pUrl->GetURL(),
                            OpenFlags::Update|OpenFlags::Delete,
                            Access::UR|Access::UW);
      }

      //------------------------------------------------------------------------
      //! Put a data chunk at a destination
      //!
      //! @param  buffer buffer containing the chunk data
      //! @param  ci     chunk information
      //! @return status of the operation
      //------------------------------------------------------------------------
      virtual XrdCl::XRootDStatus PutChunk( const XrdCl::Buffer    &buffer,
                                            const XrdCl::ChunkInfo &ci )
      {
        using namespace XrdCl;
        if( !pFile->IsOpen() )
          return XRootDStatus( stError, errUninitialized );

        return pFile->Write( ci.offset, ci.length, buffer.GetBuffer() );
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
  ClassicCopyJob::ClassicCopyJob( const URL *source, const URL *destination )
  {
    pSource      = source;
    pDestination = destination;

    Log *log = DefaultEnv::GetLog();
    log->Debug( UtilityMsg, "Creating a classic copy job, from %s to %s",
                            pSource->GetURL().c_str(),
                            pDestination->GetURL().c_str() );
  }

  //----------------------------------------------------------------------------
  // Run the copy job
  //----------------------------------------------------------------------------
  XRootDStatus ClassicCopyJob::Run( CopyProgressHandler *progress )
  {
    //--------------------------------------------------------------------------
    // Initialize the source and the destination
    //--------------------------------------------------------------------------
    std::auto_ptr<Source> src;
    if( pSource->GetProtocol() == "file" )
      src.reset( new LocalSource( pSource ) );
    else
      src.reset( new XRootDSource( pSource ) );

    XRootDStatus st = src->Initialize();
    if( !st.IsOK() ) return st;

    std::auto_ptr<Destination> dest;

    if( pDestination->GetProtocol() == "file" )
      dest.reset( new LocalDestination( pDestination ) );
    else
      dest.reset( new XRootDDestination( pDestination ) );

    st = dest->Initialize();
    if( !st.IsOK() ) return st;

    //--------------------------------------------------------------------------
    // Copy the chunks and exit
    //--------------------------------------------------------------------------
    Buffer    buff;
    ChunkInfo chunkInfo;
    uint64_t  size      = src->GetSize();
    uint64_t  processed = 0;
    while( 1 )
    {
      st = src->GetChunk( buff, chunkInfo );
      if( !st.IsOK() || (st.IsOK() && st.code == suDone) )
        return st;
      st = dest->PutChunk( buff, chunkInfo );
      if( !st.IsOK() )
        return st;
      processed += chunkInfo.length;
      if( progress ) progress->JobProgress( processed, size );
    }

    return XRootDStatus();
  }
}
