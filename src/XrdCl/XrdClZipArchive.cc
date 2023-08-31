//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------

#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClCheckpointOperation.hh"
#include "XrdCl/XrdClZipArchive.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdZip/XrdZipZIP64EOCDL.hh"

#include <sys/stat.h>

namespace XrdCl
{
  using namespace XrdZip;

  //---------------------------------------------------------------------------
  // Read data from a given file
  //---------------------------------------------------------------------------
  template<typename RSP>
  XRootDStatus ReadFromImpl( ZipArchive        &me,
                             const std::string &fn,
                             uint64_t           relativeOffset,
                             uint32_t           size,
                             void              *usrbuff,
                             ResponseHandler   *usrHandler,
                             uint16_t           timeout )
  {
    if( me.openstage != ZipArchive::Done || !me.archive.IsOpen() )
      return XRootDStatus( stError, errInvalidOp );

    Log *log = DefaultEnv::GetLog();

    auto cditr = me.cdmap.find( fn );
    if( cditr == me.cdmap.end() )
      return XRootDStatus( stError, errNotFound,
                           errNotFound, "File not found." );

    CDFH *cdfh = me.cdvec[cditr->second].get();

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
    uint64_t cdOffset = me.zip64eocd ? me.zip64eocd->cdOffset : me.eocd->cdOffset;
    uint64_t nextRecordOffset = ( cditr->second + 1 < me.cdvec.size() ) ?
                                CDFH::GetOffset( *me.cdvec[cditr->second + 1] ) : cdOffset;
    uint64_t filesize = cdfh->compressedSize;
    if( filesize == std::numeric_limits<uint32_t>::max() && cdfh->extra )
      filesize = cdfh->extra->compressedSize;
    uint16_t descsize = cdfh->HasDataDescriptor() ?
                        DataDescriptor::GetSize( cdfh->IsZIP64() ) : 0;
    uint64_t fileoff  = nextRecordOffset - filesize - descsize;
    uint64_t offset   = fileoff + relativeOffset;
    uint64_t uncompressedSize = cdfh->uncompressedSize;
    if( uncompressedSize == std::numeric_limits<uint32_t>::max() && cdfh->extra )
      uncompressedSize = cdfh->extra->uncompressedSize;
    uint64_t sizeTillEnd = relativeOffset > uncompressedSize ?
                           0 : uncompressedSize - relativeOffset;
    if( size > sizeTillEnd ) size = sizeTillEnd;

    // if it is a compressed file use ZIP cache to read from the file
    if( cdfh->compressionMethod == Z_DEFLATED )
    {
      log->Dump( ZipMsg, "[0x%x] Reading compressed data.", &me );
      // check if respective ZIP cache exists
      bool empty = me.zipcache.find( fn ) == me.zipcache.end();
      // if the entry does not exist, it will be created using
      // default constructor
      ZipCache &cache = me.zipcache[fn];

      if( relativeOffset > uncompressedSize )
      {
        // we are reading past the end of file,
        // we can serve the request right away!
        RSP *r = new RSP( relativeOffset, 0, usrbuff );
        AnyObject *rsp = new AnyObject();
        rsp->Set( r );
        usrHandler->HandleResponse( new XRootDStatus(), rsp );
        return XRootDStatus();
      }

      uint32_t sizereq = size;
      if( relativeOffset + size > uncompressedSize )
        sizereq = uncompressedSize - relativeOffset;
      cache.QueueReq( relativeOffset, sizereq, usrbuff, usrHandler );

      // if we have the whole ZIP archive we can populate the cache
      // straight away
      if( empty && me.buffer)
      {
        auto begin = me.buffer.get() + fileoff;
        auto end   = begin + filesize ;
        buffer_t buff( begin, end );
        cache.QueueRsp( XRootDStatus(), 0, std::move( buff ) );
        return XRootDStatus();
      }

      // if we don't have the data we need to issue a remote read
      if( !me.buffer )
      {
        if( relativeOffset > filesize ) return XRootDStatus(); // there's nothing to do,
                                                                           // we already have all the data locally
        uint32_t rdsize = size;
        // check if this is the last read (we reached the end of
        // file from user perspective)
        if( relativeOffset + size >= uncompressedSize )
        {
          // if yes, make sure we readout all the compressed data
          // Note: In a patological case the compressed size may
          //       be greater than the uncompressed size
          rdsize = filesize > relativeOffset ?
                   filesize - relativeOffset :
                   0;
        }
        // make sure we are not reading past the end of
        // compressed data
        if( relativeOffset + size > filesize )
          rdsize = filesize - relativeOffset;


        // now read the data ...
        auto rdbuff = std::make_shared<ZipCache::buffer_t>( rdsize );
        Pipeline p = XrdCl::RdWithRsp<RSP>( me.archive, offset, rdbuff->size(), rdbuff->data() ) >>
                       [relativeOffset, rdbuff, &cache, &me]( XRootDStatus &st, RSP &rsp )
                       {
                         Log *log = DefaultEnv::GetLog();
                         log->Dump( ZipMsg, "[0x%x] Read %d bytes of remote data at offset %d.",
                                            &me, rsp.GetLength(), rsp.GetOffset() );
                         cache.QueueRsp( st, relativeOffset, std::move( *rdbuff ) );
                       };
        Async( std::move( p ), timeout );
      }

      return XRootDStatus();
    }

    // check if we have the whole file in our local buffer
    if( me.buffer || size == 0 )
    {
      if( size )
      {
        memcpy( usrbuff, me.buffer.get() + offset, size );
        log->Dump( ZipMsg, "[0x%x] Serving read from local cache.", &me );
      }

      if( usrHandler )
      {
        XRootDStatus *st  = ZipArchive::make_status();
        RSP          *rsp = new RSP( relativeOffset, size, usrbuff );
        ZipArchive::Schedule( usrHandler, st, rsp );
      }
      return XRootDStatus();
    }

    Pipeline p = XrdCl::RdWithRsp<RSP>( me.archive, offset, size, usrbuff ) >>
                   [=, &me]( XRootDStatus &st, RSP &r )
                   {
                     log->Dump( ZipMsg, "[0x%x] Read %d bytes of remote data at "
                                        "offset %d.", &me, r.GetLength(), r.GetOffset() );
                     if( usrHandler )
                     {
                       XRootDStatus *status = ZipArchive::make_status( st );
                       RSP          *rsp    = nullptr;
                       if( st.IsOK() )
                         rsp = new RSP( relativeOffset, r.GetLength(), r.GetBuffer() );
                       usrHandler->HandleResponse( status, ZipArchive::PkgRsp( rsp ) );
                     }
                   };
    Async( std::move( p ), timeout );
    return XRootDStatus();
  }

  //---------------------------------------------------------------------------
  // Constructor
  //---------------------------------------------------------------------------
  ZipArchive::ZipArchive( bool enablePlugIns) : archive( enablePlugIns ),
                                                archsize( 0 ),
                                                cdexists( false ),
                                                updated( false ),
                                                cdoff( 0 ),
                                                orgcdsz( 0 ),
                                                orgcdcnt( 0 ),
                                                openstage( None ),
                                                ckpinit( false )
  {
  }

  //---------------------------------------------------------------------------
  // Destructor
  //---------------------------------------------------------------------------
  ZipArchive::~ZipArchive()
  {
  }

  //---------------------------------------------------------------------------
  // Open the ZIP archive in read-only mode without parsing the central
  // directory.
  //---------------------------------------------------------------------------
  XRootDStatus ZipArchive::OpenOnly( const std::string  &url,
                                     bool                update,
                                     ResponseHandler    *handler,
                                     uint16_t            timeout )
  {
    OpenFlags::Flags flags = update ? OpenFlags::Update : OpenFlags::Read;
    Pipeline open_only = XrdCl::Open( archive, url, flags ) >>
                           [=]( XRootDStatus &st, StatInfo &info )
                           {
                             Log *log = DefaultEnv::GetLog();
                             // check the status is OK
                             if( st.IsOK() )
                             {
                               archsize  = info.GetSize();
                               openstage = NotParsed;
                               log->Debug( ZipMsg, "[0x%x] Opened (only) a ZIP archive (%s).",
                                           this, url.c_str() );
                             }
                             else
                             {
                               log->Error( ZipMsg, "[0x%x] Failed to open-only a ZIP archive (%s): %s",
                                           this, url.c_str(), st.ToString().c_str() );
                             }

                             if( handler )
                               handler->HandleResponse( make_status( st ), nullptr );
                           };

    Async( std::move( open_only ), timeout );
    return XRootDStatus();
  }

  //---------------------------------------------------------------------------
  // Open ZIP Archive (and parse the Central Directory)
  //---------------------------------------------------------------------------
  XRootDStatus ZipArchive::OpenArchive( const std::string  &url,
                                        OpenFlags::Flags    flags,
                                        ResponseHandler    *handler,
                                        uint16_t            timeout )
  {
    Log *log = DefaultEnv::GetLog();
    Fwd<uint32_t> rdsize; // number of bytes to be read
    Fwd<uint64_t> rdoff;  // offset for the read request
    Fwd<void*>    rdbuff; // buffer for data to be read
    uint32_t      maxrdsz = EOCD::maxCommentLength + EOCD::eocdBaseSize +
                            ZIP64_EOCDL::zip64EocdlSize;

    Pipeline open_archive = // open the archive
                            XrdCl::Open( archive, url, flags ) >>
                              [=]( XRootDStatus &status, StatInfo &info ) mutable
                              {
                                 // check the status is OK
                                 if( !status.IsOK() ) return;

                                 archsize = info.GetSize();
                                 // if it is an empty file (possibly a new file) there's nothing more to do
                                 if( archsize == 0 )
                                 {
                                   cdexists = false;
                                   openstage = Done;
                                   log->Dump( ZipMsg, "[0x%x] Opened a ZIP archive (file empty).", this );
                                   Pipeline::Stop();
                                 }
                                 // prepare the arguments for the subsequent read
                                 rdsize = ( archsize <= maxrdsz ? archsize : maxrdsz );
                                 rdoff  = archsize - *rdsize;
                                 buffer.reset( new char[*rdsize] );
                                 rdbuff = buffer.get();
                                 openstage = HaveEocdBlk;
                                 log->Dump( ZipMsg, "[0x%x] Opened a ZIP archive, reading "
                                                    "Central Directory at offset: %d.", this, *rdoff );
                               }
                            // read the Central Directory (in several stages if necessary)
                          | XrdCl::Read( archive, rdoff, rdsize, rdbuff ) >>
                              [=]( XRootDStatus &status, ChunkInfo &chunk ) mutable
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
                                      try{
                                      eocd.reset( new EOCD( eocdBlock, chunk.length - uint32_t(eocdBlock - buff) ) );
                                      log->Dump( ZipMsg, "[0x%x] EOCD record parsed: %s", this,
                                                         eocd->ToString().c_str() );
                                      if(eocd->cdOffset > archsize || eocd->cdOffset + eocd->cdSize > archsize)
                                    	  throw bad_data();
                                      }
                                      catch(const bad_data &ex){
                                    	  XRootDStatus error( stError, errDataError, 0,
                                    	               "End-of-central-directory signature corrupted." );
                                    	  Pipeline::Stop( error );
                                      }
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
                                      log->Dump( ZipMsg, "[0x%x] Reading additional data at offset: %d.",
                                                         this, *rdoff );
                                      Pipeline::Repeat(); break; // the break is really not needed ...
                                    }

                                    case HaveZip64EocdlBlk:
                                    {
                                      std::unique_ptr<ZIP64_EOCDL> eocdl( new ZIP64_EOCDL( buff ) );
                                      log->Dump( ZipMsg, "[0x%x] EOCDL record parsed: %s",
                                                         this, eocdl->ToString().c_str() );

                                      if( chunk.offset > eocdl->zip64EocdOffset )
                                      {
                                        // we need to read more data, adjust the read arguments
                                        rdsize = archsize - eocdl->zip64EocdOffset;
                                        rdoff  = eocdl->zip64EocdOffset;
                                        buffer.reset( new char[*rdsize] );
                                        rdbuff = buffer.get();
                                        openstage = HaveZip64EocdBlk;
                                        log->Dump( ZipMsg, "[0x%x] Reading additional data at offset: %d.",
                                                           this, *rdoff );
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
                                      log->Dump( ZipMsg, "[0x%x] ZIP64EOCD record parsed: %s",
                                                         this, zip64eocd->ToString().c_str() );

                                      // now we can read the CD records, adjust the read arguments
                                      cdoff     = zip64eocd->cdOffset;
                                      orgcdsz   = zip64eocd->cdSize;
                                      orgcdcnt  = zip64eocd->nbCdRec;
                                      rdoff     = zip64eocd->cdOffset;
                                      rdsize    = zip64eocd->cdSize;
                                      buffer.reset( new char[*rdsize] );
                                      rdbuff    = buffer.get();
                                      openstage = HaveCdRecords;
                                      log->Dump( ZipMsg, "[0x%x] Reading additional data at offset: %d.",
                                                         this, *rdoff );
                                      Pipeline::Repeat(); break; // the break is really not needed ...
                                    }

                                    case HaveCdRecords:
                                    {
                                      // make a copy of the original CDFH records
                                      orgcdbuf.reserve( orgcdsz );
                                      std::copy( buff, buff + orgcdsz, std::back_inserter( orgcdbuf ) );
                                      try
                                      {
                                        if( zip64eocd )
                                          std::tie( cdvec, cdmap ) = CDFH::Parse( buff, zip64eocd->cdSize, zip64eocd->nbCdRec );
                                        else
                                          std::tie( cdvec, cdmap ) = CDFH::Parse( buff, eocd->cdSize, eocd->nbCdRec );
                                        log->Dump( ZipMsg, "[0x%x] CD records parsed.", this );
                                        uint64_t sumCompSize = 0;
                                        for (auto it = cdvec.begin(); it != cdvec.end(); it++)
                                        {
                                          sumCompSize += (*it)->IsZIP64() ? (*it)->extra->compressedSize : (*it)->compressedSize;
                                          if ((*it)->offset > archsize || (*it)->offset + (*it)->compressedSize > archsize)
                                            throw bad_data();
                                        }
                                        if (sumCompSize > archsize)
                                          throw bad_data();
                                      }
                                      catch( const bad_data &ex )
                                      {
                                        XRootDStatus error( stError, errDataError, 0,
                                                                   "ZIP Central Directory corrupted." );
                                        Pipeline::Stop( error );
                                      }
                                      if( chunk.length != archsize ) buffer.reset();
                                      openstage = Done;
                                      cdexists  = true;
                                      break;
                                    }

                                    default: Pipeline::Stop( XRootDStatus( stError, errInvalidOp ) );
                                  }

                                  break;
                                }
                              }
                          | XrdCl::Final( [=]( const XRootDStatus &status )
                              { // finalize the pipeline by calling the user callback
                                if( status.IsOK() )
                                  log->Debug( ZipMsg, "[0x%x] Opened a ZIP archive (%s): %s",
                                              this, url.c_str(), status.ToString().c_str() );
                                else
                                  log->Error( ZipMsg, "[0x%x] Failed to open a ZIP archive (%s): %s",
                                              this, url.c_str(), status.ToString().c_str() );
                                if( handler )
                                  handler->HandleResponse( make_status( status ), nullptr );
                              } );

    Async( std::move( open_archive ), timeout );
    return XRootDStatus();
  }

  //---------------------------------------------------------------------------
  // Open a file within the ZIP Archive
  //---------------------------------------------------------------------------
  XRootDStatus ZipArchive::OpenFile( const std::string &fn,
                                     OpenFlags::Flags   flags,
                                     uint64_t           size,
                                     uint32_t           crc32 )
  {
    if( !openfn.empty() || openstage != Done || !archive.IsOpen() )
      return XRootDStatus( stError, errInvalidOp );

    Log  *log = DefaultEnv::GetLog();
    auto  itr   = cdmap.find( fn );
    if( itr == cdmap.end() )
    {
      // the file does not exist in the archive so it only makes sense
      // if our user is opening for append
      if( flags & OpenFlags::New )
      {
        openfn = fn;
        lfh.reset( new LFH( fn, crc32, size, time( 0 ) ) );
        log->Dump( ZipMsg, "[0x%x] File %s opened for append.",
                           this, fn.c_str() );
        return XRootDStatus();
      }
      log->Dump( ZipMsg, "[0x%x] Open failed: %s not in the ZIP archive.",
                         this, fn.c_str() );
      return XRootDStatus( stError, errNotFound );
    }

    // the file name exist in the archive but our user wants to append
    // a file with the same name
    if( flags & OpenFlags::New )
    {
      log->Dump( ZipMsg, "[0x%x] Open failed: file exists %s, cannot append.",
                         this, fn.c_str() );

      return XRootDStatus( stError, errInvalidOp, EEXIST, "The file already exists in the ZIP archive." );
    }

    openfn = fn;
    log->Dump( ZipMsg, "[0x%x] File %s opened for reading.",
                       this, fn.c_str() );
    return XRootDStatus();
  }

  //---------------------------------------------------------------------------
  // Get a buffer with central directory of the ZIP archive
  //---------------------------------------------------------------------------
  buffer_t ZipArchive::GetCD()
  {
    uint32_t size = 0;
    uint32_t cdsize  = CDFH::CalcSize( cdvec, orgcdsz, orgcdcnt );
    // first create the EOCD record
    eocd.reset( new EOCD( cdoff, cdvec.size(), cdsize ) );
    size += eocd->eocdSize ;
    size += eocd->cdSize;
    // then create zip64eocd & zip64eocdl if necessary
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

  //---------------------------------------------------------------------------
  // Set central directory for the ZIP archive
  //---------------------------------------------------------------------------
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

  //---------------------------------------------------------------------------
  // Create the central directory at the end of ZIP archive and close it
  //---------------------------------------------------------------------------
  XRootDStatus ZipArchive::CloseArchive( ResponseHandler *handler,
                                         uint16_t         timeout )
  {
    Log *log = DefaultEnv::GetLog();

    //-------------------------------------------------------------------------
    // If the file was updated, we need to write the Central Directory before
    // closing the file.
    //-------------------------------------------------------------------------
    if( updated )
    {
      ChunkList chunks;
      std::vector<std::shared_ptr<buffer_t>> wrtbufs;
      for( auto &p : newfiles )
      {
        NewFile &nf = p.second;
        if( !nf.overwrt ) continue;
        uint32_t lfhlen = lfh->lfhSize;
        auto lfhbuf = std::make_shared<buffer_t>();
        lfhbuf->reserve( lfhlen );
        nf.lfh->Serialize( *lfhbuf );
        chunks.emplace_back( nf.offset, lfhbuf->size(), lfhbuf->data() );
        wrtbufs.emplace_back( std::move( lfhbuf ) );
      }

      auto wrtbuff = std::make_shared<buffer_t>( GetCD() );
      Pipeline p = XrdCl::Write( archive, cdoff,
                                 wrtbuff->size(),
                                 wrtbuff->data() );
      wrtbufs.emplace_back( std::move( wrtbuff ) );

      std::vector<ChunkList> listsvec;
      XrdCl::Utils::SplitChunks( listsvec, chunks, 262144, 1024 );

      for(auto itr = listsvec.rbegin(); itr != listsvec.rend(); ++itr)
      {
        p = XrdCl::VectorWrite( archive, *itr ) | p;
      }
      if( ckpinit )
        p       |= XrdCl::Checkpoint( archive, ChkPtCode::COMMIT );
      p         |= Close( archive ) >>
                     [=]( XRootDStatus &st )
                     {
                       if( st.IsOK() ) Clear();
                       else openstage = Error;
                     }
                 | XrdCl::Final( [=]( const XRootDStatus &st ) mutable
                     {
                       if( st.IsOK() )
                         log->Dump( ZipMsg, "[0x%x] Successfully closed ZIP archive "
                                            "(CD written).", this );
                       else
                         log->Error( ZipMsg, "[0x%x] Failed to close ZIP archive: %s",
                                             this, st.ToString().c_str() );
                       wrtbufs.clear();
                       if( handler ) handler->HandleResponse( make_status( st ), nullptr );
                     } );

      Async( std::move( p ), timeout );
      return XRootDStatus();
    }

    //-------------------------------------------------------------------------
    // Otherwise, just close the ZIP archive
    //-------------------------------------------------------------------------
    Pipeline p = Close( archive ) >>
                   [=]( XRootDStatus &st )
                   {
                     if( st.IsOK() )
                     {
                       Clear();
                       log->Dump( ZipMsg, "[0x%x] Successfully closed "
                                          "ZIP archive.", this );
                     }
                     else
                     {
                       openstage = Error;
                       log->Error( ZipMsg, "[0x%x] Failed to close ZIP archive:"
                                           " %s", this, st.ToString().c_str() );
                     }
                     if( handler )
                       handler->HandleResponse( make_status( st ), nullptr );
                   };
    Async( std::move( p ), timeout );
    return XRootDStatus();
  }

  //---------------------------------------------------------------------------
  // Read data from a given file
  //---------------------------------------------------------------------------
  XRootDStatus ZipArchive::ReadFrom( const std::string &fn,
                                     uint64_t           offset,
                                     uint32_t           size,
                                     void              *buffer,
                                     ResponseHandler   *handler,
                                     uint16_t           timeout )
  {
    return ReadFromImpl<ChunkInfo>( *this, fn, offset, size, buffer, handler, timeout );
  }

  //---------------------------------------------------------------------------
  // PgRead data from a given file
  //---------------------------------------------------------------------------
  XRootDStatus ZipArchive::PgReadFrom( const std::string &fn,
                                       uint64_t           offset,
                                       uint32_t           size,
                                       void              *buffer,
                                       ResponseHandler   *handler,
                                       uint16_t           timeout )
  {
    return ReadFromImpl<PageInfo>( *this, fn, offset, size, buffer, handler, timeout );
  }

  //---------------------------------------------------------------------------
  // List files in the ZIP archive
  //---------------------------------------------------------------------------
  XRootDStatus ZipArchive::List( DirectoryList *&list )
  {
    if( openstage != Done )
      return XRootDStatus( stError, errInvalidOp,
                           0, "Archive not opened." );

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
      uint64_t uncompressedSize = cdfh->uncompressedSize;
      if( uncompressedSize == std::numeric_limits<uint32_t>::max() && cdfh->extra )
        uncompressedSize = cdfh->extra->uncompressedSize;
      StatInfo *entry_info = make_stat( *info, uncompressedSize );
      DirectoryList::ListEntry *entry =
          new DirectoryList::ListEntry( url.GetHostId(), cdfh->filename, entry_info );
      list->Add( entry );
    }

    return XRootDStatus();
  }

  //-----------------------------------------------------------------------
  // Append data to a new file, implementation
  //-----------------------------------------------------------------------
  XRootDStatus ZipArchive::WriteImpl( uint32_t               size,
                                      const void            *buffer,
                                      ResponseHandler       *handler,
                                      uint16_t               timeout )
  {
    Log *log = DefaultEnv::GetLog();
    std::vector<iovec> iov( 2 );

    //-------------------------------------------------------------------------
    // If there is a LFH we need to write it first ahead of the write-buffer
    // itself.
    //-------------------------------------------------------------------------
    std::shared_ptr<buffer_t> lfhbuf;
    if( lfh )
    {
      uint32_t lfhlen = lfh->lfhSize;
      lfhbuf = std::make_shared<buffer_t>();
      lfhbuf->reserve( lfhlen );
      lfh->Serialize( *lfhbuf );
      iov[0].iov_base = lfhbuf->data();
      iov[0].iov_len  = lfhlen;
      log->Dump( ZipMsg, "[0x%x] Will write LFH.", this );
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

    Pipeline p;
    auto wrthandler = [=]( const XRootDStatus &st ) mutable
                      {
                        if( st.IsOK() ) updated = true;
                        lfhbuf.reset();
                        if( handler )
                          handler->HandleResponse( make_status( st ), nullptr );
                      };

    //-------------------------------------------------------------------------
    // If we are overwriting an existing CD we need to use checkpointed version
    // of WriteV.
    //-------------------------------------------------------------------------
    if( archsize > cdoff )
      p = XrdCl::ChkptWrtV( archive, wrtoff, iov ) | XrdCl::Final( wrthandler );
    //-------------------------------------------------------------------------
    // Otherwise use the ordinary WriteV.
    //-------------------------------------------------------------------------
    else
      p = XrdCl::WriteV( archive, wrtoff, iov ) | XrdCl::Final( wrthandler );
    //-----------------------------------------------------------------------
    // If needed make sure the checkpoint is initialized
    //-----------------------------------------------------------------------
    if( archsize > cdoff && !ckpinit )
    {
      p = XrdCl::Checkpoint( archive, ChkPtCode::BEGIN ) | p;
      ckpinit = true;
    }

    archsize += wrtlen;
    cdoff    += wrtlen;

    //-------------------------------------------------------------------------
    // If we have written the LFH, add respective CDFH record
    //-------------------------------------------------------------------------
    if( lfh )
    {
      mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
      cdvec.emplace_back( new CDFH( lfh.get(), mode, wrtoff ) );
      cdmap[openfn] = cdvec.size() - 1;
      // make sure we keep track of all appended files
      newfiles.emplace( std::piecewise_construct,
                        std::forward_as_tuple( lfh->filename ),
                        std::forward_as_tuple( wrtoff, std::move( lfh ) )
                      );
    }
    Async( std::move( p ), timeout );
    return XRootDStatus();
  }

  //-----------------------------------------------------------------------
  // Update the metadata of the currently open file
  //-----------------------------------------------------------------------
  XRootDStatus ZipArchive::UpdateMetadata( uint32_t crc32 )
  {
    if( openstage != Done || openfn.empty() )
      return XRootDStatus( stError, errInvalidOp, 0, "Archive not opened." );

    //---------------------------------------------------------------------
    // Firstly, update the crc32 in the central directory
    //---------------------------------------------------------------------
    auto itr = cdmap.find( openfn );
    if( itr == cdmap.end() )
      return XRootDStatus( stError, errInvalidOp );
    cdvec[itr->second]->ZCRC32 = crc32;

    //---------------------------------------------------------------------
    // Secondly, update the crc32 in the LFH and mark it as needing
    // overwriting
    //---------------------------------------------------------------------
    auto itr2 = newfiles.find( openfn );
    if( itr2 == newfiles.end() )
      return XRootDStatus( stError, errInvalidOp );
    itr2->second.lfh->ZCRC32 = crc32;

    return XRootDStatus();
  }

  //-----------------------------------------------------------------------
  // Create a new file in the ZIP archive and append the data
  //-----------------------------------------------------------------------
  XRootDStatus ZipArchive::AppendFile( const std::string &fn,
                                       uint32_t           crc32,
                                       uint32_t           size,
                                       const void        *buffer,
                                       ResponseHandler   *handler,
                                       uint16_t           timeout )
  {
    Log  *log = DefaultEnv::GetLog();
    auto  itr   = cdmap.find( fn );
    // check if the file already exists in the archive
    if( itr != cdmap.end() )
    {
      log->Dump( ZipMsg, "[0x%x] Open failed: file exists %s, cannot append.",
                         this, fn.c_str() );
      return XRootDStatus( stError, errInvalidOp );
    }

    log->Dump( ZipMsg, "[0x%x] Appending file: %s.", this, fn.c_str() );
    //-------------------------------------------------------------------------
    // Create Local File Header record
    //-------------------------------------------------------------------------
    lfh.reset( new LFH( fn, crc32, size, time( 0 ) ) );
    //-------------------------------------------------------------------------
    // And write it all
    //-------------------------------------------------------------------------
    return WriteImpl( size, buffer, handler, timeout );
  }

} /* namespace XrdZip */
