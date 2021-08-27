//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
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

#include "XrdCl/XrdClXCpCtx.hh"
#include "XrdCl/XrdClXCpSrc.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"

#include <algorithm>

namespace XrdCl
{

XCpCtx::XCpCtx( const std::vector<std::string> &urls, uint64_t blockSize, uint8_t parallelSrc, uint64_t chunkSize, uint64_t parallelChunks, int64_t fileSize ) :
      pUrls( std::deque<std::string>( urls.begin(), urls.end() ) ), pBlockSize( blockSize ),
      pParallelSrc( parallelSrc ), pChunkSize( chunkSize ), pParallelChunks( parallelChunks ),
      pOffset( 0 ), pFileSize( -1 ), pFileSizeCV( 0 ), pDataReceived( 0 ), pDone( false ),
      pDoneCV( 0 ), pRefCount( 1 )
{
  SetFileSize( fileSize );
}

XCpCtx::~XCpCtx()
{
  // at this point there's no concurrency
  // this object dies as the last one
  while( !pSink.IsEmpty() )
  {
    PageInfo *chunk = pSink.Get();
    if( chunk )
      XCpSrc::DeleteChunk( chunk );
  }
}

bool XCpCtx::GetNextUrl( std::string & url )
{
  XrdSysMutexHelper lck( pMtx );
  if( pUrls.empty() ) return false;
  url = pUrls.front();
  pUrls.pop();
  return true;
}

XCpSrc* XCpCtx::WeakestLink( XCpSrc *exclude )
{
  uint64_t transferRate = -1; // set transferRate to max uint64 value
  XCpSrc *ret = 0;

  std::list<XCpSrc*>::iterator itr;
  for( itr = pSources.begin() ; itr != pSources.end() ; ++itr )
  {
    XCpSrc *src = *itr;
    if( src == exclude ) continue;
    uint64_t tmp = src->TransferRate();
    if( src->HasData() && tmp < transferRate )
    {
      ret = src;
      transferRate = tmp;
    }
  }

  return ret;
}

void XCpCtx::PutChunk( PageInfo* chunk )
{
  pSink.Put( chunk );
}

std::pair<uint64_t, uint64_t> XCpCtx::GetBlock()
{
  XrdSysMutexHelper lck( pMtx );

  uint64_t blkSize = pBlockSize, offset = pOffset;
  if( pOffset + blkSize > uint64_t( pFileSize ) )
    blkSize = pFileSize - pOffset;
  pOffset += blkSize;

  return std::make_pair( offset, blkSize );
}

void XCpCtx::SetFileSize( int64_t size )
{
  XrdSysMutexHelper lck( pMtx );
  if( pFileSize < 0 && size >= 0 )
  {
    XrdSysCondVarHelper lck( pFileSizeCV );
    pFileSize = size;
    pFileSizeCV.Broadcast();

    if( pBlockSize > uint64_t( pFileSize ) / pParallelSrc )
      pBlockSize = pFileSize / pParallelSrc;

    if( pBlockSize < pChunkSize )
      pBlockSize = pChunkSize;
  }
}

XRootDStatus XCpCtx::Initialize()
{
  for( uint8_t i = 0; i < pParallelSrc; ++i )
  {
    XCpSrc *src = new XCpSrc( pChunkSize, pParallelChunks, pFileSize, this );
    pSources.push_back( src );
    src->Start();
  }

  if( pSources.empty() )
  {
    Log *log = DefaultEnv::GetLog();
    log->Error( UtilityMsg, "Failed to initialize (failed to create new threads)" );
    return XRootDStatus( stError, errInternal, EAGAIN, "XCpCtx: failed to create new threads." );
  }

  return XRootDStatus();
}

XRootDStatus XCpCtx::GetChunk( XrdCl::PageInfo &ci )
{
  // if we received all the data we are done here
  if( pDataReceived == uint64_t( pFileSize ) )
  {
    XrdSysCondVarHelper lck( pDoneCV );
    pDone = true;
    pDoneCV.Broadcast();
    return XRootDStatus( stOK, suDone );
  }

  // if we don't have active sources it means we failed
  if( GetRunning() == 0 )
  {
    XrdSysCondVarHelper lck( pDoneCV );
    pDone = true;
    pDoneCV.Broadcast();
    return XRootDStatus( stError, errNoMoreReplicas );
  }

  PageInfo *chunk = pSink.Get();
  if( chunk )
  {
    pDataReceived += chunk->GetLength();
    ci = std::move( *chunk );
    delete chunk;
    return XRootDStatus( stOK, suContinue );
  }

  return XRootDStatus( stOK, suRetry );
}

void XCpCtx::NotifyIdleSrc()
{
  pDoneCV.Broadcast();
}

bool XCpCtx::AllDone()
{
  XrdSysCondVarHelper lck( pDoneCV );

  if( !pDone )
    pDoneCV.Wait( 60 );

  return pDone;
}

size_t XCpCtx::GetRunning()
{
  // count active sources
  size_t nbRunning = 0;
  std::list<XCpSrc*>::iterator itr;
  for( itr = pSources.begin() ; itr != pSources.end() ; ++ itr)
    if( (*itr)->IsRunning() )
      ++nbRunning;
  return nbRunning;
}


} /* namespace XrdCl */
