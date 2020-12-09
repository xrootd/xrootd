/*
 * XrdEcDataObject.cc
 *
 *  Created on: Jan 23, 2020
 *      Author: simonm
 */

#include "XrdEc/XrdEcDataObject.hh"
#include "XrdEc/XrdEcThreadPool.hh"

#include "XrdCl/XrdClFileOperations.hh"
#include "XrdCl/XrdClParallelOperation.hh"


namespace XrdEc
{
//  void DataObject::OpenForWrite( XrdCl::ResponseHandler *handler )
//  {
//    const size_t size = objcfg->plgr.size();
//
//    std::vector<XrdCl::Pipeline> opens;
//    opens.reserve( size );
//    files.resize( size );
//
//    for( size_t i = 0; i < size; ++i )
//    {
//      std::string url = objcfg->plgr[i] + objcfg->obj + ".data.zip";
//      opens.emplace_back( XrdCl::Open( files[i], url, XrdCl::OpenFlags::New | XrdCl::OpenFlags::Write ) );
//    }
//
//    XrdCl::Async( XrdCl::Parallel( opens ) >> handler ); // TODO Not all need to succeed !!!
//  }

//  void DataObject::OpenForRead( XrdCl::ResponseHandler *handler )
//  {
//    // open and read the metadata, we can read the metadata just
//    std::string url_meta = objcfg->plgr[0] + objcfg->obj + ".metadata.zip";
//    auto file = std::make_shared<XrdCl::File>();
//    XrdCl::Fwd<uint32_t> length;
//    XrdCl::Fwd<void*>    buff;
//
//    XrdCl::Pipeline get_meta = XrdCl::Open( *file, url_meta, XrdCl::OpenFlags::Read ) >>
//                                 [length, buff]( XrdCl::XRootDStatus &st, XrdCl::StatInfo &info )
//                                 {
//                                   if( !st.IsOK() ) return;
//                                   length = info.GetSize();
//                                   buff = new char[info.GetSize()];
//                                 }
//                             | XrdCl::Read( *file, 0, length, buff ) >>
//                                 [this]( XrdCl::XRootDStatus &st, XrdCl::ChunkInfo &chunk )
//                                 {
//                                   if( !st.IsOK() ) return;
//                                   this->ParseMetadata( chunk.buffer, chunk.length );
//                                 }
//                             | XrdCl::Close( *file ) >> [file]( XrdCl::XRootDStatus &st ){ };
//
//    // open the data files
//    const size_t size = objcfg->plgr.size();
//    std::vector<XrdCl::Pipeline> opens;
//    opens.reserve( size );
//    files.resize( size );
//
//    for( size_t i = 0; i < size; ++i )
//    {
//     std::string url = objcfg->plgr[i] + objcfg->obj + ".data.zip";
//     opens.emplace_back( XrdCl::Open( files[i], url, XrdCl::OpenFlags::Read ) );
//    }
//
//    XrdCl::Async( XrdCl::Parallel( XrdCl::Parallel( opens ), std::move( get_meta ) ) >> handler );
//  }

//  void DataObject::ParseMetadata( const void *buffer, uint32_t length )
//  {
//    const char* buff = reinterpret_cast<const char*>( buffer );
//
//    uint32_t eocd_off = length - EOCD::size;
//    eocd_record eocd( buff + eocd_off );
//
//    for( size_t i = 0; i < eocd.nb_entries; ++i )
//    {
//      // Local File Header
//      lfh_record lfh( buff );
//      buff += LFH::size;
//
//      // File Name
//      std::string url( buff, lfh.fnlen );
//      buff += lfh.fnlen;
//
//      // Parse the Central Directory for given URL
//      int64_t cd_size = lfh.uncomp_size;
//      // TODO verify checksum for the data block !!!
//
//      while( cd_size > 0 )
//      {
//        // Central Directory File Header
//        cdh_record cdh( buff );
//        buff    += CDH::size;
//        cd_size -= CDH::size;
//
//        // File Name
//        std::string chunk = std::string( buff, cdh.fnlen );
//        buff    += cdh.fnlen;
//        cd_size -=  cdh.fnlen;
//
//        // Block ID
//        size_t chpos = chunk.rfind( '.' );
//        if( chpos == std::string::npos ) throw std::exception(); // TODO
//        uint64_t chid = std::stoull( chunk.substr( chpos + 1 ) );
//
//        // Chunk ID
//        size_t blkpos = chunk.rfind( '.', chpos - 1 );
//        if( blkpos == std::string::npos ) throw std::exception(); // TODO
//        uint64_t blkid = std::stoull( chunk.substr( blkpos + 1, chpos - blkpos - 1 ) );
//
//        // Add respective entry to the metadata
//        if( metadata[blkid].empty() ) metadata[blkid].resize( objcfg->nbchunks );
//        metadata[blkid][chid] = std::make_tuple( url, cdh.lfh_offset, cdh.uncomp_size );
//      }
//    }
//  }

//  void DataObject::Write( uint64_t offset, uint32_t size, const void *buffer, XrdCl::ResponseHandler *handler )
//  {
//    operation->Write( offset, size, buffer, handler );
//
//    const char* buff = reinterpret_cast<const char*>( buffer );
//
//    uint32_t wrtsize = size;
//    while( wrtsize > 0 )
//    {
//      if( !wrtbuff ) wrtbuff.reset( new WrtBuff( *objcfg, offset ) );
//      uint64_t written = wrtbuff->Write( offset, wrtsize, buff, handler );
//      offset  += written;
//      buff    += written;
//      wrtsize -= written;
//      if( wrtbuff->Complete() ) WriteBlock();
//    }
//
//  }

//  void DataObject::Read( uint64_t offset, uint32_t size, void *buffer, XrdCl::ResponseHandler *handler )
//  {
//    char *buff = reinterpret_cast<char*>( buffer );
//    std::vector<XrdCl::Pipeline> reads;
//
//    while( size > 0 )
//    {
//      uint32_t blkid     = offset / objcfg->datasize;
//      uint32_t blkreloff = offset - blkid * objcfg->datasize;
//      uint32_t chid      = blkreloff / objcfg->chunksize;
//      uint64_t chreloff  = blkreloff - chid * objcfg->chunksize;
//
//      std::string zip_url;
//      uint32_t    zip_off;
//      uint32_t    zip_size;
//      std::tie( zip_url, zip_off, zip_size ) = metadata[blkid][chid];
//
//      uint32_t chrdsize = size;
//      if( chrdsize > zip_size - chreloff )
//        chrdsize = zip_size - chreloff;
//
//      std::string chname = objcfg->obj + '.' + std::to_string( blkid ) + '.' + std::to_string( chid );
//      uint32_t chrdoff = zip_off + LFH::size + chname.size() + chreloff;
//
//      reads.emplace_back( XrdCl::Read( ToFile( zip_url ), chrdoff, chrdsize, buff ) );
//
//      buff   += chrdsize;
//      offset += chrdsize;
//      size   -= chrdsize;
//    }
//
//    XrdCl::Async( XrdCl::Parallel( reads ) >> handler ); // TODO create proper read response
//  }

//  void DataObject::WriteBlock()
//  {
//    wrtbuff->Encode();  // TODO this should happen in a separate thread !!!
//
//    std::vector<std::future<uint32_t>> checksums;
//    checksums.reserve( objcfg->nbchunks );
//
//    for( uint8_t strpnb = 0; strpnb < objcfg->nbchunks; ++strpnb )
//    {
////      char *buff = wrtbuff->GetChunk( strpnb );
//      std::future<uint32_t> ftr = ThreadPool::Instance().Execute( crc32c, 0, wrtbuff->GetChunk( strpnb ), objcfg->chunksize );
//      checksums.emplace_back( std::move( ftr ) );
//    }
//
//    const size_t size = objcfg->nbchunks;
//    std::vector<XrdCl::Pipeline> writes;
//    writes.reserve( size );
//
//    for( size_t i = 0; i < size; ++i ) // TODO select randomly !!!
//    {
//      std::string fn = objcfg->obj + '.' + std::to_string( wrtbuff->GetBlkNb() ) + '.' + std::to_string( i );
//      uint32_t checksum = checksums[i].get();
//      std::shared_ptr<WrtCtx> wrtctx( new WrtCtx( objcfg->chunksize, checksum, fn, wrtbuff->GetChunk( i ), wrtbuff->GetStrpSize( i ) ) );
//      uint32_t offset = offsets[i].fetch_add( wrtctx->total_size );
//      writes.emplace_back( XrdCl::WriteV( files[i], offset, wrtctx->iov, wrtctx->iovcnt ) >> [wrtctx]( XrdCl::XRootDStatus& ){ } ); // TODO fallback to spare if fails !!!
//      // create respective CDH record
//      dirs[i].Add( fn, objcfg->chunksize, checksum, offset ); // TODO this needs to be thread safe !!!
//    }
//
//    std::shared_ptr<WrtBuff> pend_buff = std::move( wrtbuff );
//    pending_wrts.emplace( XrdCl::Async( XrdCl::Parallel( writes ) >> [pend_buff]( XrdCl::XRootDStatus& ){ } ) ); // TODO this needs to be thread safe !!!
//  }

//  void DataObject::CloseAfterWrite( XrdCl::ResponseHandler *handler )
//  {
//    const size_t size = objcfg->plgr.size();
//    std::vector<XrdCl::Pipeline> closes;
//    closes.reserve( size );
//
//    for( size_t i = 0; i < size; ++i )
//    {
//      dirs[i].CreateEOCD();
//      if( !dirs[i].Empty() )
//      {
//        uint32_t offset = offsets[i];
//        closes.emplace_back( XrdCl::Write( files[i], offset, dirs[i].cd_buffer.GetCursor(), dirs[i].cd_buffer.GetBuffer() ) | XrdCl::Close( files[i] ) );
//      }
//    }
//
//    // now create the metadata files
//    auto metactx = std::make_shared<MetaDataCtx>( objcfg.get(), dirs );
//    std::vector<XrdCl::Pipeline> put_metadata;
//
//    for( size_t i = 0; i < size; ++i )
//    {
//      auto file = std::make_shared<XrdCl::File>();
//      std::string url = objcfg->plgr[i] + objcfg->obj + ".metadata.zip";
//
//      XrdCl::Pipeline put = XrdCl::Open( *file, url, XrdCl::OpenFlags::New | XrdCl::OpenFlags::Write )
//                          | XrdCl::WriteV( *file, 0, metactx->iov, metactx->iovcnt ) >> [metactx]( XrdCl::XRootDStatus& ){ }
//                          | XrdCl::Close( *file ) >> [file]( XrdCl::XRootDStatus& ){ };
//
//      put_metadata.emplace_back( std::move( put ) );
//    }
//
//    XrdCl::Async( XrdCl::Parallel( XrdCl::Parallel( closes ), XrdCl::Parallel( put_metadata ) ) >> handler );
//  }

//  void DataObject::CloseAfterRead( XrdCl::ResponseHandler *handler )
//  {
//    metadata.clear();
//
//    const size_t size = objcfg->plgr.size();
//    std::vector<XrdCl::Pipeline> closes;
//    closes.reserve( size );
//
//    for( size_t i = 0; i < size; ++i )
//      closes.emplace_back( XrdCl::Close( files[i] ) );
//
//    XrdCl::Async( XrdCl::Parallel( closes ) >> handler );
//  }

} /* namespace XrdEc */
