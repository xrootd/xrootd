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

#include "XrdCl/XrdClXCpSrc.hh"
#include "XrdCl/XrdClXCpCtx.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClUtils.hh"

#include <cmath>
#include <cstdlib>

namespace XrdCl
{

class ChunkHandler: public ResponseHandler
{
  public:

    ChunkHandler( XCpSrc *src, uint64_t offset, uint64_t size, char *buffer, File *handle, bool usepgrd ) :
      pSrc( src->Self() ), pOffset( offset ), pSize( size ), pBuffer( buffer ), pHandle( handle ), pUsePgRead( usepgrd )
    {

    }

    virtual ~ChunkHandler()
    {
      pSrc->Delete();
    }

    virtual void HandleResponse( XRootDStatus *status, AnyObject *response )
    {
      PageInfo *chunk = 0;
      if( response ) // get the response
      {
        ToPgInfo( response, chunk );
        delete response;
      }

      if( !chunk && status->IsOK() ) // if the response is not there make sure the status is error
      {
        *status = XRootDStatus( stError, errInternal );
      }

      if( status->IsOK() && chunk->GetLength() != pSize ) // the file size on the server is different
      {                                              // than the one specified in metalink file
        *status = XRootDStatus( stError, errDataError );
      }

      if( !status->IsOK() )
      {
        delete[] pBuffer;
        delete   chunk;
        chunk = 0;
      }

      pSrc->ReportResponse( status, chunk, pHandle );

      delete this;
    }

  private:

    void ToPgInfo( AnyObject *response, PageInfo *&chunk )
    {
      if( pUsePgRead )
      {
        response->Get( chunk );
        response->Set( ( int* )0 );
      }
      else
      {
        ChunkInfo *rsp = nullptr;
        response->Get( rsp );
        chunk = new PageInfo( rsp->offset, rsp->length, rsp->buffer );
      }
    }

    XCpSrc           *pSrc;
    uint64_t           pOffset;
    uint64_t           pSize;
    char              *pBuffer;
    File              *pHandle;
    bool               pUsePgRead;
};


XCpSrc::XCpSrc( uint32_t chunkSize, uint8_t parallel, int64_t fileSize, XCpCtx *ctx ) :
  pChunkSize( chunkSize ), pParallel( parallel ), pFileSize( fileSize ), pThread(),
  pCtx( ctx->Self() ), pFile( 0 ), pCurrentOffset( 0 ), pBlkEnd( 0 ), pDataTransfered( 0 ), pRefCount( 1 ),
  pRunning( false ), pStartTime( 0 ), pTransferTime( 0 ), pUsePgRead( false )
{
}

XCpSrc::~XCpSrc()
{
  pCtx->RemoveSrc( this );
  pCtx->Delete();
}

void XCpSrc::Start()
{
  pRunning = true;
  int rc = pthread_create( &pThread, 0, Run, this );
  if( rc )
  {
    pRunning = false;
    pCtx->RemoveSrc( this );
    pCtx->Delete();
  }
}

void* XCpSrc::Run( void* arg )
{
  XCpSrc *me = static_cast<XCpSrc*>( arg );
  me->StartDownloading();
  me->Delete();
  return 0;
}

void XCpSrc::StartDownloading()
{
  XRootDStatus st = Initialize();
  if( !st.IsOK() )
  {
    pRunning = false;
    // notify those who wait for the file
    // size, they won't get it from this
    // source
    pCtx->NotifyInitExpectant();
    // put a null chunk so we are sure
    // the main thread doesn't get stuck
    // at the sync queue
    pCtx->PutChunk( 0 );
    return;
  }

  // start counting transfer time
  pStartTime = time( 0 );

  while( pRunning )
  {
    st = ReadChunks();
    if( st.IsOK() && st.code == suPartial )
    {
      // we have only ongoing transfers
      // so we can already ask for new block
      if( GetWork().IsOK() ) continue;
    }
    else if( st.IsOK() && st.code == suDone )
    {
      // if we are done, try to get more work,
      // if successful continue
      if( GetWork().IsOK() ) continue;
      // keep track of the time before we go idle
      pTransferTime += time( 0 ) - pStartTime;
      // check if the overall download process is
      // done, this makes the thread wait until
      // either the download is done, or a source
      // went to error, or a 60s timeout has been
      // reached (the timeout is there so we can
      // check if a source degraded in the meanwhile
      // and now we can steal from it)
      if( !pCtx->AllDone() )
      {
        // reset start time after pause
        pStartTime = time( 0 );
        continue;
      }
      // stop counting
      // otherwise we are done here
      pRunning = false;
      return;
    }

    XRootDStatus *status = pReports.Get();
    if( !status->IsOK() )
    {
      Log *log = DefaultEnv::GetLog();
      std::string myHost = URL( pUrl ).GetHostName();
      log->Error( UtilityMsg, "Failed to read chunk from %s: %s", myHost.c_str(), status->GetErrorMessage().c_str() );

      if( !Recover().IsOK() )
      {
        delete status;
        pRunning = false;
        // notify idle sources, they might be
        // interested in taking over my workload
        pCtx->NotifyIdleSrc();
        // put a null chunk so we are sure
        // the main thread doesn't get stuck
        // at the sync queue
        pCtx->PutChunk( 0 );
        // if we have data we need to wait for someone to take over
        // unless the extreme copy is over, in this case we don't care
        while( HasData() && !pCtx->AllDone() );

        return;
      }
    }
    delete status;
  }
}

XRootDStatus XCpSrc::Initialize()
{
  Log *log = DefaultEnv::GetLog();
  XRootDStatus st;

  do
  {
    if( !pCtx->GetNextUrl( pUrl ) )
    {
      log->Error( UtilityMsg, "Failed to initialize XCp source, no more replicas to try" );
      return XRootDStatus( stError );
    }

    log->Debug( UtilityMsg, "Opening %s for reading", pUrl.c_str() );

    std::string value;
    DefaultEnv::GetEnv()->GetString( "ReadRecovery", value );

    pFile = new File();
    pFile->SetProperty( "ReadRecovery", value );

    st = pFile->Open( pUrl, OpenFlags::Read );
    if( !st.IsOK() )
    {
      log->Warning( UtilityMsg, "Failed to open %s for reading: %s", pUrl.c_str(), st.GetErrorMessage().c_str() );
      DeletePtr( pFile );
      continue;
    }

    URL url( pUrl );
    if( ( !url.IsLocalFile() && !pFile->IsSecure() ) ||
        ( url.IsLocalFile() && url.IsMetalink() ) )
    {
      std::string datasrv;
      pFile->GetProperty( "DataServer", datasrv );
      //--------------------------------------------------------------------
      // Decide whether we can use PgRead
      //--------------------------------------------------------------------
      int val = XrdCl::DefaultCpUsePgWrtRd;
      XrdCl::DefaultEnv::GetEnv()->GetInt( "CpUsePgWrtRd", val );
      pUsePgRead = XrdCl::Utils::HasPgRW( datasrv ) && ( val == 1 );
    }

    if( pFileSize < 0 )
    {
      StatInfo *statInfo = 0;
      st = pFile->Stat( false, statInfo );
      if( !st.IsOK() )
      {
        log->Warning( UtilityMsg, "Failed to stat %s: %s", pUrl.c_str(), st.GetErrorMessage().c_str() );
        DeletePtr( pFile );
        continue;
      }
      pFileSize = statInfo->GetSize();
      pCtx->SetFileSize( pFileSize );
      delete statInfo;
    }
  }
  while( !st.IsOK() );

  std::pair<uint64_t, uint64_t> p = pCtx->GetBlock();
  pCurrentOffset = p.first;
  pBlkEnd        = p.second + p.first;

  return st;
}

XRootDStatus XCpSrc::Recover()
{
  Log *log = DefaultEnv::GetLog();
  XRootDStatus st;

  do
  {
    if( !pCtx->GetNextUrl( pUrl ) )
    {
      log->Error( UtilityMsg, "Failed to initialize XCp source, no more replicas to try" );
      return XRootDStatus( stError );
    }

    log->Debug( UtilityMsg, "Opening %s for reading", pUrl.c_str() );

    std::string value;
    DefaultEnv::GetEnv()->GetString( "ReadRecovery", value );

    pFile = new File();
    pFile->SetProperty( "ReadRecovery", value );

    st = pFile->Open( pUrl, OpenFlags::Read );
    if( !st.IsOK() )
    {
      DeletePtr( pFile );
      log->Warning( UtilityMsg, "Failed to open %s for reading: %s", pUrl.c_str(), st.GetErrorMessage().c_str() );
    }

    URL url( pUrl );
    if( ( !url.IsLocalFile() && pFile->IsSecure() ) ||
        ( url.IsLocalFile() && url.IsMetalink() ) )
    {
      std::string datasrv;
      pFile->GetProperty( "DataServer", datasrv );
      //--------------------------------------------------------------------
      // Decide whether we can use PgRead
      //--------------------------------------------------------------------
      int val = XrdCl::DefaultCpUsePgWrtRd;
      XrdCl::DefaultEnv::GetEnv()->GetInt( "CpUsePgWrtRd", val );
      pUsePgRead = XrdCl::Utils::HasPgRW( datasrv ) && ( val == 1 );
    }
  }
  while( !st.IsOK() );

  pRecovered.insert( pOngoing.begin(), pOngoing.end() );
  pOngoing.clear();

  // since we have a brand new source, we need
  // to restart transfer rate statistics
  pTransferTime   = 0;
  pStartTime      = time( 0 );
  pDataTransfered = 0;

  return st;
}

XRootDStatus XCpSrc::ReadChunks()
{
  XrdSysMutexHelper lck( pMtx );

  while( pOngoing.size() < pParallel && !pRecovered.empty() )
  {
    std::pair<uint64_t, uint64_t> p;
    std::map<uint64_t, uint64_t>::iterator itr = pRecovered.begin();
    p = *itr;
    pOngoing.insert( p );
    pRecovered.erase( itr );

    char *buffer = new char[p.second];
    ChunkHandler *handler = new ChunkHandler( this, p.first, p.second, buffer, pFile, pUsePgRead );
    XRootDStatus st = pUsePgRead
                    ? pFile->PgRead( p.first, p.second, buffer, handler )
                    : pFile->Read( p.first, p.second, buffer, handler );
    if( !st.IsOK() )
    {
      delete[] buffer;
      delete   handler;
      ReportResponse( new XRootDStatus( st ), 0, pFile );
      return st;
    }
  }

  while( pOngoing.size() < pParallel && pCurrentOffset < pBlkEnd )
  {
    uint64_t chunkSize = pChunkSize;
    if( pCurrentOffset + chunkSize > pBlkEnd )
      chunkSize = pBlkEnd - pCurrentOffset;
    pOngoing[pCurrentOffset] = chunkSize;
    char *buffer = new char[chunkSize];
    ChunkHandler *handler = new ChunkHandler( this, pCurrentOffset, chunkSize, buffer, pFile, pUsePgRead );
    XRootDStatus st = pUsePgRead
                    ? pFile->PgRead( pCurrentOffset, chunkSize, buffer, handler )
                    : pFile->Read( pCurrentOffset, chunkSize, buffer, handler );
    pCurrentOffset += chunkSize;
    if( !st.IsOK() )
    {
      delete[] buffer;
      delete   handler;
      ReportResponse( new XRootDStatus( st ), 0, pFile );
      return st;
    }
  }

  if( pOngoing.empty() ) return XRootDStatus( stOK, suDone );

  if( pRecovered.empty() && pCurrentOffset >= pBlkEnd ) return XRootDStatus( stOK, suPartial );

  return XRootDStatus( stOK, suContinue );
}

void XCpSrc::ReportResponse( XRootDStatus *status, PageInfo *chunk, File *handle )
{
  XrdSysMutexHelper lck( pMtx );
  bool ignore = false;

  if( status->IsOK() )
  {
    // if the status is OK remove it from
    // the list of ongoing transfers, if it
    // was not on the list we ignore the
    // response (this could happen due to
    // source change or stealing)
    ignore = !pOngoing.erase( chunk->GetOffset() );
  }
  else if( FilesEqual( pFile, handle ) )
  {
    // if the status is NOT OK, and pFile
    // match the handle it means that we see
    // an error for the first time, map the
    // broken file to the number of outstanding
    // asynchronous operations and reset the pointer
    pFailed[pFile] = pOngoing.size();
    pFile = 0;
  }
  else
    DeletePtr( status );

  if( !FilesEqual( pFile, handle ) )
  {
    // if the pFile does not match the handle,
    // it means that this response came from
    // a broken source, decrement the count of
    // outstanding async operations for this src,
    --pFailed[handle];
    if( pFailed[handle] == 0 )
    {
      // if this was the last outstanding operation
      // close the file and delete it
      pFailed.erase( handle );
      XRootDStatus st = handle->Close();
      delete handle;
    }
  }

  lck.UnLock();

  if( status ) pReports.Put( status );

  if( ignore )
  {
    DeleteChunk( chunk );
    return;
  }

  if( chunk )
  {
    pDataTransfered += chunk->GetLength();
    pCtx->PutChunk( chunk );
  }
}

void XCpSrc::Steal( XCpSrc *src )
{
  if( !src ) return;

  XrdSysMutexHelper lck1( pMtx ), lck2( src->pMtx );

  Log *log = DefaultEnv::GetLog();
  std::string myHost = URL( pUrl ).GetHostName(), srcHost = URL( src->pUrl ).GetHostName();

  if( !src->pRunning )
  {
    // the source we are stealing from is in error state, we can have everything

    pRecovered.insert( src->pOngoing.begin(),   src->pOngoing.end() );
    pRecovered.insert( src->pRecovered.begin(), src->pRecovered.end() );
    pCurrentOffset = src->pCurrentOffset;
    pBlkEnd        = src->pBlkEnd;

    src->pOngoing.clear();
    src->pRecovered.clear();
    src->pCurrentOffset = 0;
    src->pBlkEnd = 0;

    // a broken source might be waiting for
    // someone to take over his data, so we
    // need to notify
    pCtx->NotifyIdleSrc();

    log->Debug( UtilityMsg, "s%: Stealing everything from %s", myHost.c_str(), srcHost.c_str() );

    return;
  }

  // the source we are stealing from is just slower, only take part of its work
  // so we want a fraction of its work we want for ourself
  uint64_t myTransferRate = TransferRate(), srcTransferRate = src->TransferRate();
  if( myTransferRate == 0 ) return;
  double fraction = double( myTransferRate ) / double( myTransferRate + srcTransferRate );

  if( src->pCurrentOffset < src->pBlkEnd )
  {
    // the source still has a block of data
    uint64_t blkSize = src->pBlkEnd - src->pCurrentOffset;
    uint64_t steal = static_cast<uint64_t>( round( fraction * blkSize ) );
    // if after stealing there will be less than one chunk
    // take everything
    if( blkSize - steal <= pChunkSize )
      steal = blkSize;

    pCurrentOffset = src->pBlkEnd - steal;
    pBlkEnd        = src->pBlkEnd;
    src->pBlkEnd  -= steal;

    log->Debug( UtilityMsg, "s%: Stealing fraction (%f) of block from %s", myHost.c_str(), fraction, srcHost.c_str() );

    return;
  }

  if( !src->pRecovered.empty() )
  {
    size_t count = static_cast<size_t>( round( fraction * src->pRecovered.size() ) );
    while( count-- )
    {
      std::map<uint64_t, uint64_t>::iterator itr = src->pRecovered.begin();
      pRecovered.insert( *itr );
      src->pRecovered.erase( itr );
    }

    log->Debug( UtilityMsg, "s%: Stealing fraction (%f) of recovered chunks from %s", myHost.c_str(), fraction, srcHost.c_str() );

    return;
  }

  // * a fraction < 0.5 means that we are actually slower (so it does
  //   not make sense to steal ongoing's from someone who's faster)
  // * a fraction ~ 0.5 means that we have more or less the same transfer
  //   rate (similarly, it doesn't make sense to steal)
  // * the source needs to be really faster (though, this is an arbitrary
  //   choice) to actually steal something
  if( !src->pOngoing.empty() && fraction > 0.7 )
  {
    size_t count = static_cast<size_t>( round( fraction * src->pOngoing.size() ) );
    while( count-- )
    {
      std::map<uint64_t, uint64_t>::iterator itr = src->pOngoing.begin();
      pRecovered.insert( *itr );
      src->pOngoing.erase( itr );
    }

    log->Debug( UtilityMsg, "s%: Stealing fraction (%f) of ongoing chunks from %s", myHost.c_str(), fraction, srcHost.c_str() );
  }
}

XRootDStatus XCpSrc::GetWork()
{
  std::pair<uint64_t, uint64_t> p = pCtx->GetBlock();

  if( p.second > 0 )
  {
    XrdSysMutexHelper lck( pMtx );
    pCurrentOffset = p.first;
    pBlkEnd        = p.first + p.second;

    Log *log = DefaultEnv::GetLog();
    std::string myHost = URL( pUrl ).GetHostName();
    log->Debug( UtilityMsg, "s% got next block", myHost.c_str() );

    return XRootDStatus();
  }

  XCpSrc *wLink = pCtx->WeakestLink( this );
  Steal( wLink );

  // if we managed to steal something declare success
  if( pCurrentOffset < pBlkEnd || !pRecovered.empty() ) return XRootDStatus();
  // otherwise return an error
  return XRootDStatus( stError, errInvalidOp );
}

uint64_t XCpSrc::TransferRate()
{
  time_t duration = pTransferTime + time( 0 ) - pStartTime;
  return pDataTransfered / ( duration + 1 ); // add one to avoid floating point exception
}

} /* namespace XrdCl */
