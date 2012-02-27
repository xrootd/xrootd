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

    StatInfo *response = 0;
    XRootDStatus stat = MessageUtils::WaitForResponse( &handler, response );
    delete response;
    return st;
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
                           uint16_t  timeout )
  {
    SyncResponseHandler handler;
    Status st = Read( offset, size, buffer, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Write a data chank at a given offset - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Write( uint64_t         /*offset*/,
                            uint32_t         /*size*/,
                            void            */*buffer*/,
                            ResponseHandler */*handler*/,
                            uint16_t         /*timeout*/ )
  {
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Write a data chunk at a given offset - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Write( uint64_t  /*offset*/,
                            uint32_t  /*size*/,
                            void     */*buffer*/,
                            uint16_t  /*timeout*/ )
  {
    return XRootDStatus();
  }

}
