/*
 * XrdZipArchiveReader.cc
 *
 *  Created on: 10 Nov 2020
 *      Author: simonm
 */

#include "XrdZip/XrdZipArchiveReader.hh"
#include "XrdZip/XrdZipZIP64EOCDL.hh"
#include "XrdCl/XrdClFileOperations.hh"

namespace XrdZip
{

  ArchiveReader::ArchiveReader() : archsize( 0 ),
                                   newarch( false ),
                                   openstage( None ),
                                   flags( XrdCl::OpenFlags::None )
  {
  }

  ArchiveReader::~ArchiveReader()
  {
  }

  XrdCl::XRootDStatus ArchiveReader::OpenArchive( const std::string      &url,
                                                  XrdCl::OpenFlags::Flags flags,
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
                            XrdCl::Open( archive, url, flags ) >>
                              [=]( XRootDStatus &status, StatInfo &info )
                              {
                                 if( !status.IsOK() )
                                   return handler->HandleResponse( make_status( status ), nullptr );
                                 archsize = info.GetSize();
                                 // if it is an empty file (possibly a new file) there's nothing more to do
                                 if( archsize == 0 )
                                 {
                                   newarch = true;
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
                                  return handler->HandleResponse( make_status( status ), nullptr );;

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
                                        handler->HandleResponse( make_status( error ), nullptr );;
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
                                        handler->HandleResponse( make_status( error ), nullptr );
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
                                        handler->HandleResponse( make_status( error ), nullptr );
                                        Pipeline::Stop( error );
                                      }
                                      openstage = Done;
                                      handler->HandleResponse( make_status( status ), nullptr );
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
                                               XrdCl::OpenFlags::Flags  flags,
                                               uint64_t                 size,
                                               uint32_t                 crc32,
                                               XrdCl::ResponseHandler  *handler,
                                               uint16_t                 timeout )
  {
    if( !openfn.empty() )
      return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInvalidOp );

    this->flags = flags;
    auto itr = cdmap.find( fn );
    if( itr == cdmap.end() )
    {
      if( flags | XrdCl::OpenFlags::New )
      {
        openfn = fn;
        lfh.reset( new LFH( fn, crc32, size, time( 0 ) ) );

        uint64_t wrtoff = archsize;
        uint32_t wrtlen = lfh->lfhSize;
        buffer_t wrtbuf;
        wrtbuf.reserve( wrtlen );
        lfh->Serialize( wrtbuf );
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        if( !newarch )
        {
          // TODO set wrtoff to cdoff and shift cdoff
          // TODO if this is an append: checkpoint the EOCD&co
        }

        XrdCl::Pipeline p = XrdCl::Write( archive, wrtoff, lfh->lfhSize, wrtbuf.data() ) >>
                              [=]( XrdCl::XRootDStatus &st )
                              {
                                if( st.IsOK() )
                                {
                                  archsize += wrtlen;
                                  cdvec.emplace_back( new CDFH( lfh.get(), mode, wrtoff ) );
                                  cdmap[fn] = cdvec.size() - 1;
                                }
                                if( handler )
                                  handler->HandleResponse( make_status( st ), nullptr );
                                lfh.reset();
                              };
        XrdCl::Async( std::move( p ), timeout );
        return XrdCl::XRootDStatus();
      }
      return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotFound );
    }

    openfn = fn;
    if( handler ) Schedule( handler, make_status() );
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
    if( openstage != Done || openfn.empty() )
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
                                CDFH::GetOffset( *cdvec[cditr->second + 1] ) : cdOffset;
    uint64_t filesize  = cdfh->compressedSize;
    uint64_t fileoff  = nextRecordOffset - filesize;
    uint64_t offset   = fileoff + relativeOffset;
    uint64_t sizeTillEnd = relativeOffset > cdfh->uncompressedSize ?
                           0 : cdfh->uncompressedSize - relativeOffset;
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
            XrdCl::XRootDStatus *st = make_status();
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
                                if( usrHandler ) usrHandler->HandleResponse( make_status( st ), nullptr );
                                return;
                              }
                              st = cache.Input( ch.buffer, ch.length, rawOffset );
                              if( !st.IsOK() )
                              {
                                if( usrHandler ) usrHandler->HandleResponse( make_status( st ), nullptr );
                                buffer.reset();
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
                                if( usrHandler ) usrHandler->HandleResponse( make_status( st ), nullptr );
                                buffer.reset();
                                XrdCl::Pipeline::Stop( st );
                              }

                              // call the user handler
                              if( usrHandler)
                              {
                                XrdCl::ChunkInfo *rsp = new XrdCl::ChunkInfo( relativeOffset, size, usrbuff );
                                usrHandler->HandleResponse( make_status(), PkgRsp( rsp ) );
                              }
                              buffer.reset();
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
        XrdCl::XRootDStatus *st = make_status();
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
                              XrdCl::XRootDStatus *status = make_status( st );
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

  XrdCl::XRootDStatus ArchiveReader::List( XrdCl::DirectoryList *&list )
  {
    if( openstage != Done )
      return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInvalidOp,
                                  XrdCl::errInvalidOp, "Archive not opened." );

    std::string value;
    archive.GetProperty( "LastURL", value );
    XrdCl::URL url( value );

    XrdCl::StatInfo *infoptr = 0;
    XrdCl::XRootDStatus st = archive.Stat( false, infoptr );
    std::unique_ptr<XrdCl::StatInfo> info( infoptr );

    list = new XrdCl::DirectoryList();
    list->SetParentName( url.GetPath() );

    auto itr = cdvec.begin();
    for( ; itr != cdvec.end() ; ++itr )
    {
      CDFH *cdfh = itr->get();
      XrdCl::StatInfo *entry_info = make_stat( *info, cdfh->uncompressedSize );
      XrdCl::DirectoryList::ListEntry *entry =
          new XrdCl::DirectoryList::ListEntry( url.GetHostId(), cdfh->filename, entry_info );
      list->Add( entry );
    }

    return XrdCl::XRootDStatus();
  }

  XrdCl::XRootDStatus ArchiveReader::Write( uint32_t                size,
                                            const void             *buffer,
                                            XrdCl::ResponseHandler *handler,
                                            uint16_t                timeout )
  {
    uint64_t wrtoff = archsize; // we only support appending

    XrdCl::Pipeline p = XrdCl::Write( archive, wrtoff, size, buffer ) >>
                          [=]( XrdCl::XRootDStatus &st )
                          {
                            if( st.IsOK() ) archsize += size;
                            if( handler )
                              handler->HandleResponse( make_status( st ), nullptr );
                          };
    XrdCl::Async( std::move( p ), timeout );
    return XrdCl::XRootDStatus();
  }

} /* namespace XrdZip */
