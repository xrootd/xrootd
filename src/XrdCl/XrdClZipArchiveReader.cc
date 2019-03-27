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

#include "XrdCl/XrdClZipArchiveReader.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include "XrdSys/XrdSysPthread.hh"

#include <string>
#include <map>
#include <memory>

namespace XrdCl
{

template<typename RESP>
struct ZipHandlerException
{
    ZipHandlerException( XRootDStatus *status, RESP *response ) : status( status ), response( response ) { }

    XRootDStatus *status;
    RESP         *response;
};


struct EOCD
{
    EOCD( const char *buffer )
    {
      pNbDisk   = *reinterpret_cast<const uint16_t*>( buffer + 4 );
      pDisk     = *reinterpret_cast<const uint16_t*>( buffer + 6 );
      pNbCdRecD = *reinterpret_cast<const uint16_t*>( buffer + 8 );
      pNbCdRec  = *reinterpret_cast<const uint16_t*>( buffer + 10 );
      pCdSize   = *reinterpret_cast<const uint32_t*>( buffer + 12 );
      pCdOffset = *reinterpret_cast<const uint32_t*>( buffer + 16 );
      pCommSize = *reinterpret_cast<const uint16_t*>( buffer + 20 );
      pComment  = std::string( buffer + 22, pCommSize );
    }

    uint16_t    pNbDisk;
    uint16_t    pDisk;
    uint16_t    pNbCdRecD;
    uint16_t    pNbCdRec;
    uint32_t    pCdSize;
    uint32_t    pCdOffset;
    uint16_t    pCommSize;
    std::string pComment;

    static const uint16_t kEocdBaseSize   = 22;
    static const uint32_t kEocdSign       = 0x06054b50;
    static const uint16_t kMaxCommentSize = 65535;
};


struct ZIP64_EOCDL
{
    ZIP64_EOCDL( const char *buffer )
    {
      pZip64EocdOffset = *reinterpret_cast<const uint64_t*>( buffer + 8 );
    }

    uint64_t pZip64EocdOffset;
    static const uint16_t kZip64EocdlSize = 20;
    static const uint32_t kZip64EocdlSign = 0x07064b50;
};

struct ZIP64_EOCD
{
    ZIP64_EOCD( const char* buffer )
    {
      pZipVersion    = *reinterpret_cast<const uint16_t*>( buffer + 12 );
      pMinZipVersion = *reinterpret_cast<const uint16_t*>( buffer + 14 );
      pNbCdEntries   = *reinterpret_cast<const uint16_t*>( buffer + 32 );
      pCdSize        = *reinterpret_cast<const uint64_t*>( buffer + 40 );
      pCdOffset      = *reinterpret_cast<const uint64_t*>( buffer + 48 );
    }

    uint16_t    pZipVersion;
    uint16_t    pMinZipVersion;
    uint64_t    pNbCdEntries;
    uint64_t    pCdSize;
    uint64_t    pCdOffset;

    static const uint16_t kZip64EocdBaseSize = 56;
    static const uint32_t kZip64EocdSign = 0x06064b50;
};

struct CDFH
{
    CDFH( const char *buffer )
    {
      pZipVersion        = *reinterpret_cast<const uint16_t*>( buffer + 4 );
      pMinZipVersion     = *reinterpret_cast<const uint16_t*>( buffer + 6 );
      pCompressionMethod = *reinterpret_cast<const uint16_t*>( buffer + 10 );
      pCrc32             = *reinterpret_cast<const uint32_t*>( buffer + 16 );
      pCompressedSize    = *reinterpret_cast<const uint32_t*>( buffer + 20 );
      pUncompressedSize  = *reinterpret_cast<const uint32_t*>( buffer + 24 );
      pDiskNb            = *reinterpret_cast<const uint16_t*>( buffer + 34 );
      pOffset            = *reinterpret_cast<const uint32_t*>( buffer + 42 );

      uint16_t filenameLength = *reinterpret_cast<const uint16_t*>( buffer + 28 );
      uint16_t extraLength    = *reinterpret_cast<const uint16_t*>( buffer + 30 );
      uint16_t commentLength  = *reinterpret_cast<const uint16_t*>( buffer + 32 );

      pFilename = std::string( buffer + 46, filenameLength );

      // now parse the 'extra' (may contain the zip64 extension to CDFH)
      ParseExtra( buffer + 46 + filenameLength, extraLength );

      pCdfhSize = kCdfhBaseSize + filenameLength +extraLength + commentLength;
    }

    void ParseExtra( const char *buffer, uint16_t length)
    {
      static const uint32_t ovrflw32 = 0xffffffff;
      static const uint16_t ovrflw16 = 0xffff;
      static const uint16_t ZIP64_EXTENSION_SIGN = 0x0001;

      uint16_t exsize = 0;
      uint64_t *ovrflw[3] = {0, 0, 0};
      uint8_t count = 0;

      // check if compressed size is overflown
      if( pCompressedSize == ovrflw32)
      {
        ovrflw[count] = &pCompressedSize;
        ++count;
        exsize += sizeof( uint64_t );
      }

      // check if original size is overflown
      if( pUncompressedSize == ovrflw32 )
      {
        ovrflw[count] = &pUncompressedSize;
        ++count;
        exsize += sizeof( uint64_t );
      }

      // check if offset is overflown
      if( pOffset == ovrflw32 )
      {
        ovrflw[count] = &pOffset;
        ++count;
        exsize += sizeof( uint64_t );
      }

      // check if number of disks is overflown
      if( pDiskNb == ovrflw16 )
        exsize += sizeof( uint32_t );

      // if the expected size of ZIP64 extension is 0 we
      // can skip parsing of 'extra'
      if( exsize == 0 ) return;

      // Parse the extra part
      const char *end = buffer + length;
      uint16_t signature = 0;
      uint16_t blksize   = 0;
      while( buffer < end )
      {
        signature = *reinterpret_cast<const uint16_t*>( buffer );
        buffer += sizeof( uint16_t );
        blksize   = *reinterpret_cast<const uint16_t*>( buffer );
        buffer += sizeof( uint16_t );

        // is it ZIP64 extension
        if( signature == ZIP64_EXTENSION_SIGN )
        {
          if( blksize != exsize )
            throw ZipHandlerException<AnyObject>( new XRootDStatus( stError, errDataError, 0, "Wrong size of ZIP64 extension!" ), 0 );

          for( uint8_t j = 0; j < count; ++j )
          {
            uint64_t value = *reinterpret_cast<const uint64_t*>( buffer );
            buffer += sizeof( uint64_t );
            exsize -= sizeof( uint64_t );
            *ovrflw[j] = value;
          }

          if( exsize == sizeof( uint32_t ) )
          {
            pDiskNb = *reinterpret_cast<const uint32_t*>( buffer );
            buffer += sizeof( uint32_t );
          }

          break;
        }
        buffer += blksize;
      }
    }

    uint16_t    pZipVersion;
    uint16_t    pMinZipVersion;
    uint16_t    pCompressionMethod;
    uint32_t    pCrc32;
    uint64_t    pCompressedSize;
    uint64_t    pUncompressedSize;
    uint32_t    pDiskNb;
    uint64_t    pOffset;
    std::string pFilename;
    uint16_t    pCdfhSize;

    static const uint16_t kCdfhBaseSize = 46;
    static const uint32_t kCdfhSign     = 0x02014b50;
};


class ZipArchiveReaderImpl
{
  public:

    ZipArchiveReaderImpl( File &archive ) : pArchive( archive ), pArchiveSize( 0 ), pRefCount( 1 ), pOpen( false ) { }

    ZipArchiveReaderImpl* Self()
    {
      XrdSysMutexHelper scopedLock( pMutex );
      ++pRefCount;
      return this;
    }

    void Delete()
    {
      XrdSysMutexHelper scopedLock( pMutex );
      --pRefCount;
      if( !pRefCount )
      {
        scopedLock.UnLock();
        delete this;
      }
    }

    XRootDStatus Open( const std::string &url, ResponseHandler *userHandler, uint16_t timeout  = 0 );

    XRootDStatus StatArchive( ResponseHandler *userHandler );

    XRootDStatus ReadArchive( ResponseHandler *userHandler );

    XRootDStatus ReadEocd( ResponseHandler *userHandler );

    XRootDStatus ReadCdfh( uint64_t bytesRead, ResponseHandler *userHandler );

    XRootDStatus Read( const std::string &filename, uint64_t relativeOffset, uint32_t size, void *buffer, ResponseHandler *userHandler, uint16_t timeout = 0 );

    XRootDStatus Read( uint64_t relativeOffset, uint32_t size, void *buffer, ResponseHandler *userHandler, uint16_t timeout = 0 )
    {
      if( pBoundFile.empty() )
        return XRootDStatus( stError, errInvalidOp );

      return Read( pBoundFile, relativeOffset, size, buffer, userHandler, timeout );
    }

    DirectoryList* List();

    XRootDStatus Close( ResponseHandler *handler, uint16_t timeout )
    {
      XRootDStatus st = pArchive.Close( handler, timeout );
      if( st.IsOK() )
      {
        pBuffer.reset();
        ClearRecords();
      }
      return st;
    }

    XRootDStatus GetSize( const std::string & filename, uint64_t &size ) const
    {
      std::map<std::string, size_t>::const_iterator it = pFileToCdfh.find( filename );
      if( it == pFileToCdfh.end() ) return XRootDStatus( stError, errNotFound );
      CDFH *cdfh = pCdRecords[it->second];
      size = cdfh->pCompressionMethod ? cdfh->pCompressedSize : cdfh->pUncompressedSize;
      return XRootDStatus();
    }

    XRootDStatus Bind( const std::string &filename )
    {
      std::map<std::string, size_t>::const_iterator it = pFileToCdfh.find( filename );
      if( it == pFileToCdfh.end() ) return XRootDStatus( stError, errNotFound );
      pBoundFile = filename;
      return XRootDStatus();
    }

    bool IsOpen() const
    {
      return pOpen;
    }

    void SetArchiveSize( uint64_t size )
    {
      pArchiveSize = size;
    }

    char* LookForEocd( uint64_t size )
    {
      for( ssize_t offset = size - EOCD::kEocdBaseSize; offset >= 0; --offset )
      {
        uint32_t *signature = reinterpret_cast<uint32_t*>( pBuffer.get() + offset );
        if( *signature == EOCD::kEocdSign ) return pBuffer.get() + offset;
      }
      return 0;
    }

    XRootDStatus ParseCdRecords( char *buffer, uint16_t nbCdRecords, uint32_t bufferSize )
    {
      uint32_t offset = 0;
      pCdRecords.reserve( nbCdRecords );

      for( size_t i = 0; i < nbCdRecords; ++i )
      {
        if( bufferSize < CDFH::kCdfhBaseSize ) break;
        // check the signature
        uint32_t *signature = (uint32_t*)( buffer + offset );
        if( *signature != CDFH::kCdfhSign ) return XRootDStatus( stError, errErrorResponse, errDataError, "Central-directory-file-header signature not found." );
        // parse the record
        CDFH *cdfh = new CDFH( buffer + offset );
        offset     += cdfh->pCdfhSize;
        bufferSize -= cdfh->pCdfhSize;
        pCdRecords.push_back( cdfh );
        pFileToCdfh[cdfh->pFilename] = i;
      }

      pOpen = true;
      return XRootDStatus();
    }

    XRootDStatus HandleWholeArchive()
    {
      // create the End-of-Central-Directory record
      char *eocdBlock = LookForEocd( pArchiveSize );
      if( !eocdBlock ) return XRootDStatus( stError, errErrorResponse, errDataError, "End-of-central-directory signature not found." );
      pEocd.reset( new EOCD( eocdBlock ) );

      // If we managed to download the whole archive we don't need to
      // worry about zip64, it is so small that standard EOCD will do

      // parse Central-Directory-File-Header records
      XRootDStatus st = ParseCdRecords( pBuffer.get() + pEocd->pCdOffset, pEocd->pNbCdRec, pEocd->pCdSize );

      return st;
    }

    XRootDStatus HandleCdfh( uint16_t nbCdRecords, uint32_t bufferSize )
    {
      // parse Central-Directory-File-Header records
      XRootDStatus st = ParseCdRecords( pBuffer.get(), nbCdRecords, bufferSize );
      // successful or not we don't need it anymore
      pBuffer.reset();
      return st;
    }

  private:

    void ClearRecords()
    {
      pEocd.reset();
      pZip64Eocd.reset();

      for( std::vector<CDFH*>::iterator it = pCdRecords.begin(); it != pCdRecords.end(); ++it )
        delete *it;
      pCdRecords.clear();
      pFileToCdfh.clear();

      pBoundFile.erase();
    }

    ~ZipArchiveReaderImpl()
    {
      ClearRecords();
      if( pArchive.IsOpen() )
      {
        XRootDStatus st = pArchive.Close();
        if( !st.IsOK() )
        {
          Log *log = DefaultEnv::GetLog();
          log->Warning( FileMsg, "ZipArchiveReader failed to close file upon destruction: %s.", st.ToString().c_str() );
        }
      }
    }

    File                          &pArchive;
    uint64_t                       pArchiveSize;
    std::unique_ptr<char[]>        pBuffer;
    std::unique_ptr<EOCD>          pEocd;
    std::unique_ptr<ZIP64_EOCD>    pZip64Eocd;
    std::vector<CDFH*>             pCdRecords;
    std::map<std::string, size_t>  pFileToCdfh;
    mutable XrdSysMutex            pMutex;
    size_t                         pRefCount;
    bool                           pOpen;
    std::string                    pBoundFile;
};


class ZipHandlerCommon : public ResponseHandler
{
  public:

    ZipHandlerCommon( ZipArchiveReaderImpl *impl, ResponseHandler *userHandler ) : pImpl( impl->Self() ), pUserHandler( userHandler ) { }

    virtual ~ZipHandlerCommon()
    {
      pImpl->Delete();
    }

    template<typename RESP>
    void DeleteArgs( XRootDStatus *status, RESP *response )
    {
      delete status;
      delete response;
    }

    template<typename RESP>
    AnyObject* PkgResp( RESP *resp )
    {
      AnyObject *response = new AnyObject();
      response->Set( resp );
      return response;
    }

  protected:

    ZipArchiveReaderImpl *pImpl;
    ResponseHandler      *pUserHandler;
};


template<typename RESP>
class ZipHandlerBase : public ZipHandlerCommon
{
  public:

    ZipHandlerBase( ZipArchiveReaderImpl *impl, ResponseHandler *userHandler ) : ZipHandlerCommon( impl, userHandler ) { }

    virtual void HandleResponseImpl( XRootDStatus *status, RESP *response ) = 0;

    virtual void HandleResponse( XRootDStatus *status, AnyObject *response )
    {
      try
      {
        if( !status->IsOK() ) throw ZipHandlerException<AnyObject>( status, response );

        if( !response )
        {
          *status = XRootDStatus( stError, errInternal );
          throw ZipHandlerException<AnyObject>( status, response );
        }

        RESP *resp = 0;
        response->Get( resp );
        if( !resp )
        {
          *status = XRootDStatus( stError, errInternal );
          throw ZipHandlerException<AnyObject>( status, response );
        }
        response->Set( (int *)0 );
        delete response;

        HandleResponseImpl( status, resp );
      }
      catch( ZipHandlerException<AnyObject>& ex)
      {
        if( pUserHandler ) pUserHandler->HandleResponse( ex.status, ex.response );
        else DeleteArgs( ex.status, ex.response );
      }
      catch( ZipHandlerException<RESP>& ex )
      {
        if( pUserHandler ) pUserHandler->HandleResponse( ex.status, ex.response ? PkgResp( ex.response ) : 0 );
        else DeleteArgs( ex.status, ex.response );
      }

      delete this;
    }
};


template<>
class ZipHandlerBase<void> : public ZipHandlerCommon
{
  public:

    ZipHandlerBase( ZipArchiveReaderImpl *impl, ResponseHandler *userHandler ) : ZipHandlerCommon( impl, userHandler ) { }

    virtual void HandleResponseImpl( XRootDStatus *status, AnyObject *response ) = 0;

    virtual void HandleResponse( XRootDStatus *status, AnyObject *response )
    {
      try
      {
        if( !status->IsOK() ) throw ZipHandlerException<AnyObject>( status, response );
        HandleResponseImpl( status, response );
      }
      catch( ZipHandlerException<AnyObject>& ex)
      {
        if( pUserHandler ) pUserHandler->HandleResponse( ex.status, ex.response );
        else DeleteArgs( ex.status, ex.response );
      }

      delete this;
    }
};


class ZipOpenHandler : public ZipHandlerBase<void>
{
  public:

    ZipOpenHandler( ZipArchiveReaderImpl *impl, ResponseHandler *userHandler ): ZipHandlerBase<void>( impl, userHandler ) { }

    virtual void HandleResponseImpl( XRootDStatus *status, AnyObject *response )
    {
      XRootDStatus st = pImpl->StatArchive( pUserHandler );
      if( !st.IsOK() )
      {
        *status = st;
        throw ZipHandlerException<AnyObject>( status, response );
      }

      DeleteArgs( status, response );
    }
};


class StatArchiveHandler : public ZipHandlerBase<StatInfo>
{
  public:

    StatArchiveHandler( ZipArchiveReaderImpl *impl, ResponseHandler *userHandler ): ZipHandlerBase<StatInfo>( impl, userHandler ) { }

    virtual void HandleResponseImpl( XRootDStatus *status, StatInfo *response )
    {
      uint64_t size = response->GetSize();
      pImpl->SetArchiveSize( size );

      // if the size of the file is smaller than the maximum comment size +
      // EOCD size simply download the whole file, otherwise download the EOCD
      XRootDStatus st = ( size <= EOCD::kMaxCommentSize + EOCD::kEocdBaseSize + ZIP64_EOCDL::kZip64EocdlSize ) ?
                        pImpl->ReadArchive( pUserHandler ) :
                        pImpl->ReadEocd( pUserHandler );
      if( !st.IsOK() )
      {
        *status = st;
        throw ZipHandlerException<StatInfo>( status, response );
      }

      DeleteArgs( status, response );
    }
};


class ReadArchiveHandler : public ZipHandlerBase<ChunkInfo>
{
  public:

    ReadArchiveHandler( ZipArchiveReaderImpl *impl, ResponseHandler *userHandler ) :  ZipHandlerBase<ChunkInfo>( impl, userHandler ) { }

    virtual void HandleResponseImpl( XRootDStatus *status, ChunkInfo *response )
    {
      pImpl->SetArchiveSize( response->length );
      // we have the whole archive locally in the buffer
      XRootDStatus st = pImpl->HandleWholeArchive();
      if( pUserHandler )
      {
        // in fact this is the result of open, so in this case
        // the user does not care about the ChunkInfo response
        delete response;
        *status = st;
        pUserHandler->HandleResponse( status, 0 );
      }
      else
        DeleteArgs( status, response );
    }
};


class ReadCdfhHandler : public ZipHandlerBase<ChunkInfo>
{
  public:

    ReadCdfhHandler( ZipArchiveReaderImpl *impl, ResponseHandler *userHandler, uint16_t nbCdRec ) : ZipHandlerBase<ChunkInfo>( impl, userHandler ), pNbCdRec( nbCdRec ) { }

    virtual void HandleResponseImpl( XRootDStatus *status, ChunkInfo *response )
    {
      XRootDStatus st = pImpl->HandleCdfh( pNbCdRec, response->length );
      if( pUserHandler )
      {
        *status = st;
        // in fact this is the result of open, so in this case
        // the user does not care about the ChunkInfo response
        delete response;
        pUserHandler->HandleResponse( status, 0 );
      }
      else
        DeleteArgs( status, response );
    }

  private:

    uint16_t pNbCdRec;
};


class ReadEocdHandler : public ZipHandlerBase<ChunkInfo>
{
  public:

    ReadEocdHandler( ZipArchiveReaderImpl *impl, ResponseHandler *userHandler ) : ZipHandlerBase<ChunkInfo>( impl, userHandler ) { }

    virtual void HandleResponseImpl( XRootDStatus *status, ChunkInfo *response )
    {
      XRootDStatus st = pImpl->ReadCdfh( response->length, pUserHandler );
      if( !st.IsOK() )
      {
        *status = st;
        throw ZipHandlerException<ChunkInfo>( status, response );
      }
      else
        DeleteArgs( status, response );
    }

};


class ZipReadHandler : public ZipHandlerBase<ChunkInfo>
{
  public:

    ZipReadHandler( uint64_t relativeOffset, ZipArchiveReaderImpl *impl, ResponseHandler *userHandler ) : ZipHandlerBase<ChunkInfo>( impl, userHandler ), pRelativeOffset( relativeOffset ) { }

    virtual void HandleResponseImpl( XRootDStatus *status, ChunkInfo *response )
    {
      response->offset = pRelativeOffset;
      if( pUserHandler ) pUserHandler->HandleResponse( status, PkgResp( response ) );
      else
        DeleteArgs( status, response );
    }

  private:

    uint64_t pRelativeOffset;
};


ZipArchiveReader::ZipArchiveReader( File &archive ) : pImpl( new ZipArchiveReaderImpl( archive ) )
{

}


ZipArchiveReader::~ZipArchiveReader()
{
  pImpl->Delete();
}


XRootDStatus ZipArchiveReaderImpl::Open( const std::string &url, ResponseHandler *userHandler, uint16_t timeout )
{
  ZipOpenHandler *handler = new ZipOpenHandler( this, userHandler );
  XRootDStatus st = pArchive.Open( url, OpenFlags::Read, Access::None, handler, timeout );
  if( !st.IsOK() ) delete handler;
  return st;
}


XRootDStatus ZipArchiveReader::Open( const std::string &url, ResponseHandler *handler, uint16_t timeout )
{
  return pImpl->Open( url, handler, timeout );
}


XRootDStatus ZipArchiveReader::Open( const std::string &url, uint16_t timeout )
{
  SyncResponseHandler handler;
  Status st = Open( url, &handler, timeout );
  if( !st.IsOK() )
    return st;

  return MessageUtils::WaitForStatus( &handler );
}


XRootDStatus ZipArchiveReaderImpl::StatArchive( ResponseHandler *userHandler )
{
  // we are doing the stat only while
  // opening an archive so we clear
  // just to be on the safe side
  ClearRecords();

  // create a stat handler
  StatArchiveHandler *handler = new StatArchiveHandler( this, userHandler );

  // let's check if we got a stat together with the open response
  StatInfo *response = 0;
  XRootDStatus st = pArchive.Stat( false, response ); // always returns stOK
  if( st.IsOK() && response )
  {
    AnyObject *resp = new AnyObject();
    resp->Set( response );
    handler->HandleResponse( new XRootDStatus(), resp ); //this will deallocate the handler memory
    return XRootDStatus();
  }

  // if we don't have the stat yet, we need to issue a stat request
  st = pArchive.Stat( true, handler );
  if( !st.IsOK() ) delete handler;
  return st;
}

XRootDStatus ZipArchiveReaderImpl::ReadArchive( ResponseHandler *userHandler )
{
  uint64_t offset = 0;
  uint32_t size   = pArchiveSize;
  pBuffer.reset( new char[size] );
  ReadArchiveHandler *handler = new ReadArchiveHandler( this, userHandler );
  XRootDStatus st = pArchive.Read( offset, size, pBuffer.get(), handler );
  if( !st.IsOK() ) delete handler;
  return st;
}

XRootDStatus ZipArchiveReaderImpl::ReadEocd( ResponseHandler *userHandler )
{
  uint32_t size   = EOCD::kMaxCommentSize + EOCD::kEocdBaseSize + ZIP64_EOCDL::kZip64EocdlSize;
  uint64_t offset = pArchiveSize - size;
  pBuffer.reset( new char[size] );
  ReadEocdHandler *handler = new ReadEocdHandler( this, userHandler );
  XRootDStatus st = pArchive.Read( offset, size, pBuffer.get(), handler );
  if( !st.IsOK() ) delete handler;
  return st;
}

XRootDStatus ZipArchiveReaderImpl::ReadCdfh( uint64_t bytesRead, ResponseHandler *userHandler )
{
  char *eocdBlock = LookForEocd( bytesRead );
  if( !eocdBlock ) throw ZipHandlerException<AnyObject>( new XRootDStatus( stError, errDataError, errDataError, "End-of-central-directory signature not found." ), 0 );
  pEocd.reset( new EOCD( eocdBlock ) );

  // Let's see if it is ZIP64 (if yes, the EOCD will be preceded with ZIP64 EOCD locator)
  char *zip64EocdlBlock = eocdBlock - ZIP64_EOCDL::kZip64EocdlSize;
  // make sure there is enough data to assume there's a ZIP64 EOCD locator
  if( zip64EocdlBlock > pBuffer.get() )
  {
    uint32_t *signature = reinterpret_cast<uint32_t*>( zip64EocdlBlock );
    if( *signature == ZIP64_EOCDL::kZip64EocdlSign )
    {
      std::unique_ptr<ZIP64_EOCDL> eocdl( new ZIP64_EOCDL( zip64EocdlBlock ) );
      // the offset at which we did the read
      uint64_t buffOffset = pArchiveSize - bytesRead;
      if( buffOffset > eocdl->pZip64EocdOffset )
      {
        // we need to read more data
        uint32_t size = pArchiveSize - eocdl->pZip64EocdOffset;
        pBuffer.reset( new char[size] );
        ReadEocdHandler *handler = new ReadEocdHandler( this, userHandler );
        XRootDStatus st = pArchive.Read( eocdl->pZip64EocdOffset, size, pBuffer.get(), handler );
        if( !st.IsOK() ) delete handler;
        return st;
      }

      char *zip64EocdBlock = pBuffer.get() + ( eocdl->pZip64EocdOffset - buffOffset );
      signature = reinterpret_cast<uint32_t*>( zip64EocdBlock );
      if( *signature != ZIP64_EOCD::kZip64EocdSign )
        throw ZipHandlerException<AnyObject>( new XRootDStatus( stError, errDataError, errDataError, "ZIP64 End-of-central-directory signature not found." ), 0 );
      pZip64Eocd.reset( new ZIP64_EOCD( zip64EocdBlock ) );
    }
    /*
    else
      it is not ZIP64 so we have everything in EOCD
    */
  }

  uint64_t offset = pZip64Eocd ? pZip64Eocd->pCdOffset : pEocd->pCdOffset;
  uint32_t size   = pZip64Eocd ? pZip64Eocd->pCdSize   : pEocd->pCdSize;
  pBuffer.reset( new char[size] );
  ReadCdfhHandler *handler = new ReadCdfhHandler( this, userHandler, pEocd->pNbCdRec );
  XRootDStatus st = pArchive.Read( offset, size, pBuffer.get(), handler );
  if( !st.IsOK() ) delete handler;
  return st;
}

XRootDStatus ZipArchiveReader::Read( const std::string &filename, uint64_t offset, uint32_t size, void *buffer, ResponseHandler *handler, uint16_t timeout )
{
  return pImpl->Read( filename, offset, size, buffer, handler, timeout );
}

XRootDStatus ZipArchiveReader::Read( const std::string &filename, uint64_t offset, uint32_t size, void *buffer, uint32_t &bytesRead, uint16_t timeout )
{
  SyncResponseHandler handler;
  Status st = Read( filename, offset, size, buffer, &handler, timeout );
  if( !st.IsOK() )
    return st;

  ChunkInfo *chunkInfo = 0;
  XRootDStatus status = MessageUtils::WaitForResponse( &handler, chunkInfo );
  if( status.IsOK() )
  {
    bytesRead = chunkInfo->length;
    delete chunkInfo;
  }
  return status;
}

//------------------------------------------------------------------------
// Bounds the reader to a file inside the archive.
//------------------------------------------------------------------------
XRootDStatus ZipArchiveReader::Bind( const std::string &filename )
{
  return pImpl->Bind( filename );
}

//------------------------------------------------------------------------
// Async bound read.
//------------------------------------------------------------------------
XRootDStatus ZipArchiveReader::Read( uint64_t offset, uint32_t size, void *buffer, ResponseHandler *handler, uint16_t timeout )
{
  return pImpl->Read( offset, size, buffer, handler, timeout );
}

//------------------------------------------------------------------------
// Sync bound read.
//------------------------------------------------------------------------
XRootDStatus ZipArchiveReader::Read( uint64_t offset, uint32_t size, void *buffer, uint32_t &bytesRead, uint16_t timeout )
{
  SyncResponseHandler handler;
  Status st = Read( offset, size, buffer, &handler, timeout );
  if( !st.IsOK() )
    return st;

  ChunkInfo *chunkInfo = 0;
  XRootDStatus status = MessageUtils::WaitForResponse( &handler, chunkInfo );
  if( status.IsOK() )
  {
    bytesRead = chunkInfo->length;
    delete chunkInfo;
  }
  return status;
}

//------------------------------------------------------------------------
// Sync list
//------------------------------------------------------------------------
XRootDStatus ZipArchiveReader::List( DirectoryList *&list )
{
  if( !pImpl->IsOpen() )
    return XRootDStatus( stError, errInvalidOp );

  list = pImpl->List();
  return XRootDStatus();
}

XRootDStatus ZipArchiveReaderImpl::Read( const std::string &filename, uint64_t relativeOffset, uint32_t size, void *buffer, ResponseHandler *userHandler, uint16_t timeout )
{
  if( !pArchive.IsOpen() ) return XRootDStatus( stError, errInvalidOp, errInvalidOp, "Archive not opened." );

  std::map<std::string, size_t>::iterator cditr = pFileToCdfh.find( filename );
  if( cditr == pFileToCdfh.end() ) return XRootDStatus( stError, errNotFound, errNotFound, "File not found." );
  CDFH *cdfh = pCdRecords[cditr->second];

  // check if the file is compressed, for now we only support uncompressed files!
  if( cdfh->pCompressionMethod != 0 )
    return XRootDStatus( stError, errNotSupported, 0, "Decompression is not supported!" );

  // Now the problem is that at the beginning of our
  // file there is the Local-file-header, which size
  // is not known because of the variable size 'extra'
  // field, so we need to know the offset of the next
  // record and shift it by the file size.
  // The next record is either the next LFH (next file)
  // or the start of the Central-directory.
  uint64_t cdOffset = pZip64Eocd ? pZip64Eocd->pCdOffset : pEocd->pCdOffset;
  uint64_t nextRecordOffset = ( cditr->second + 1 < pCdRecords.size() ) ? pCdRecords[cditr->second + 1]->pOffset : cdOffset;
  uint64_t fileSize = cdfh->pCompressionMethod ? cdfh->pCompressedSize : cdfh->pUncompressedSize;
  uint64_t offset = nextRecordOffset - fileSize + relativeOffset;
  uint64_t sizeTillEnd = fileSize - relativeOffset;
  if( size > sizeTillEnd ) size = sizeTillEnd;

  // check if we have the whole file in our local buffer
  if( pBuffer )
  {
    if( offset + size > pArchiveSize )
    {
      if( userHandler ) userHandler->HandleResponse( new XRootDStatus( stError, errDataError ), 0 );
      return XRootDStatus( stError, errDataError );
    }

    memcpy( buffer, pBuffer.get() + offset, size );

    if( userHandler )
    {
      XRootDStatus *st   = new XRootDStatus();
      AnyObject    *resp = new AnyObject();
      ChunkInfo    *info = new ChunkInfo( relativeOffset, size, buffer );
      resp->Set( info );
      userHandler->HandleResponse( st, resp );
    }
    return XRootDStatus();
  }

  ZipReadHandler *handler = new ZipReadHandler( relativeOffset, this, userHandler );
  XRootDStatus st = pArchive.Read( offset, size, buffer, handler, timeout );
  if( !st.IsOK() ) delete handler;

  return st;
}

DirectoryList* ZipArchiveReaderImpl::List()
{
  std::string value;
  pArchive.GetProperty( "LastURL", value );
  URL url( value );

  StatInfo *infoptr = 0;
  XRootDStatus st = pArchive.Stat( false, infoptr );
  std::unique_ptr<StatInfo> info( infoptr );

  DirectoryList *list = new DirectoryList();
  list->SetParentName( url.GetPath() );

  auto itr = pCdRecords.begin();
  for( ; itr != pCdRecords.end() ; ++itr )
  {
    CDFH *cdfh = *itr;
    StatInfo *entry_info = new StatInfo( info->GetId(),
                                         cdfh->pCdfhSize,
                                         info->GetFlags() & ( ~StatInfo::IsWritable ), // make sure it is not listed as writable
                                         info->GetModTime() );
    DirectoryList::ListEntry *entry =
        new DirectoryList::ListEntry( url.GetHostId(), cdfh->pFilename, entry_info );
    list->Add( entry );
  }

  return list;
}

XRootDStatus ZipArchiveReader::Close( ResponseHandler *handler, uint16_t timeout )
{
  return pImpl->Close( handler, timeout );
}

XRootDStatus ZipArchiveReader::Close( uint16_t timeout )
{
  SyncResponseHandler handler;
  Status st = Close( &handler, timeout );
  if( !st.IsOK() )
    return st;

  return MessageUtils::WaitForStatus( &handler );
}

XRootDStatus ZipArchiveReader::GetSize( const std::string &filename, uint64_t &size ) const
{
  return pImpl->GetSize( filename, size );
}

bool ZipArchiveReader::IsOpen() const
{
  return pImpl->IsOpen();
}

} /* namespace XrdCl */
