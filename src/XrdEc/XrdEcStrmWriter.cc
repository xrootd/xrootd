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

#include "XrdEc/XrdEcStrmWriter.hh"
#include "XrdEc/XrdEcThreadPool.hh"

#include "XrdOuc/XrdOucCRC32C.hh"

#include "XrdZip/XrdZipLFH.hh"
#include "XrdZip/XrdZipCDFH.hh"
#include "XrdZip/XrdZipEOCD.hh"
#include "XrdZip/XrdZipUtils.hh"

#include <numeric>
#include <algorithm>
#include <future>

namespace XrdEc
{
  //---------------------------------------------------------------------------
  // Open the data object for writting
  //---------------------------------------------------------------------------
  void StrmWriter::Open( XrdCl::ResponseHandler *handler, uint16_t timeout )
  {
    const size_t size = objcfg.plgr.size();

    std::vector<XrdCl::Pipeline> opens;
    opens.reserve( size );
    // initialize all zip archive objects
    for( size_t i = 0; i < size; ++i )
      dataarchs.emplace_back( std::make_shared<XrdCl::ZipArchive>(
          Config::Instance().enable_plugins ) );

    for( size_t i = 0; i < size; ++i )
    {
      std::string url = objcfg.GetDataUrl( i );
      XrdCl::Ctx<XrdCl::ZipArchive> zip( *dataarchs[i] );
      opens.emplace_back( XrdCl::OpenArchive( zip, url, XrdCl::OpenFlags::New | XrdCl::OpenFlags::Write ) );
    }

    XrdCl::Async( XrdCl::Parallel( opens ).AtLeast( objcfg.nbchunks ) >>
                  [=]( XrdCl::XRootDStatus &st )
                  {
                    if( !st.IsOK() ) global_status.report_open( st );
                    handler->HandleResponse( new XrdCl::XRootDStatus( st ), nullptr );
                  }, timeout );
  }

  //---------------------------------------------------------------------------
  // Write data to the data object
  //---------------------------------------------------------------------------
  void StrmWriter::Write( uint32_t size, const void *buff, XrdCl::ResponseHandler *handler )
  {
    //-------------------------------------------------------------------------
    // First, check the global status, if we are in an error state just
    // fail the request.
    //-------------------------------------------------------------------------
    XrdCl::XRootDStatus gst = global_status.get();
    if( !gst.IsOK() ) return ScheduleHandler( handler, gst );

    //-------------------------------------------------------------------------
    // Update the number of bytes left to be written
    //-------------------------------------------------------------------------
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

    //-------------------------------------------------------------------------
    // We can tell the user it's done as we have the date cached in the
    // buffer
    //-------------------------------------------------------------------------
    ScheduleHandler( handler );
  }

  //---------------------------------------------------------------------------
  // Close the data object
  //---------------------------------------------------------------------------
  void StrmWriter::Close( XrdCl::ResponseHandler *handler, uint16_t timeout )
  {
    //-------------------------------------------------------------------------
    // First, check the global status, if we are in an error state just
    // fail the request.
    //-------------------------------------------------------------------------
    XrdCl::XRootDStatus gst = global_status.get();
    if( !gst.IsOK() ) return ScheduleHandler( handler, gst );
    //-------------------------------------------------------------------------
    // Take care of the left-over data ...
    //-------------------------------------------------------------------------
    if( wrtbuff && !wrtbuff->Empty() ) EnqueueBuff( std::move( wrtbuff ) );
    //-------------------------------------------------------------------------
    // Let the global status handle the close
    //-------------------------------------------------------------------------
    global_status.issue_close( handler, timeout );
  }

  //---------------------------------------------------------------------------
  // Issue the write requests for the given write buffer
  //---------------------------------------------------------------------------
  void StrmWriter::WriteBuff( std::unique_ptr<WrtBuff> buff )
  {
    //-------------------------------------------------------------------------
    // Our buffer with the data block, will be shared between all pipelines
    // writing to different servers.
    //-------------------------------------------------------------------------
    std::shared_ptr<WrtBuff> wrtbuff( std::move( buff ) );

    //-------------------------------------------------------------------------
    // Shuffle the servers so every block has a different placement
    //-------------------------------------------------------------------------
    static std::default_random_engine random_engine( std::chrono::system_clock::now().time_since_epoch().count() );
    std::shared_ptr<sync_queue<size_t>> servers = std::make_shared<sync_queue<size_t>>();
    std::vector<size_t> zipid( dataarchs.size() );
    std::iota( zipid.begin(), zipid.end(), 0 );
    std::shuffle( zipid.begin(), zipid.end(), random_engine );
    auto itr = zipid.begin();
    for( ; itr != zipid.end() ; ++itr ) servers->enqueue( std::move( *itr ) );

    //-------------------------------------------------------------------------
    // Create the write pipelines for updating stripes
    //-------------------------------------------------------------------------
    const size_t nbchunks = objcfg.nbchunks;
    std::vector<XrdCl::Pipeline> writes;
    writes.reserve( nbchunks );
    size_t   blknb = next_blknb++;
    uint64_t blksize = 0;
    for( size_t strpnb = 0; strpnb < nbchunks; ++strpnb )
    {
      std::string fn       = objcfg.GetFileName( blknb, strpnb );
      uint32_t    crc32c   = wrtbuff->GetCrc32c( strpnb );
      uint64_t    strpsize = wrtbuff->GetStrpSize( strpnb );
      char*       strpbuff = wrtbuff->GetStrpBuff( strpnb );
      if( strpnb < objcfg.nbdata ) blksize += strpsize;

      //-----------------------------------------------------------------------
      // Find a server where we can append the next data chunk
      //-----------------------------------------------------------------------
      XrdCl::Ctx<XrdCl::ZipArchive> zip;
      size_t srvid;
      if( !servers->dequeue( srvid ) )
      {
        XrdCl::XRootDStatus err( XrdCl::stError, XrdCl::errNoMoreReplicas,
                                 0, "No more data servers to try." );
        //---------------------------------------------------------------------
        // calculate the full block size, otherwise the user handler
        // will be never called
        //---------------------------------------------------------------------
        for( size_t i = strpnb + 1; i < objcfg.nbdata; ++i )
          blksize += wrtbuff->GetStrpSize( i );
        global_status.report_wrt( err, blksize );
        return;
      }
      zip = *dataarchs[srvid];

      //-----------------------------------------------------------------------
      // Create the Write request
      //-----------------------------------------------------------------------
      XrdCl::Pipeline p = XrdCl::AppendFile( zip, fn, crc32c, strpsize, strpbuff ) >>
                           [=]( XrdCl::XRootDStatus &st ) mutable
                           {
                             //------------------------------------------------
                             // Try to recover from error
                             //------------------------------------------------
                             if( !st.IsOK() )
                             {
                               //----------------------------------------------
                               // Select another server
                               //----------------------------------------------
                               if( !servers->dequeue( srvid ) ) return; // if there are no more servers we simply fail
                               zip = *dataarchs[srvid];
                               //----------------------------------------------
                               // Retry this operation at different server
                               //----------------------------------------------
                               XrdCl::Pipeline::Repeat();
                             }
                             //------------------------------------------------
                             // Make sure the buffer is only deallocated
                             // after the handler is called
                             //------------------------------------------------
                             wrtbuff.reset();
                           };
      writes.emplace_back( std::move( p ) );
    }

    XrdCl::WaitFor( XrdCl::Parallel( writes ) >> [=]( XrdCl::XRootDStatus &st ){ global_status.report_wrt( st, blksize ); } );
  }

  //---------------------------------------------------------------------------
  // Get a buffer with metadata (CDFH and EOCD records)
  //---------------------------------------------------------------------------
  XrdZip::buffer_t StrmWriter::GetMetadataBuffer()
  {
    using namespace XrdZip;

    const size_t cdcnt = objcfg.plgr.size();
    std::vector<buffer_t> buffs; buffs.reserve( cdcnt ); // buffers with raw data
    std::vector<LFH> lfhs; lfhs.reserve( cdcnt );        // LFH records
    std::vector<CDFH> cdfhs; cdfhs.reserve( cdcnt );     // CDFH records

    //-------------------------------------------------------------------------
    // prepare data structures (LFH and CDFH records)
    //-------------------------------------------------------------------------
    uint64_t offset = 0;
    uint64_t cdsize = 0;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    for( size_t i = 0; i < cdcnt; ++i )
    {
      std::string fn = std::to_string( i );                          // file name (URL of the data archive)
      buffer_t buff( dataarchs[i]->GetCD() );                        // raw data buffer (central directory of the data archive)
      uint32_t cksum = objcfg.digest( 0, buff.data(), buff.size() ); // digest (crc) of the buffer
      lfhs.emplace_back( fn, cksum, buff.size(), time( 0 ) );        // LFH record for the buffer
      LFH &lfh = lfhs.back();
      cdfhs.emplace_back( &lfh, mode, offset );                      // CDFH record for the buffer
      offset += LFH::lfhBaseSize + fn.size() + buff.size();          // shift the offset
      cdsize += cdfhs.back().cdfhSize;                               // update central directory size
      buffs.emplace_back( std::move( buff ) );                       // keep the buffer for later
    }

    uint64_t zipsize = offset + cdsize + EOCD::eocdBaseSize;
    buffer_t zipbuff; zipbuff.reserve( zipsize );

    //-------------------------------------------------------------------------
    // write into the final buffer LFH records + raw data
    //-------------------------------------------------------------------------
    for( size_t i = 0; i < cdcnt; ++i )
    {
      lfhs[i].Serialize( zipbuff );
      std::copy( buffs[i].begin(), buffs[i].end(), std::back_inserter( zipbuff ) );
    }
    //-------------------------------------------------------------------------
    // write into the final buffer CDFH records
    //-------------------------------------------------------------------------
    for( size_t i = 0; i < cdcnt; ++i )
      cdfhs[i].Serialize( zipbuff );
    //-------------------------------------------------------------------------
    // prepare and write into the final buffer the EOCD record
    //-------------------------------------------------------------------------
    EOCD eocd( offset, cdcnt, cdsize );
    eocd.Serialize( zipbuff );

    return zipbuff;
  }

  //---------------------------------------------------------------------------
  // Close the data object (implementation)
  //---------------------------------------------------------------------------
  void StrmWriter::CloseImpl( XrdCl::ResponseHandler *handler, uint16_t timeout )
  {
    //-------------------------------------------------------------------------
    // First, check the global status, if we are in an error state just
    // fail the request.
    //-------------------------------------------------------------------------
    XrdCl::XRootDStatus gst = global_status.get();
    if( !gst.IsOK() ) return ScheduleHandler( handler, gst );

    const size_t size = objcfg.plgr.size();
    //-------------------------------------------------------------------------
    // prepare the metadata (the Central Directory of each data ZIP)
    //-------------------------------------------------------------------------
    auto zipbuff = objcfg.nomtfile ? nullptr :
                   std::make_shared<XrdZip::buffer_t>( GetMetadataBuffer() );
    //-------------------------------------------------------------------------
    // prepare the pipelines ...
    //-------------------------------------------------------------------------
    std::vector<XrdCl::Pipeline> closes;
    std::vector<XrdCl::Pipeline> save_metadata;
    closes.reserve( size );
    std::string closeTime = std::to_string( time(NULL) );

    std::vector<XrdCl::xattr_t> xav{ {"xrdec.filesize", std::to_string(GetSize())},
                                     {"xrdec.strpver", closeTime.c_str()} };

    for( size_t i = 0; i < size; ++i )
    {
      //-----------------------------------------------------------------------
      // close ZIP archives with data
      //-----------------------------------------------------------------------
      if( dataarchs[i]->IsOpen() )
      {
        XrdCl::Pipeline p = XrdCl::SetXAttr( dataarchs[i]->GetFile(), xav )
                          | XrdCl::CloseArchive( *dataarchs[i] );
        closes.emplace_back( std::move( p ) );
      }
      //-----------------------------------------------------------------------
      // replicate the metadata
      //-----------------------------------------------------------------------
      if( zipbuff )
      {
        std::string url = objcfg.GetMetadataUrl( i );
        metadataarchs.emplace_back( std::make_shared<XrdCl::File>(
            Config::Instance().enable_plugins ) );
        XrdCl::Pipeline p = XrdCl::Open( *metadataarchs[i], url, XrdCl::OpenFlags::New | XrdCl::OpenFlags::Write )
                          | XrdCl::Write( *metadataarchs[i], 0, zipbuff->size(), zipbuff->data() )
                          | XrdCl::Close( *metadataarchs[i] )
                          | XrdCl::Final( [zipbuff]( const XrdCl::XRootDStatus& ){ } );

        save_metadata.emplace_back( std::move( p ) );
      }
    }

    //-------------------------------------------------------------------------
    // If we were instructed not to create the the additional metadata file
    // do the simplified close
    //-------------------------------------------------------------------------
    if( save_metadata.empty() )
    {
      XrdCl::Pipeline p = XrdCl::Parallel( closes ).AtLeast( objcfg.nbchunks ) >> handler;
      XrdCl::Async( std::move( p ), timeout );
      return;
    }

    //-------------------------------------------------------------------------
    // compose closes & save_metadata:
    //  - closes must be successful at least for #data + #parity
    //  - save_metadata must be successful at least for #parity + 1
    //-------------------------------------------------------------------------
    XrdCl::Pipeline p = XrdCl::Parallel(
        XrdCl::Parallel( closes ).AtLeast( objcfg.nbchunks ),
        XrdCl::Parallel( save_metadata ).AtLeast( objcfg.nbparity + 1 )
      ) >> handler;
    XrdCl::Async( std::move( p ), timeout );
  }
}
