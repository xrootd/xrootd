//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClFileStateHandler.hh"
#include "XrdCl/XrdClMessageUtils.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  File::File()
  {
    pStateHandler = new FileStateHandler();
  }

  //------------------------------------------------------------------------
  // Destructor
  //------------------------------------------------------------------------
  File::~File()
  {
    Close();
    delete pStateHandler;
  }

  //----------------------------------------------------------------------------
  // Open the file pointed to by the given URL - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Open( const std::string &url,
                           uint16_t           flags,
                           uint16_t           mode,
                           ResponseHandler   *handler,
                           uint16_t           timeout )
  {
    return pStateHandler->Open( url, flags, mode, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Open the file pointed to by the given URL - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Open( const std::string &url,
                           uint16_t           flags,
                           uint16_t           mode,
                           uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = Open( url, flags, mode, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Close the file - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Close( ResponseHandler *handler,
                            uint16_t         timeout )
  {
    return pStateHandler->Close( handler, timeout );
  }


  //----------------------------------------------------------------------------
  // Close the file
  //----------------------------------------------------------------------------
  XRootDStatus File::Close( uint16_t timeout )
  {
    SyncResponseHandler handler;
    Status st = Close( &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for this file - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Stat( bool             force,
                           ResponseHandler *handler,
                           uint16_t         timeout )
  {
    return pStateHandler->Stat( force, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for this file - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Stat( bool       force,
                           StatInfo *&response,
                           uint16_t   timeout )
  {
    SyncResponseHandler handler;
    Status st = Stat( force, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }


  //----------------------------------------------------------------------------
  // Read a data chunk at a given offset - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Read( uint64_t         offset,
                           uint32_t         size,
                           void            *buffer,
                           ResponseHandler *handler,
                           uint16_t         timeout )
  {
    return pStateHandler->Read( offset, size, buffer, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Read a data chunk at a given offset - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Read( uint64_t  offset,
                           uint32_t  size,
                           void     *buffer,
                           uint32_t &bytesRead,
                           uint16_t  timeout )
  {
    SyncResponseHandler handler;
    Status st = Read( offset, size, buffer, &handler, timeout );
    if( !st.IsOK() )
      return st;

    uint32_t *bytesR = 0;
    XRootDStatus status = MessageUtils::WaitForResponse( &handler, bytesR );
    if( status.IsOK() )
    {
      bytesRead = *bytesR;
      delete bytesR;
    }
    return status;
  }

  //----------------------------------------------------------------------------
  // Write a data chank at a given offset - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Write( uint64_t         offset,
                            uint32_t         size,
                            void            *buffer,
                            ResponseHandler *handler,
                            uint16_t         timeout )
  {
    return pStateHandler->Write( offset, size, buffer, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Write a data chunk at a given offset - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Write( uint64_t  offset,
                            uint32_t  size,
                            void     *buffer,
                            uint16_t  timeout )
  {
    SyncResponseHandler handler;
    Status st = Write( offset, size, buffer, &handler, timeout );
    if( !st.IsOK() )
      return st;

    XRootDStatus status = MessageUtils::WaitForStatus( &handler );
    return status;
  }

  //----------------------------------------------------------------------------
  // Commit all pending disk writes - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Sync( ResponseHandler *handler,
                           uint16_t         timeout )
  {
    return pStateHandler->Sync( handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Commit all pending disk writes - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Sync( uint16_t timeout )
  {
    SyncResponseHandler handler;
    Status st = Sync( &handler, timeout );
    if( !st.IsOK() )
      return st;

    XRootDStatus status = MessageUtils::WaitForStatus( &handler );
    return status;
  }

  //----------------------------------------------------------------------------
  // Truncate the file to a particular size - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Truncate( uint64_t         size,
                               ResponseHandler *handler,
                               uint16_t         timeout )
  {
    return pStateHandler->Truncate( size, handler, timeout );
  }


  //----------------------------------------------------------------------------
  // Truncate the file to a particular size - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Truncate( uint64_t size, uint16_t timeout )
  {
    SyncResponseHandler handler;
    Status st = Truncate( size, &handler, timeout );
    if( !st.IsOK() )
      return st;

    XRootDStatus status = MessageUtils::WaitForStatus( &handler );
    return status;
  }

  //----------------------------------------------------------------------------
  // Read scattered data chunks in one operation - async
  //----------------------------------------------------------------------------
  XRootDStatus File::VectorRead( const ChunkList &chunks,
                                 void            *buffer,
                                 ResponseHandler *handler,
                                 uint16_t         timeout )
  {
    return pStateHandler->VectorRead( chunks, buffer, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Read scattered data chunks in one operation - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::VectorRead( const ChunkList  &chunks,
                                 void             *buffer,
                                 VectorReadInfo  *&vReadInfo,
                                 uint16_t          timeout )
  {
    SyncResponseHandler handler;
    Status st = VectorRead( chunks, buffer, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, vReadInfo );
  }
}
