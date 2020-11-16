/*
 * XrdZipArchiveReader.cc
 *
 *  Created on: 10 Nov 2020
 *      Author: simonm
 */

#include "XrdZip/XrdZipArchiveReader.hh"
#include "XrdZip/XrdZipZIP64EOCDL.hh"
#include "XrdCl/XrdClFileOperations.hh"

#include <zlib.h>

namespace XrdZip
{

  ArchiveReader::ArchiveReader() : archsize( 0 ), openstage( None )
  {
  }

  ArchiveReader::~ArchiveReader()
  {
  }

  XrdCl::XRootDStatus ArchiveReader::OpenArchive( const std::string      &url,
                                                  XrdCl::ResponseHandler *handler,
                                                  uint16_t                timeout )
  {
    using namespace XrdCl;

    Fwd<uint32_t> rdsize;
    Fwd<uint64_t> rdoff;
    Fwd<void*>    rdbuff;
    uint32_t      maxrdsz = EOCD::maxCommentLength + EOCD::eocdBaseSize +
                            ZIP64_EOCDL::zip64EocdlSize;

    Pipeline open_archive = // open the archive
                            XrdCl::Open( archive, url, OpenFlags::Read ) >> // TODO either Read or Update
                              [=]( XRootDStatus &status, StatInfo &info )
                              {
                                 if( !status.IsOK() )
                                   return handler->HandleResponse( new XrdCl::XRootDStatus( status ), nullptr );
                                 archsize = info.GetSize();
                                 // if it is an empty file (possibly a new file) there's nothing more to do
                                 if( archsize == 0 )
                                 {
                                   openstage = Done;
                                   Pipeline::Stop();
                                 }

                                 rdsize = ( archsize <= maxrdsz ? archsize : maxrdsz );
                                 rdoff  = archsize - *rdsize;
                                 buffer.reset( new char[*rdsize] );
                                 rdbuff = buffer.get();
                                 openstage = HaveEocdBlk;
                               }
                            // read the Central Directory (in several stages if necessary)
                          | XrdCl::Read( archive, rdoff, rdsize, rdbuff ) >>
                              [=]( XRootDStatus &status, ChunkInfo &chunk )
                              {
                                if( !status.IsOK() )
                                  return handler->HandleResponse( new XrdCl::XRootDStatus( status ), nullptr );;

                                const char *buff = reinterpret_cast<char*>( chunk.buffer );

                                while( true )
                                {
                                  switch( openstage )
                                  {
                                    case HaveEocdBlk:
                                    {
                                      // Parse the EOCD record
                                      const char *eocdBlock = EOCD::Find( buff, chunk.length );
                                      if( !eocdBlock )
                                      {
                                        XRootDStatus error( stError, errDataError, 0,
                                                            "End-of-central-directory signature not found." );
                                        handler->HandleResponse( new XrdCl::XRootDStatus( error ), nullptr );;
                                        Pipeline::Stop( error );
                                      }
                                      eocd.reset( new EOCD( eocdBlock ) );

                                      // Do we have the whole archive?
                                      if( chunk.length == archsize )
                                      {
                                        // If we managed to download the whole archive we don't need to
                                        // worry about zip64, it is so small that standard EOCD will do
                                        buff = buff + eocd->cdOffset;
                                        openstage = HaveCdRecords;
                                        continue;
                                      }

                                      // Let's see if it is ZIP64 (if yes, the EOCD will be preceded with ZIP64 EOCD locator)
                                      const char *zip64EocdlBlock = eocdBlock - ZIP64_EOCDL::zip64EocdlSize;
                                      // make sure there is enough data to assume there's a ZIP64 EOCD locator
                                      if( zip64EocdlBlock > buffer.get() )
                                      {
                                        uint32_t signature = to<uint32_t>( zip64EocdlBlock );
                                        if( signature == ZIP64_EOCDL::zip64EocdlSign )
                                        {
                                          buff = zip64EocdlBlock;
                                          openstage = HaveZip64EocdlBlk;
                                          continue;
                                        }
                                      }

                                      // It's not ZIP64, we already know where the CD records are
                                      // we need to read more data
                                      rdoff  = eocd->cdOffset;
                                      rdsize = eocd->cdSize;
                                      buffer.reset( new char[*rdsize] );
                                      rdbuff = buffer.get();
                                      openstage = HaveCdRecords;
                                      Pipeline::Repeat();
                                    }

                                    case HaveZip64EocdlBlk:
                                    {
                                      std::unique_ptr<ZIP64_EOCDL> eocdl( new ZIP64_EOCDL( buff ) );
                                      if( chunk.offset > eocdl->zip64EocdOffset )
                                      {
                                        // we need to read more data
                                        rdsize = archsize - eocdl->zip64EocdOffset;
                                        rdoff  = eocdl->zip64EocdOffset;
                                        buffer.reset( new char[*rdsize] );
                                        rdbuff = buffer.get();
                                        openstage = HaveZip64EocdBlk;
                                        Pipeline::Repeat();
                                      }

                                      buff = buffer.get() + ( eocdl->zip64EocdOffset - chunk.offset );
                                      openstage = HaveZip64EocdBlk;
                                      continue;
                                    }

                                    case HaveZip64EocdBlk:
                                    {
                                      uint32_t signature = to<uint32_t>( buff );
                                      if( signature != ZIP64_EOCD::zip64EocdSign )
                                      {
                                        XrdCl::XRootDStatus error( XrdCl::stError, XrdCl::errDataError, 0,
                                                            "ZIP64 End-of-central-directory signature not found." );
                                        handler->HandleResponse( new XrdCl::XRootDStatus( error ), nullptr );
                                        Pipeline::Stop( error );
                                      }
                                      zip64eocd.reset( new ZIP64_EOCD( buff ) );

                                      // now we can read the CD records
                                      rdoff  = zip64eocd->cdOffset;
                                      rdsize = zip64eocd->cdSize;
                                      buffer.reset( new char[*rdsize] );
                                      rdbuff = buffer.get();
                                      openstage = HaveCdRecords;
                                      Pipeline::Repeat();
                                    }

                                    case HaveCdRecords:
                                    {
                                      try
                                      {
                                        std::tie( cdvec, cdmap ) = CDFH::Parse( buff, eocd->cdSize, eocd->nbCdRec );
                                      }
                                      catch( const bad_data &ex )
                                      {
                                        XrdCl::XRootDStatus error( XrdCl::stError, XrdCl::errDataError, 0,
                                                                   "ZIP Central Directory corrupted." );
                                        handler->HandleResponse( new XrdCl::XRootDStatus( error ), nullptr );
                                        Pipeline::Stop( error );
                                      }
                                      openstage = Done;
                                      handler->HandleResponse( new XrdCl::XRootDStatus( status ), nullptr );
                                      if( chunk.length != archsize ) buffer.reset();
                                      break;
                                    }

                                    default:
                                    {
                                      Pipeline::Stop( XRootDStatus( stError, errInvalidOp ) );
                                    }
                                  }

                                  break;
                                }
                              };


    Async( std::move( open_archive ), timeout );
    return XRootDStatus();
  }

  XrdCl::XRootDStatus ArchiveReader::OpenFile( const std::string       &fn,
                                               XrdCl::OpenFlags::Flags  flags )
  {
    if( !openfn.empty() )
      return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInvalidOp );

    auto itr = cdmap.find( fn );
    if( itr == cdmap.end() )
    {
      if( flags | XrdCl::OpenFlags::New )
      {
        // TODO
        // allocate LFH for the new file
      }

      return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotFound );
    }

    openfn = fn;
    return XrdCl::XRootDStatus();
  }

  XrdCl::XRootDStatus ArchiveReader::CloseArchive( XrdCl::ResponseHandler *handler,
                                                   uint16_t                timeout )
  {
    buffer.reset();
    eocd.reset();
    cdvec.clear();
    cdmap.clear();
    zip64eocd.reset();
    openstage = None;
    return archive.Close( handler, timeout );
  }

  XrdCl::XRootDStatus ArchiveReader::Read( uint64_t                relativeOffset,
                                           uint32_t                size,
                                           void                   *usrbuff,
                                           XrdCl::ResponseHandler *usrHandler,
                                           uint16_t                timeout )
  {
    if( !archive.IsOpen() )
      return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInvalidOp,
                                  XrdCl::errInvalidOp, "Archive not opened." );

    auto cditr = cdmap.find( openfn );
    if( cditr == cdmap.end() )
      return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotFound,
                                  XrdCl::errNotFound, "File not found." );
    CDFH *cdfh = cdvec[cditr->second].get();

    // check if the file is compressed, for now we only support uncompressed and inflate/deflate compression
    if( cdfh->compressionMethod != 0 && cdfh->compressionMethod != Z_DEFLATED )
      return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported,
                                  0, "The compression algorithm is not supported!" );

    // Now the problem is that at the beginning of our
    // file there is the Local-file-header, which size
    // is not known because of the variable size 'extra'
    // field, so we need to know the offset of the next
    // record and shift it by the file size.
    // The next record is either the next LFH (next file)
    // or the start of the Central-directory.
    uint64_t cdOffset = zip64eocd ? zip64eocd->cdOffset : eocd->cdOffset;
    uint64_t nextRecordOffset = ( cditr->second + 1 < cdvec.size() ) ?
                                cdvec[cditr->second + 1]->offset : cdOffset;
    uint64_t filesize  = cdfh->compressedSize;
    uint64_t fileoff  = nextRecordOffset - filesize;
    uint64_t offset   = fileoff + relativeOffset;
    uint64_t sizeTillEnd = relativeOffset > cdfh->compressedSize ?
                           0 : cdfh->compressedSize - relativeOffset;
    if( size > sizeTillEnd ) size = sizeTillEnd;

    // if it is a compressed file use ZIP cache to read from the file
    if( cdfh->compressionMethod == Z_DEFLATED )
    {
      // check if respective ZIP cache exists
      bool empty = inflcache.find( openfn ) == inflcache.end();
      // if the entry does not exist, it will be created using
      // default constructor
      InflCache &cache = inflcache[openfn];
      // if we have the whole ZIP archive we can populate the cache
      // straight away
      if( empty && buffer)
      {
        XrdCl::XRootDStatus st = cache.Input( buffer.get() + offset, filesize - fileoff, relativeOffset );
        if( !st.IsOK() ) return st;
      }

      XrdCl::XRootDStatus st = cache.Output( usrbuff, size, relativeOffset );

      // read from cache
      if( !empty || buffer )
      {
        uint32_t bytesRead = 0;
        st = cache.Read( bytesRead );
        // propagate errors to the end-user
        if( !st.IsOK() ) return st;
        // we have all the data ...
        if( st.code == XrdCl::suDone )
        {
          if( usrHandler )
          {
            XrdCl::XRootDStatus *st = new XrdCl::XRootDStatus();
            XrdCl::ChunkInfo    *ch = new XrdCl::ChunkInfo( relativeOffset, size, usrbuff );
            Schedule( usrHandler, st, ch );
          }
          return XrdCl::XRootDStatus();
        }
      }

      // the raw offset of the next chunk within the file
      uint64_t rawOffset = cache.NextChunkOffset();
      // if this is the first time we are setting an input chunk
      // use the user-specified offset
      if( !rawOffset )
        rawOffset = relativeOffset;
      // size of the next chunk of raw (compressed) data
      uint32_t chunkSize = size;
      // make sure we are not reading passed the end of the file
      if( rawOffset + chunkSize > filesize )
        chunkSize = filesize - rawOffset;
      // allocate the buffer for the compressed data
      buffer.reset( new char[chunkSize] );

      XrdCl::Pipeline p = XrdCl::Read( archive, fileoff + rawOffset, chunkSize, buffer.get() ) >>
                            [=, &cache]( XrdCl::XRootDStatus &st, XrdCl::ChunkInfo &ch )
                            {
                              if( !st.IsOK() )
                              {
                                if( usrHandler ) usrHandler->HandleResponse( new XrdCl::XRootDStatus( st ), nullptr );
                                return;
                              }

                              st = cache.Input( ch.buffer, ch.length, rawOffset );
                              if( !st.IsOK() )
                              {
                                if( usrHandler ) usrHandler->HandleResponse( new XrdCl::XRootDStatus( st ), nullptr );
                                XrdCl::Pipeline::Stop( st );
                              }

                              // at this point we can be sure that all the needed data are in the cache
                              // (we requested as much data as the user asked for so in the worst case
                              // we have exactly as much data as the user needs, most likely we have
                              // more because the data are compressed)
                              uint32_t bytesRead = 0;
                              st = cache.Read( bytesRead );
                              if( !st.IsOK() )
                              {
                                if( usrHandler ) usrHandler->HandleResponse( new XrdCl::XRootDStatus( st ), nullptr );
                                XrdCl::Pipeline::Stop( st );
                              }

                              // call the user handler
                              if( usrHandler)
                              {
                                XrdCl::ChunkInfo *rsp = new XrdCl::ChunkInfo( relativeOffset, size, usrbuff );
                                usrHandler->HandleResponse( new XrdCl::XRootDStatus(), PkgRsp( rsp ) );
                              }
                            };
      XrdCl::Async( std::move( p ), timeout );
      return XrdCl::XRootDStatus();
    }

    // check if we have the whole file in our local buffer
    if( buffer || size == 0 )
    {
      if( size ) memcpy( usrbuff, buffer.get() + offset, size );

      if( usrHandler )
      {
        XrdCl::XRootDStatus *st = new XrdCl::XRootDStatus();
        XrdCl::ChunkInfo    *ch = new XrdCl::ChunkInfo( relativeOffset, size, usrbuff );
        Schedule( usrHandler, st, ch );
      }
      return XrdCl::XRootDStatus();
    }

    XrdCl::Pipeline p = XrdCl::Read( archive, offset, size, usrbuff ) >>
                          [=]( XrdCl::XRootDStatus &st, XrdCl::ChunkInfo &chunk )
                          {
                            if( usrHandler )
                            {
                              XrdCl::XRootDStatus *status = new XrdCl::XRootDStatus( st );
                              XrdCl::ChunkInfo    *rsp = nullptr;
                              if( st.IsOK() )
                                rsp = new XrdCl::ChunkInfo( relativeOffset, chunk.length,
                                                            chunk.buffer );
                              usrHandler->HandleResponse( status, PkgRsp( rsp ) );
                            }
                          };
    XrdCl::Async( std::move( p ), timeout );
    return XrdCl::XRootDStatus();
  }

} /* namespace XrdZip */
