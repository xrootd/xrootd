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

#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClZipArchive.hh"
#include "XrdZip/XrdZipZIP64EOCDL.hh"

#include <sys/stat.h>

namespace XrdCl
{

  using namespace XrdZip;

  ZipArchive::ZipArchive() : archsize( 0 ),
                             cdexists( false ),
                             updated( false ),
                             cdoff( 0 ),
                             orgcdsz( 0 ),
                             orgcdcnt( 0 ),
                             openstage( None ),
                             flags( OpenFlags::None )
  {
  }

  ZipArchive::~ZipArchive()
  {
  }

  XRootDStatus ZipArchive::OpenOnly( const std::string  &url,
                                     ResponseHandler    *handler,
                                     uint16_t            timeout )
  {
    Pipeline open_only = XrdCl::Open( archive, url, OpenFlags::Read ) >>
                           [=]( XRootDStatus &st, StatInfo &info )
                           {
                             // check the status is OK
                             if( st.IsOK() )
                             {
                               archsize  = info.GetSize();
                               openstage = NotParsed;
                             }
                             if( handler )
                               handler->HandleResponse( make_status( st ), nullptr );
                           };

    Async( std::move( open_only ), timeout );
    return XRootDStatus();
  }

  XRootDStatus ZipArchive::OpenArchive( const std::string  &url,
                                        OpenFlags::Flags    flags,
                                        ResponseHandler    *handler,
                                        uint16_t            timeout )
  {
    Fwd<uint32_t> rdsize;
    Fwd<uint64_t> rdoff;
    Fwd<void*>    rdbuff;
    uint32_t      maxrdsz = EOCD::maxCommentLength + EOCD::eocdBaseSize +
                            ZIP64_EOCDL::zip64EocdlSize;

    Pipeline open_archive = // open the archive
                            XrdCl::Open( archive, url, flags ) >>
                              [=]( XRootDStatus &status, StatInfo &info )
                              {
                                 // check the status is OK
                                 if( !status.IsOK() ) return;

                                 archsize = info.GetSize();
                                 // if it is an empty file (possibly a new file) there's nothing more to do
                                 if( archsize == 0 )
                                 {
                                   cdexists = false;
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
                                // check the status is OK
                                if( !status.IsOK() ) return;

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
                                        Pipeline::Stop( error );
                                      }
                                      eocd.reset( new EOCD( eocdBlock ) );

                                      // Do we have the whole archive?
                                      if( chunk.length == archsize )
                                      {
                                        // If we managed to download the whole archive we don't need to
                                        // worry about zip64, it is so small that standard EOCD will do
                                        cdoff     = eocd->cdOffset;
                                        orgcdsz   = eocd->cdSize;
                                        orgcdcnt  = eocd->nbCdRec;
                                        buff = buff + cdoff;
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
                                      cdoff     = eocd->cdOffset;
                                      orgcdsz   = eocd->cdSize;
                                      orgcdcnt  = eocd->nbCdRec;
                                      rdoff     = eocd->cdOffset;
                                      rdsize    = eocd->cdSize;
                                      buffer.reset( new char[*rdsize] );
                                      rdbuff    = buffer.get();
                                      openstage = HaveCdRecords;
                                      Pipeline::Repeat(); break; // the break is really not needed ...
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
                                        XRootDStatus error( stError, errDataError, 0,
                                                            "ZIP64 End-of-central-directory signature not found." );
                                        Pipeline::Stop( error );
                                      }
                                      zip64eocd.reset( new ZIP64_EOCD( buff ) );

                                      // now we can read the CD records
                                      cdoff     = zip64eocd->cdOffset;
                                      orgcdsz   = zip64eocd->cdSize;
                                      orgcdcnt  = zip64eocd->nbCdRec;
                                      rdoff     = zip64eocd->cdOffset;
                                      rdsize    = zip64eocd->cdSize;
                                      buffer.reset( new char[*rdsize] );
                                      rdbuff    = buffer.get();
                                      openstage = HaveCdRecords;
                                      Pipeline::Repeat(); break; // the break is really not needed ...
                                    }

                                    case HaveCdRecords:
                                    {
                                      // make a copy of the original CDFH records
                                      orgcdbuf.reserve( orgcdsz );
                                      std::copy( buff, buff + orgcdsz, std::back_inserter( orgcdbuf ) );
                                      try
                                      {
                                        std::tie( cdvec, cdmap ) = CDFH::Parse( buff, eocd->cdSize, eocd->nbCdRec );
                                      }
                                      catch( const bad_data &ex )
                                      {
                                        XRootDStatus error( stError, errDataError, 0,
                                                                   "ZIP Central Directory corrupted." );
                                        Pipeline::Stop( error );
                                      }
                                      if( chunk.length != archsize ) buffer.reset();
                                      openstage = Done;
                                      break;
                                    }

                                    default: Pipeline::Stop( XRootDStatus( stError, errInvalidOp ) );
                                  }

                                  break;
                                }
                              }
                          | XrdCl::Final( [handler]( const XRootDStatus &status )
                              {
                                if( handler )
                                  handler->HandleResponse( make_status( status ), nullptr );
                              } );

    Async( std::move( open_archive ), timeout );
    return XRootDStatus();
  }

  XRootDStatus ZipArchive::OpenFile( const std::string &fn,
                                     OpenFlags::Flags   flags,
                                     uint64_t           size,
                                     uint32_t           crc32 )
  {
    if( !openfn.empty() || openstage != Done || !archive.IsOpen() )
      return XRootDStatus( stError, errInvalidOp );

    this->flags = flags;
    auto  itr   = cdmap.find( fn );
    if( itr == cdmap.end() )
    {
      // the file does not exist in the archive so it only makes sense
      // if our user is opening for append
      if( flags | OpenFlags::New )
      {
        openfn = fn;
        lfh.reset( new LFH( fn, crc32, size, time( 0 ) ) );
        return XRootDStatus();
      }
      return XRootDStatus( stError, errNotFound );
    }

    // the file name exist in the archive but our user wants to append
    // a file with the same name
    if( flags | OpenFlags::New ) return XRootDStatus( stError, errInvalidOp );

    openfn = fn;
    return XRootDStatus();
  }

  buffer_t ZipArchive::GetCD()
  {
    uint32_t size = 0;
    uint32_t cdsize  = CDFH::CalcSize( cdvec, orgcdsz, orgcdcnt );
    // first create the EOCD record
    eocd.reset( new EOCD( cdoff, cdvec.size(), cdsize ) );
    size += eocd->eocdSize ;
    size += eocd->cdSize;
    // then create zip64eocd & zip64eocdl if necessary
    std::unique_ptr<ZIP64_EOCD>  zip64eocd;
    std::unique_ptr<ZIP64_EOCDL> zip64eocdl;
    if( eocd->useZip64 )
    {
      zip64eocd.reset( new ZIP64_EOCD( cdoff, cdvec.size(), cdsize ) );
      size += zip64eocd->zip64EocdTotalSize;
      zip64eocdl.reset( new ZIP64_EOCDL( *eocd, *zip64eocd ) );
      size += ZIP64_EOCDL::zip64EocdlSize;
    }

    // Now serialize all records into a buffer
    buffer_t metadata;
    metadata.reserve( size );
    CDFH::Serialize( orgcdcnt, orgcdbuf, cdvec, metadata );
    if( zip64eocd )
      zip64eocd->Serialize( metadata );
    if( zip64eocdl )
      zip64eocdl->Serialize( metadata );
    eocd->Serialize( metadata );

    return metadata;
  }

  void ZipArchive::SetCD( const buffer_t &buffer )
  {
    if( openstage != NotParsed ) return;

    const char *buff = buffer.data();
    size_t      size = buffer.size();
    // parse Central Directory records
    std::tie(cdvec, cdmap ) = CDFH::Parse( buff, size );
    // make a copy of the original CDFH records
    orgcdsz  = buff - buffer.data();
    orgcdcnt = cdvec.size();
    orgcdbuf.reserve( orgcdsz );
    std::copy( buffer.data(), buff, std::back_inserter( orgcdbuf ) );
    // parse ZIP64EOCD record if exists
    uint32_t signature = to<uint32_t>( buff );
    if( signature == ZIP64_EOCD::zip64EocdSign )
    {
      zip64eocd.reset( new ZIP64_EOCD( buff ) );
      buff += zip64eocd->zip64EocdTotalSize;
      // now shift the buffer by EOCDL size if necessary
      signature = to<uint32_t>( buff );
      if( signature == ZIP64_EOCDL::zip64EocdlSign )
        buff += ZIP64_EOCDL::zip64EocdlSize;
    }
    // parse EOCD record
    eocd.reset( new EOCD( buff ) );
    // update the state of the ZipArchive object
    openstage = XrdCl::ZipArchive::Done;
    cdexists  = true;
  }

  XRootDStatus ZipArchive::CloseArchive( ResponseHandler *handler,
                                      uint16_t         timeout )
  {
    if( updated )
    {
      uint64_t wrtoff  = cdoff;
      auto wrtbuff = std::make_shared<buffer_t>( GetCD() );

      Pipeline p = XrdCl::Write( archive, wrtoff, wrtbuff->size(), wrtbuff->data() )
                 | Close( archive ) >>
                     [=]( XRootDStatus &st )
                     {
                       if( st.IsOK() ) Clear();
                       else openstage = Error;
                     }
                 | XrdCl::Final( [wrtbuff, handler]( const XRootDStatus &st ) mutable
                     {
                       wrtbuff.reset();
                       if( handler ) handler->HandleResponse( make_status( st ), nullptr );
                     } );
      Async( std::move( p ), timeout );
      return XRootDStatus();
    }

    Pipeline p = Close( archive ) >>
                          [=]( XRootDStatus &st )
                          {
                            if( st.IsOK() ) Clear();
                            else openstage = Error;
                            if( handler ) handler->HandleResponse( make_status( st ), nullptr );
                          };
    Async( std::move( p ), timeout );
    return XRootDStatus();
  }

  XRootDStatus ZipArchive::ReadFrom( const std::string &fn,
                                     uint64_t           relativeOffset,
                                     uint32_t           size,
                                     void              *usrbuff,
                                     ResponseHandler   *usrHandler,
                                     uint16_t           timeout )
  {
    if( openstage != Done || !archive.IsOpen() )
      return XRootDStatus( stError, errInvalidOp );

    auto cditr = cdmap.find( fn );
    if( cditr == cdmap.end() )
      return XRootDStatus( stError, errNotFound,
                           errNotFound, "File not found." );
    CDFH *cdfh = cdvec[cditr->second].get();

    // check if the file is compressed, for now we only support uncompressed and inflate/deflate compression
    if( cdfh->compressionMethod != 0 && cdfh->compressionMethod != Z_DEFLATED )
      return XRootDStatus( stError, errNotSupported,
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
      bool empty = zipcache.find( fn ) == zipcache.end();
      // if the entry does not exist, it will be created using
      // default constructor
      ZipCache &cache = zipcache[fn];
      // if we have the whole ZIP archive we can populate the cache
      // straight away
      if( empty && buffer)
      {
        XRootDStatus st = cache.Input( buffer.get() + offset, filesize - fileoff, relativeOffset );
        if( !st.IsOK() ) return st;
      }

      XRootDStatus st = cache.Output( usrbuff, size, relativeOffset );

      // read from cache
      if( !empty || buffer )
      {
        uint32_t bytesRead = 0;
        st = cache.Read( bytesRead );
        // propagate errors to the end-user
        if( !st.IsOK() ) return st;
        // we have all the data ...
        if( st.code == suDone )
        {
          if( usrHandler )
          {
            XRootDStatus *st = make_status();
            ChunkInfo    *ch = new ChunkInfo( relativeOffset, size, usrbuff );
            Schedule( usrHandler, st, ch );
          }
          return XRootDStatus();
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
      Fwd<ChunkInfo> chunk;
      Pipeline p = XrdCl::Read( archive, fileoff + rawOffset, chunkSize, buffer.get() ) >>
                     [=, &cache]( XRootDStatus &st, ChunkInfo &ch )
                     {
                       if( !st.IsOK() ) return;

                       st = cache.Input( ch.buffer, ch.length, rawOffset );
                       if( !st.IsOK() ) Pipeline::Stop( st );

                       // at this point we can be sure that all the needed data are in the cache
                       // (we requested as much data as the user asked for so in the worst case
                       // we have exactly as much data as the user needs, most likely we have
                       // more because the data are compressed)
                       uint32_t bytesRead = 0;
                       st = cache.Read( bytesRead );
                       if( !st.IsOK() ) Pipeline::Stop( st );

                       // forward server response to the final operation
                       chunk->buffer = usrbuff;
                       chunk->length = size;
                       chunk->offset = relativeOffset;
                     }
                 | XrdCl::Final( [=]( const XRootDStatus &st ) mutable
                     {
                       buffer.reset();
                       AnyObject *rsp = nullptr;
                       if( st.IsOK() ) rsp = PkgRsp( new ChunkInfo( *chunk ) );
                       if( usrHandler ) usrHandler->HandleResponse( make_status( st ), rsp );
                     } );
      Async( std::move( p ), timeout );
      return XRootDStatus();
    }

    // check if we have the whole file in our local buffer
    if( buffer || size == 0 )
    {
      if( size ) memcpy( usrbuff, buffer.get() + offset, size );

      if( usrHandler )
      {
        XRootDStatus *st = make_status();
        ChunkInfo    *ch = new ChunkInfo( relativeOffset, size, usrbuff );
        Schedule( usrHandler, st, ch );
      }
      return XRootDStatus();
    }

    Pipeline p = XrdCl::Read( archive, offset, size, usrbuff ) >>
                   [=]( XRootDStatus &st, ChunkInfo &chunk )
                   {
                     if( usrHandler )
                     {
                       XRootDStatus *status = make_status( st );
                       ChunkInfo    *rsp = nullptr;
                       if( st.IsOK() )
                         rsp = new ChunkInfo( relativeOffset, chunk.length, chunk.buffer );
                       usrHandler->HandleResponse( status, PkgRsp( rsp ) );
                     }
                   };
    Async( std::move( p ), timeout );
    return XRootDStatus();
  }

  XRootDStatus ZipArchive::List( DirectoryList *&list )
  {
    if( openstage != Done )
      return XRootDStatus( stError, errInvalidOp,
                                  errInvalidOp, "Archive not opened." );

    std::string value;
    archive.GetProperty( "LastURL", value );
    URL url( value );

    StatInfo *infoptr = 0;
    XRootDStatus st = archive.Stat( false, infoptr );
    std::unique_ptr<StatInfo> info( infoptr );

    list = new DirectoryList();
    list->SetParentName( url.GetPath() );

    auto itr = cdvec.begin();
    for( ; itr != cdvec.end() ; ++itr )
    {
      CDFH *cdfh = itr->get();
      StatInfo *entry_info = make_stat( *info, cdfh->uncompressedSize );
      DirectoryList::ListEntry *entry =
          new DirectoryList::ListEntry( url.GetHostId(), cdfh->filename, entry_info );
      list->Add( entry );
    }

    return XRootDStatus();
  }

  XRootDStatus ZipArchive::Write( uint32_t         size,
                                  const void      *buffer,
                                  ResponseHandler *handler,
                                  uint16_t         timeout )
  {
    if( openstage != Done || openfn.empty() )
      return XRootDStatus( stError, errInvalidOp,
                           errInvalidOp, "Archive not opened." );

    if( cdexists )
    {
      // TODO if this is an append: checkpoint the EOCD&co
      cdexists = false;
    }

    static const int iovcnt = 2;
    iovec iov[iovcnt];

    //-------------------------------------------------------------------------
    // If there is a LFH we need to write it first ahead of the write-buffer
    // itself.
    //-------------------------------------------------------------------------
    std::shared_ptr<buffer_t> lfhbuf;
    if( lfh )
    {
      uint32_t lfhlen = lfh->lfhSize;
      lfhbuf.reset( new buffer_t() );
      lfhbuf->reserve( lfhlen );
      lfh->Serialize( *lfhbuf );
      iov[0].iov_base = lfhbuf->data();
      iov[0].iov_len  = lfhlen;
    }
    //-------------------------------------------------------------------------
    // If there is no LFH just make the first chunk empty.
    //-------------------------------------------------------------------------
    else
    {
      iov[0].iov_base = nullptr;
      iov[0].iov_len  = 0;
    }

    //-------------------------------------------------------------------------
    // In the second chunk write the user data
    //-------------------------------------------------------------------------
    iov[1].iov_base = const_cast<void*>( buffer );
    iov[1].iov_len  = size;

    uint64_t wrtoff = cdoff; // we only support appending
    uint32_t wrtlen = iov[0].iov_len + iov[1].iov_len;
    Pipeline p = XrdCl::WriteV( archive, wrtoff, iov, iovcnt ) >>
                   [=]( XRootDStatus &st ) mutable
                   {
                     if( st.IsOK() )
                     {
                       updated   = true;
                       archsize += wrtlen;
                       cdoff    += wrtlen;
                       mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
                       cdvec.emplace_back( new CDFH( lfh.get(), mode, wrtoff ) );
                       cdmap[openfn] = cdvec.size() - 1;
                     }
                     lfh.reset();
                     lfhbuf.reset();
                     if( handler )
                       handler->HandleResponse( make_status( st ), nullptr );
                   };

    Async( std::move( p ), timeout );
    return XRootDStatus();
  }

} /* namespace XrdZip */
