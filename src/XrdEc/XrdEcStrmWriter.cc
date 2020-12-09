/*
 * XrdEcStrmWriter.cc
 *
 *  Created on: 5 May 2020
 *      Author: simonm
 */

#include "XrdEc/XrdEcStrmWriter.hh"
#include "XrdEc/XrdEcThreadPool.hh"

#include "XrdOuc/XrdOucCRC32C.hh"

#include <numeric>
#include <algorithm>

namespace
{

}

namespace XrdEc
{
  void StrmWriter::WriteBlock()
  {
    wrtbuff->Encode();

    std::vector<std::future<uint32_t>> checksums;
    checksums.reserve( objcfg->nbchunks );

    for( uint8_t strpnb = 0; strpnb < objcfg->nbchunks; ++strpnb )
    {
      std::future<uint32_t> ftr = ThreadPool::Instance().Execute( crc32c, 0, wrtbuff->GetChunk( strpnb ), objcfg->chunksize );
      checksums.emplace_back( std::move( ftr ) );
    }

    const size_t size = objcfg->nbchunks;
    std::vector<XrdCl::Pipeline> writes;
    writes.reserve( size );

    std::vector<size_t> fileid( files->size() );
    std::iota( fileid.begin(), fileid.end(), 0 );
    std::shuffle( fileid.begin(), fileid.end(), random_engine );

    std::vector<size_t> spareid;
    auto itr = fileid.begin() + objcfg->nbchunks;
    for( ; itr != fileid.end() ; ++itr )
      spareid.emplace_back( *itr );
    std::shared_ptr<spare_files> spares = std::make_shared<spare_files>();
    spares->spareid.swap( spareid );

    for( size_t i = 0; i < size; ++i )
    {
      std::shared_ptr<size_t> fid = std::make_shared( fileid[i] );
      std::string fn = objcfg->obj + '.' + std::to_string( wrtbuff->GetBlkNb() ) + '.' + std::to_string( i );
      uint32_t checksum = checksums[i].get();
      std::shared_ptr<WrtCtx> wrtctx( new WrtCtx( checksum, fn, wrtbuff->GetChunk( i ), wrtbuff->GetStrpSize( i ) ) );
      uint32_t offset   = offsets[*fid].fetch_add( wrtctx->total_size );
      uint32_t strpsize = wrtbuff->GetStrpSize( i );
      auto &file = (*files)[*fid];
      auto wrt_handler = [files, wrtctx, fid, dirs, strpsize, checksum, offset, fn]( XrdCl::XRootDStatus &st )
        {
          if( !st.IsOK() ) return;
          // create respective CDH record
          dirs[*fid].Add( fn, strpsize, checksum, offset ); // TODO figure out the right file ID !!!
        };

      XrdCl::rcvry_func WrtRcvry = spares->spareid.empty() ? nullptr : WrtRecovery( spares, offset, wrtctx, files );
      writes.emplace_back( XrdCl::WriteV( file, offset, wrtctx->iov, wrtctx->iovcnt ).Recovery( WrtRcvry ) >> wrt_handler );
    }

    std::shared_ptr<WrtBuff> pend_buff = std::move( wrtbuff );
    std::shared_ptr<WrtCallback> _wrt_callback = wrt_callback;
    XrdCl::Async( XrdCl::Parallel( writes ) >> [pend_buff, _wrt_callback]( XrdCl::XRootDStatus &st ){ if( !st.IsOK() ) _wrt_callback->Run( st ); } );
  }
}
