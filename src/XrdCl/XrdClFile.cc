//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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

#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClFileStateHandler.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClPlugInManager.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // The implementation
  //----------------------------------------------------------------------------
  struct FileImpl
  {
    FileImpl( FilePlugIn *plugin ) :
      pStateHandler( std::make_shared<FileStateHandler>( plugin ) )
    {
    }

    FileImpl( bool useVirtRedirector, FilePlugIn *plugin ) :
      pStateHandler( std::make_shared<FileStateHandler>( useVirtRedirector, plugin ) )
    {
    }

    std::shared_ptr<FileStateHandler> pStateHandler;
  };

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  File::File( bool enablePlugIns ):
    pPlugIn(0),
    pEnablePlugIns( enablePlugIns )
  {
    pImpl = new FileImpl( pPlugIn );
  }

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  File::File( VirtRedirect virtRedirect, bool enablePlugIns ):
    pPlugIn(0),
    pEnablePlugIns( enablePlugIns )
  {
    pImpl = new FileImpl( virtRedirect == EnableVirtRedirect, pPlugIn );
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  File::~File()
  {
    //--------------------------------------------------------------------------
    // This, in principle, should never ever happen. Except for the case
    // when we're interfaced with ROOT that may call this desctructor from
    // its garbage collector, from its __cxa_finalize, ie. after the XrdCl lib
    // has been finalized by the linker. So, if we don't have the log object
    // at this point we just give up the hope.
    // Also, make sure the PostMaster threads are running - if not the Close
    // will hang forever (this could happen when Python interpreter exits).
    //--------------------------------------------------------------------------
    if ( DefaultEnv::GetLog() && DefaultEnv::GetPostMaster()->IsRunning() && IsOpen() )
      XRootDStatus status = Close( nullptr, 0 );
    delete pImpl;
    delete pPlugIn;
  }

  //----------------------------------------------------------------------------
  // Open the file pointed to by the given URL - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Open( const std::string &url,
                           OpenFlags::Flags   flags,
                           Access::Mode       mode,
                           ResponseHandler   *handler,
                           time_t             timeout )
  {
    //--------------------------------------------------------------------------
    // Check if we need to install and run a plug-in for this URL
    //--------------------------------------------------------------------------
    if( pEnablePlugIns && !pPlugIn )
    {
      Log *log = DefaultEnv::GetLog();
      PlugInFactory *fact = DefaultEnv::GetPlugInManager()->GetFactory( url );
      if( fact )
      {
        pPlugIn = fact->CreateFile( url );
        if( !pPlugIn )
        {
          log->Error( FileMsg, "Plug-in factory failed to produce a plug-in "
                      "for %s, continuing without one", url.c_str() );
        }
      }
    }

    //--------------------------------------------------------------------------
    // Open the file
    //--------------------------------------------------------------------------
    if( pPlugIn )
      return pPlugIn->Open( url, flags, mode, handler, timeout );

    return FileStateHandler::Open( pImpl->pStateHandler, url, flags, mode, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Open the file pointed to by the given URL - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Open( const std::string &url,
                           OpenFlags::Flags   flags,
                           Access::Mode       mode,
                           time_t             timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = Open( url, flags, mode, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Close the file - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Close( ResponseHandler *handler,
                            time_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Close( handler, timeout );

    return FileStateHandler::Close( pImpl->pStateHandler, handler, timeout );
  }


  //----------------------------------------------------------------------------
  // Close the file
  //----------------------------------------------------------------------------
  XRootDStatus File::Close( time_t timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = Close( &handler, timeout );
    if( !st.IsOK() || st.code == suAlreadyDone )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for this file - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Stat( bool             force,
                           ResponseHandler *handler,
                           time_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Stat( force, handler, timeout );

    return FileStateHandler::Stat( pImpl->pStateHandler, force, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for this file - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Stat( bool       force,
                           StatInfo *&response,
                           time_t     timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = Stat( force, &handler, timeout );
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
                           time_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Read( offset, size, buffer, handler, timeout );

    return FileStateHandler::Read( pImpl->pStateHandler, offset, size, buffer, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Read a data chunk at a given offset - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Read( uint64_t  offset,
                           uint32_t  size,
                           void     *buffer,
                           uint32_t &bytesRead,
                           time_t    timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = Read( offset, size, buffer, &handler, timeout );
    if( !st.IsOK() )
      return st;

    ChunkInfo *chunkInfo = 0;
    XRootDStatus status = MessageUtils::WaitForResponse( &handler, chunkInfo );
    if( status.IsOK() )
    {
      bytesRead = chunkInfo->length;
      delete chunkInfo;
    }
    return status;
  }

  //------------------------------------------------------------------------
  // Read number of pages at a given offset - async
  //------------------------------------------------------------------------
  XRootDStatus File::PgRead( uint64_t         offset,
                             uint32_t         size,
                             void            *buffer,
                             ResponseHandler *handler,
                             time_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->PgRead( offset, size, buffer, handler, timeout );

    return FileStateHandler::PgRead( pImpl->pStateHandler, offset, size, buffer, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Read number of pages at a given offset - async
  //------------------------------------------------------------------------
  XRootDStatus File::PgRead( uint64_t               offset,
                             uint32_t               size,
                             void                  *buffer,
                             std::vector<uint32_t> &cksums,
                             uint32_t              &bytesRead,
                             time_t                 timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = PgRead( offset, size, buffer, &handler, timeout );
    if( !st.IsOK() )
      return st;

    PageInfo *pageInfo = 0;
    XRootDStatus status = MessageUtils::WaitForResponse( &handler, pageInfo );
    if( status.IsOK() )
    {
      bytesRead = pageInfo->GetLength();
      cksums = pageInfo->GetCksums();
      delete pageInfo;
    }
    return status;
  }

  //----------------------------------------------------------------------------
  // Write a data chunk at a given offset - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Write( uint64_t         offset,
                            uint32_t         size,
                            const void      *buffer,
                            ResponseHandler *handler,
                            time_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Write( offset, size, buffer, handler, timeout );

    return FileStateHandler::Write( pImpl->pStateHandler, offset, size, buffer, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Write a data chunk at a given offset - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Write( uint64_t    offset,
                            uint32_t    size,
                            const void *buffer,
                            time_t      timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = Write( offset, size, buffer, &handler, timeout );
    if( !st.IsOK() )
      return st;

    XRootDStatus status = MessageUtils::WaitForStatus( &handler );
    return status;
  }


  XRootDStatus File::Write( uint64_t          offset,
                            Buffer          &&buffer,
                            ResponseHandler  *handler,
                            time_t            timeout )
  {
    if( pPlugIn )
      return pPlugIn->Write( offset, std::move( buffer ), handler, timeout );

    return FileStateHandler::Write( pImpl->pStateHandler, offset, std::move( buffer ), handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Write a data chunk at a given offset - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Write( uint64_t    offset,
                            Buffer    &&buffer,
                            time_t      timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = Write( offset, std::move( buffer ), &handler, timeout );
    if( !st.IsOK() )
      return st;

    XRootDStatus status = MessageUtils::WaitForStatus( &handler );
    return status;
  }

  //------------------------------------------------------------------------
  // Write a data from a given file descriptor at a given offset - async
  //------------------------------------------------------------------------
  XRootDStatus File::Write( uint64_t            offset,
                            uint32_t            size,
                            Optional<uint64_t>  fdoff,
                            int                 fd,
                            ResponseHandler    *handler,
                            time_t              timeout )
  {
    if( pPlugIn )
      return pPlugIn->Write( offset, size, fdoff, fd, handler, timeout );

    return FileStateHandler::Write( pImpl->pStateHandler, offset, size, fdoff, fd, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Write a data from a given file descriptor at a given offset - sync
  //------------------------------------------------------------------------
  XRootDStatus File::Write( uint64_t            offset,
                            uint32_t            size,
                            Optional<uint64_t>  fdoff,
                            int                 fd,
                            time_t              timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = Write( offset, size, fdoff, fd, &handler, timeout );
    if( !st.IsOK() )
      return st;

    XRootDStatus status = MessageUtils::WaitForStatus( &handler );
    return status;
  }

  //------------------------------------------------------------------------
  // Write number of pages at a given offset - async
  //------------------------------------------------------------------------
  XRootDStatus File::PgWrite( uint64_t               offset,
                              uint32_t               size,
                              const void            *buffer,
                              std::vector<uint32_t> &cksums,
                              ResponseHandler       *handler,
                              time_t                 timeout )
  {
    if( pPlugIn )
      return pPlugIn->PgWrite( offset, size, buffer, cksums, handler, timeout );

    return FileStateHandler::PgWrite( pImpl->pStateHandler, offset, size, buffer, cksums, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Write number of pages at a given offset - sync
  //------------------------------------------------------------------------
  XRootDStatus File::PgWrite( uint64_t               offset,
                              uint32_t               size,
                              const void            *buffer,
                              std::vector<uint32_t> &cksums,
                              time_t                 timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = PgWrite( offset, size, buffer, cksums, &handler, timeout );
    if( !st.IsOK() )
      return st;

    XRootDStatus status = MessageUtils::WaitForStatus( &handler );
    return status;
  }

  //----------------------------------------------------------------------------
  // Commit all pending disk writes - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Sync( ResponseHandler *handler,
                           time_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Sync( handler, timeout );

    return FileStateHandler::Sync( pImpl->pStateHandler, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Commit all pending disk writes - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Sync( time_t timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = Sync( &handler, timeout );
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
                               time_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Truncate( size, handler, timeout );

    return FileStateHandler::Truncate( pImpl->pStateHandler, size, handler, timeout );
  }


  //----------------------------------------------------------------------------
  // Truncate the file to a particular size - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Truncate( uint64_t size, time_t timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = Truncate( size, &handler, timeout );
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
                                 time_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->VectorRead( chunks, buffer, handler, timeout );

    return FileStateHandler::VectorRead( pImpl->pStateHandler, chunks, buffer, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Read scattered data chunks in one operation - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::VectorRead( const ChunkList  &chunks,
                                 void             *buffer,
                                 VectorReadInfo  *&vReadInfo,
                                 time_t            timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = VectorRead( chunks, buffer, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, vReadInfo );
  }

  //------------------------------------------------------------------------
  // Write scattered data chunks in one operation - async
  //------------------------------------------------------------------------
  XRootDStatus File::VectorWrite( const ChunkList &chunks,
                            ResponseHandler *handler,
                            time_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->VectorWrite( chunks, handler, timeout );

    return FileStateHandler::VectorWrite( pImpl->pStateHandler, chunks, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Read scattered data chunks in one operation - sync
  //------------------------------------------------------------------------
  XRootDStatus File::VectorWrite( const ChunkList  &chunks,
                           time_t            timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = VectorWrite( chunks, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //------------------------------------------------------------------------
  // Write scattered buffers in one operation - async
  //------------------------------------------------------------------------
  XRootDStatus File::WriteV( uint64_t            offset,
                             const struct iovec *iov,
                             int                 iovcnt,
                             ResponseHandler    *handler,
                             time_t              timeout )
  {
    if( pPlugIn )
      return pPlugIn->WriteV( offset, iov, iovcnt, handler, timeout );

    return FileStateHandler::WriteV( pImpl->pStateHandler, offset, iov, iovcnt, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Write scattered buffers in one operation - sync
  //------------------------------------------------------------------------
  XRootDStatus File::WriteV( uint64_t            offset,
                             const struct iovec *iov,
                             int                 iovcnt,
                             time_t              timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = WriteV( offset, iov, iovcnt, &handler, timeout );
    if( !st.IsOK() )
      return st;

    XRootDStatus status = MessageUtils::WaitForStatus( &handler );
    return status;
  }

  //------------------------------------------------------------------------
  //! Read data into scattered buffers in one operation - async
  //!
  //! @param offset    offset from the beginning of the file
  //! @param iov       list of the buffers to be written
  //! @param iovcnt    number of buffers
  //! @param handler   handler to be notified when the response arrives
  //! @param timeout   timeout value, if 0 then the environment default
  //!                  will be used
  //! @return          status of the operation
  //------------------------------------------------------------------------
  XRootDStatus File::ReadV( uint64_t         offset,
                            struct iovec    *iov,
                            int              iovcnt,
                            ResponseHandler *handler,
                            time_t           timeout )
  {
    return FileStateHandler::ReadV( pImpl->pStateHandler, offset, iov, iovcnt, handler, timeout );
  }

  //------------------------------------------------------------------------
  //! Read data into scattered buffers in one operation - sync
  //!
  //! @param offset    offset from the beginning of the file
  //! @param iov       list of the buffers to be written
  //! @param iovcnt    number of buffers
  //! @param handler   handler to be notified when the response arrives
  //! @param timeout   timeout value, if 0 then the environment default
  //!                  will be used
  //! @return          status of the operation
  //------------------------------------------------------------------------
  XRootDStatus File::ReadV( uint64_t      offset,
                            struct iovec *iov,
                            int           iovcnt,
                            uint32_t     &bytesRead,
                            time_t        timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = ReadV( offset, iov, iovcnt, &handler, timeout );
    if( !st.IsOK() )
      return st;

    VectorReadInfo *vrInfo = 0;
    XRootDStatus status = MessageUtils::WaitForResponse( &handler, vrInfo );
    if( status.IsOK() )
    {
      bytesRead = vrInfo->GetSize();
      delete vrInfo;
    }
    return status;
  }

  //----------------------------------------------------------------------------
  // Performs a custom operation on an open file, server implementation
  // dependent - async
  //----------------------------------------------------------------------------
  XRootDStatus File::Fcntl( const Buffer    &arg,
                            ResponseHandler *handler,
                            time_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Fcntl( arg, handler, timeout );

    return FileStateHandler::Fcntl( pImpl->pStateHandler, arg, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Performs a custom operation on an open file, server implementation
  // dependent - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Fcntl( const Buffer     &arg,
                            Buffer          *&response,
                            time_t            timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = Fcntl( arg, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //------------------------------------------------------------------------
  //! Get access token to a file - async
  //------------------------------------------------------------------------
  XRootDStatus File::Visa( ResponseHandler *handler,
                           time_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Visa( handler, timeout );

    return FileStateHandler::Visa( pImpl->pStateHandler, handler, timeout );
  }

  //----------------------------------------------------------------------------
  // Get access token to a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus File::Visa( Buffer   *&visa,
                           time_t     timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = Visa( &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, visa );
  }

  //------------------------------------------------------------------------
  // Set extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus File::SetXAttr( const std::vector<xattr_t>  &attrs,
                               ResponseHandler             *handler,
                               time_t                       timeout )
  {
    if( pPlugIn )
      return XRootDStatus( stError, errNotSupported );

    return FileStateHandler::SetXAttr( pImpl->pStateHandler, attrs, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Set extended attributes - sync
  //------------------------------------------------------------------------
  XRootDStatus File::SetXAttr( const std::vector<xattr_t>  &attrs,
                               std::vector<XAttrStatus>    &result,
                               time_t                       timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = SetXAttr( attrs, &handler, timeout );
    if( !st.IsOK() )
      return st;

    std::vector<XAttrStatus> *resp = 0;
    st = MessageUtils::WaitForResponse( &handler, resp );
    if( resp ) result.swap( *resp );
    delete resp;

    return st;
  }

  //------------------------------------------------------------------------
  // Get extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus File::GetXAttr( const std::vector<std::string>  &attrs,
                               ResponseHandler                 *handler,
                               time_t                           timeout )
  {
    if( pPlugIn )
      return XRootDStatus( stError, errNotSupported );

    return FileStateHandler::GetXAttr( pImpl->pStateHandler, attrs, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Get extended attributes - sync
  //------------------------------------------------------------------------
  XRootDStatus File::GetXAttr( const std::vector<std::string>  &attrs,
                               std::vector<XAttr>              &result,
                               time_t                           timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = GetXAttr( attrs, &handler, timeout );
    if( !st.IsOK() )
      return st;

    std::vector<XAttr> *resp = 0;
    st = MessageUtils::WaitForResponse( &handler, resp );
    if( resp ) result.swap( *resp );
    delete resp;

    return st;
  }

  //------------------------------------------------------------------------
  // Delete extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus File::DelXAttr( const std::vector<std::string>  &attrs,
                               ResponseHandler                 *handler,
                               time_t                           timeout )
  {
    if( pPlugIn )
      return XRootDStatus( stError, errNotSupported );

    return FileStateHandler::DelXAttr( pImpl->pStateHandler, attrs, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Delete extended attributes - sync
  //------------------------------------------------------------------------
  XRootDStatus File::DelXAttr( const std::vector<std::string>  &attrs,
                               std::vector<XAttrStatus>        &result,
                               time_t                           timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = DelXAttr( attrs, &handler, timeout );
    if( !st.IsOK() )
      return st;

    std::vector<XAttrStatus> *resp = 0;
    st = MessageUtils::WaitForResponse( &handler, resp );
    if( resp ) result.swap( *resp );
    delete resp;

    return st;
  }

  //------------------------------------------------------------------------
  // List extended attributes - async
  //------------------------------------------------------------------------
  XRootDStatus File::ListXAttr( ResponseHandler  *handler,
                                time_t            timeout )
  {
    if( pPlugIn )
      return XRootDStatus( stError, errNotSupported );

    return FileStateHandler::ListXAttr( pImpl->pStateHandler, handler, timeout );
  }

  //------------------------------------------------------------------------
  // List extended attributes - sync
  //------------------------------------------------------------------------
  XRootDStatus File::ListXAttr( std::vector<XAttr>  &result,
                                time_t               timeout )
  {
    SyncResponseHandler handler;
    XRootDStatus st = ListXAttr( &handler, timeout );
    if( !st.IsOK() )
      return st;

    std::vector<XAttr> *resp = 0;
    st = MessageUtils::WaitForResponse( &handler, resp );
    if( resp ) result.swap( *resp );
    delete resp;

    return st;
  }

  //------------------------------------------------------------------------
  // Create a checkpoint
  //------------------------------------------------------------------------
  XRootDStatus File::Checkpoint( kXR_char                  code,
                                 ResponseHandler          *handler,
                                 time_t                    timeout )
  {
    if( pPlugIn )
      return XRootDStatus( stError, errNotSupported );

    return FileStateHandler::Checkpoint( pImpl->pStateHandler, code, handler, timeout );
  }

  //------------------------------------------------------------------------
  //! Checkpointed write - async
  //------------------------------------------------------------------------
  XRootDStatus File::ChkptWrt( uint64_t         offset,
                               uint32_t         size,
                               const void      *buffer,
                               ResponseHandler *handler,
                               time_t           timeout )
  {
    if( pPlugIn )
      return XRootDStatus( stError, errNotSupported );

    return FileStateHandler::ChkptWrt( pImpl->pStateHandler, offset, size, buffer, handler, timeout );
  }

  //------------------------------------------------------------------------
  //! Checkpointed WriteV - async
  //------------------------------------------------------------------------
  XRootDStatus File::ChkptWrtV( uint64_t            offset,
                                const struct iovec *iov,
                                int                 iovcnt,
                                ResponseHandler    *handler,
                                time_t              timeout )
  {
    if( pPlugIn )
      return XRootDStatus( stError, errNotSupported );

    return FileStateHandler::ChkptWrtV( pImpl->pStateHandler, offset, iov, iovcnt, handler, timeout );
  }

  //------------------------------------------------------------------------
  // Try different data server
  //------------------------------------------------------------------------
  XRootDStatus File::TryOtherServer( time_t timeout )
  {
    return FileStateHandler::TryOtherServer( pImpl->pStateHandler, timeout );
  }

  //----------------------------------------------------------------------------
  // Check if the file is open
  //----------------------------------------------------------------------------
  bool File::IsOpen() const
  {
    if( pPlugIn )
      return pPlugIn->IsOpen();

    return pImpl->pStateHandler->IsOpen();
  }

  //------------------------------------------------------------------------
  //! Check if the file is using an encrypted connection
  //------------------------------------------------------------------------
  bool File::IsSecure() const
  {
    if( pPlugIn )
      return false;
    return pImpl->pStateHandler->IsSecure();
  }

  //----------------------------------------------------------------------------
  // Set file property
  //----------------------------------------------------------------------------
  bool File::SetProperty( const std::string &name, const std::string &value )
  {
    if( pPlugIn )
      return pPlugIn->SetProperty( name, value );

    return pImpl->pStateHandler->SetProperty( name, value );
  }

  //----------------------------------------------------------------------------
  // Get file property
  //----------------------------------------------------------------------------
  bool File::GetProperty( const std::string &name, std::string &value ) const
  {
    if( pPlugIn )
      return pPlugIn->GetProperty( name, value );

    return pImpl->pStateHandler->GetProperty( name, value );
  }
}
