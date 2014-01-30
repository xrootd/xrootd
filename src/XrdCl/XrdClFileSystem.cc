//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClRequestSync.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClForkHandler.hh"
#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClPlugInManager.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <memory>

namespace
{
  //----------------------------------------------------------------------------
  // Deep locate handler
  //----------------------------------------------------------------------------
  class DeepLocateHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      DeepLocateHandler( XrdCl::ResponseHandler   *handler,
                         const std::string        &path,
                         XrdCl::OpenFlags::Flags   flags,
                         time_t                    expires ):
        pFirstTime( true ),
        pOutstanding( 1 ),
        pHandler( handler ),
        pPath( path ),
        pFlags( flags ),
        pExpires(expires)
      {
        pLocations = new XrdCl::LocationInfo();
      }

      //------------------------------------------------------------------------
      // Destructor
      //------------------------------------------------------------------------
      ~DeepLocateHandler()
      {
        delete pLocations;
      }

      //------------------------------------------------------------------------
      // Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdCl::XRootDStatus *status,
                                   XrdCl::AnyObject    *response )
      {
        XrdSysMutexHelper scopedLock( pMutex );
        using namespace XrdCl;
        Log *log = DefaultEnv::GetLog();
        --pOutstanding;

        //----------------------------------------------------------------------
        // We've got an error, react accordingly
        //----------------------------------------------------------------------
        if( !status->IsOK() )
        {
          log->Dump( FileSystemMsg, "[0x%x@DeepLocate(%s)] Got error "
                     "response: %s", this, pPath.c_str(),
                     status->ToStr().c_str() );

          //--------------------------------------------------------------------
          // We have failed with the first request
          //--------------------------------------------------------------------
          if( pFirstTime )
          {
            log->Debug( FileSystemMsg, "[0x%x@DeepLocate(%s)] Failed to get "
                        "the initial location list: %s", this, pPath.c_str(),
                        status->ToStr().c_str() );
            pHandler->HandleResponse( status, response );
            scopedLock.UnLock();
            delete this;
            return;
          }

          //--------------------------------------------------------------------
          // We have no more outstanding requests, so let give to the client
          // what we have
          //--------------------------------------------------------------------
          if( !pOutstanding )
          {
            log->Debug( FileSystemMsg, "[0x%x@DeepLocate(%s)] No outstanding "
                        "requests, give out what we've got", this,
                        pPath.c_str() );
            scopedLock.UnLock();
            HandleFinalResponse();
          }
          delete status;
          return;
        }
        pFirstTime = false;

        //----------------------------------------------------------------------
        // Extract the answer
        //----------------------------------------------------------------------
        LocationInfo *info = 0;
        response->Get( info );
        LocationInfo::Iterator it;

        log->Dump( FileSystemMsg, "[0x%x@DeepLocate(%s)] Got %d locations",
                   this, pPath.c_str(), info->GetSize() );

        for( it = info->Begin(); it != info->End(); ++it )
        {
          //--------------------------------------------------------------------
          // Add the location to the list
          //--------------------------------------------------------------------
          if( it->IsServer() )
          {
            pLocations->Add( *it );
            continue;
          }

          //--------------------------------------------------------------------
          // Ask the manager for the location of servers
          //--------------------------------------------------------------------
          if( it->IsManager() )
          {
            FileSystem fs( it->GetAddress() );
            if( fs.Locate( pPath, pFlags, this, pExpires-::time(0)).IsOK() )
              ++pOutstanding;
          }
        }

        //----------------------------------------------------------------------
        // Clean up and check if we have anything else to do
        //----------------------------------------------------------------------
        delete response;
        delete status;
        if( !pOutstanding )
        {
          scopedLock.UnLock();
          HandleFinalResponse();
        }
      }

      //------------------------------------------------------------------------
      // Build the response for the client
      //------------------------------------------------------------------------
      void HandleFinalResponse()
      {
        using namespace XrdCl;

        //----------------------------------------------------------------------
        // Nothing found
        //----------------------------------------------------------------------
        if( !pLocations->GetSize() )
        {
          pHandler->HandleResponse( new XRootDStatus( stError, errErrorResponse,
                                                   kXR_NotFound,
                                                   "No valid location found" ),
                                    0 );
        }
        //----------------------------------------------------------------------
        // We return an answer
        //----------------------------------------------------------------------
        else
        {
          AnyObject *obj = new AnyObject();
          obj->Set( pLocations );
          pLocations = 0;
          pHandler->HandleResponse( new XRootDStatus(), obj );
        }
        delete this;
      }

    private:
      bool                      pFirstTime;
      uint16_t                  pOutstanding;
      XrdCl::ResponseHandler   *pHandler;
      XrdCl::LocationInfo      *pLocations;
      std::string               pPath;
      XrdCl::OpenFlags::Flags   pFlags;
      time_t                    pExpires;
      XrdSysMutex               pMutex;
  };

  //----------------------------------------------------------------------------
  // Handle stat results for a dirlist request
  //----------------------------------------------------------------------------
  class DirListStatHandler: public XrdCl::ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor
      //------------------------------------------------------------------------
      DirListStatHandler( XrdCl::DirectoryList *list,
                          uint32_t                  index,
                          XrdCl::RequestSync   *sync ):
        pList( list ),
        pIndex( index ),
        pSync( sync )
      {
      }

      //------------------------------------------------------------------------
      // Check if we were successful and if so put the StatInfo object
      // in the appropriate entry info
      //------------------------------------------------------------------------
      virtual void HandleResponse( XrdCl::XRootDStatus *status,
                                   XrdCl::AnyObject    *response )
      {
        if( !status->IsOK() )
        {
          delete status;
          pSync->TaskDone( false );
          delete this;
          return;
        }

        XrdCl::StatInfo *info = 0;
        response->Get( info );
        response->Set( (char*) 0 );
        pList->At( pIndex )->SetStatInfo( info );
        delete status;
        delete response;
        pSync->TaskDone();
        delete this;
      }

    private:
      XrdCl::DirectoryList *pList;
      uint32_t                  pIndex;
      XrdCl::RequestSync   *pSync;
  };
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Wrapper class used to assign a load balancer
  //----------------------------------------------------------------------------
  class AssignLBHandler: public ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor and destructor
      //------------------------------------------------------------------------
      AssignLBHandler( FileSystem *fs, ResponseHandler *userHandler ):
        pFS(fs), pUserHandler(userHandler) {}

      virtual ~AssignLBHandler() {}

      //------------------------------------------------------------------------
      // Response callback
      //------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XRootDStatus *status,
                                            AnyObject    *response,
                                            HostList     *hostList )
      {
        if( status->IsOK() )
        {
          HostList::reverse_iterator it;
          for( it = hostList->rbegin(); it != hostList->rend(); ++it )
          if( it->loadBalancer )
          {
            pFS->AssignLoadBalancer( it->url );
            break;
          }
        }
        pUserHandler->HandleResponseWithHosts( status, response, hostList );
        delete this;
      }

    private:
      FileSystem      *pFS;
      ResponseHandler *pUserHandler;
  };


  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  FileSystem::FileSystem( const URL &url, bool enablePlugIns ):
    pLoadBalancerLookupDone( false ),
    pPlugIn(0)
  {
    pUrl = new URL( url.GetURL() );

    //--------------------------------------------------------------------------
    // Check if we need to install a plug-in for this URL
    //--------------------------------------------------------------------------
    if( enablePlugIns )
    {
      Log *log = DefaultEnv::GetLog();
      std::string urlStr = url.GetURL();
      PlugInFactory *fact = DefaultEnv::GetPlugInManager()->GetFactory(urlStr);
      if( fact )
      {
        pPlugIn = fact->CreateFileSystem( urlStr );
        if( !pPlugIn )
        {
          log->Error( FileMsg, "Plug-in factory failed to produce a plug-in "
                      "for %s, continuing without one", urlStr.c_str() );
        }
      }
    }

    if( !pPlugIn )
      DefaultEnv::GetForkHandler()->RegisterFileSystemObject( this );
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  FileSystem::~FileSystem()
  {
    if( !pPlugIn )
      DefaultEnv::GetForkHandler()->UnRegisterFileSystemObject( this );
    delete pUrl;
    delete pPlugIn;
  }

  //----------------------------------------------------------------------------
  // Locate a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Locate( const std::string &path,
                                   OpenFlags::Flags   flags,
                                   ResponseHandler   *handler,
                                   uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Locate( path, flags, handler, timeout );

    Message             *msg;
    ClientLocateRequest *req;
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid = kXR_locate;
    req->options   = flags;
    req->dlen      = path.length();
    msg->Append( path.c_str(), path.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Locate a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Locate( const std::string  &path,
                                   OpenFlags::Flags    flags,
                                   LocationInfo      *&response,
                                   uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = Locate( path, flags, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Locate a file, recursively locate all disk servers - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::DeepLocate( const std::string &path,
                                       OpenFlags::Flags   flags,
                                       ResponseHandler   *handler,
                                       uint16_t           timeout )
  {
    return Locate( path, flags,
                   new DeepLocateHandler( handler, path, flags,
                                          ::time(0)+timeout ),
                   timeout );
  }

  //----------------------------------------------------------------------------
  // Locate a file, recursively locate all disk servers - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::DeepLocate( const std::string  &path,
                                       OpenFlags::Flags    flags,
                                       LocationInfo      *&response,
                                       uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = DeepLocate( path, flags, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Move a directory or a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Mv( const std::string &source,
                               const std::string &dest,
                               ResponseHandler   *handler,
                               uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Mv( source, dest, handler, timeout );

    Message         *msg;
    ClientMvRequest *req;
    MessageUtils::CreateRequest( msg, req, source.length()+dest.length()+1 );

    req->requestid = kXR_mv;
    req->dlen      = source.length()+dest.length()+1;
    msg->Append( source.c_str(), source.length(), 24 );
    *msg->GetBuffer(24+source.length()) = ' ';
    msg->Append( dest.c_str(), dest.length(), 25+source.length() );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );

    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Move a directory or a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Mv( const std::string &source,
                               const std::string &dest,
                               uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = Mv( source, dest, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Obtain server information - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Query( QueryCode::Code  queryCode,
                                  const Buffer    &arg,
                                  ResponseHandler *handler,
                                  uint16_t         timeout )
  {
    if( pPlugIn )
      return pPlugIn->Query( queryCode, arg, handler, timeout );

    Message            *msg;
    ClientQueryRequest *req;
    MessageUtils::CreateRequest( msg, req, arg.GetSize() );

    req->requestid = kXR_query;
    req->infotype  = queryCode;
    req->dlen      = arg.GetSize();
    msg->Append( arg.GetBuffer(), arg.GetSize(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Obtain server information - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Query( QueryCode::Code   queryCode,
                                  const Buffer     &arg,
                                  Buffer          *&response,
                                  uint16_t          timeout )
  {
    SyncResponseHandler handler;
    Status st = Query( queryCode, arg, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Truncate a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Truncate( const std::string &path,
                                     uint64_t           size,
                                     ResponseHandler   *handler,
                                     uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Truncate( path, size, handler, timeout );

    Message               *msg;
    ClientTruncateRequest *req;
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid = kXR_truncate;
    req->offset    = size;
    req->dlen      = path.length();
    msg->Append( path.c_str(), path.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Truncate a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Truncate( const std::string &path,
                                     uint64_t           size,
                                     uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = Truncate( path, size, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Remove a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Rm( const std::string &path,
                               ResponseHandler   *handler,
                               uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Rm( path, handler, timeout );

    Message         *msg;
    ClientRmRequest *req;
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid = kXR_rm;
    req->dlen      = path.length();
    msg->Append( path.c_str(), path.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Remove a file - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Rm( const std::string &path,
                               uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = Rm( path, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Create a directory - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::MkDir( const std::string &path,
                                  MkDirFlags::Flags  flags,
                                  Access::Mode       mode,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->MkDir( path, flags, mode, handler, timeout );

    Message            *msg;
    ClientMkdirRequest *req;
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid  = kXR_mkdir;
    req->options[0] = flags;
    req->mode       = mode;
    req->dlen       = path.length();
    msg->Append( path.c_str(), path.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Create a directory - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::MkDir( const std::string &path,
                                  MkDirFlags::Flags  flags,
                                  Access::Mode       mode,
                                  uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = MkDir( path, flags, mode, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Remove a directory - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::RmDir( const std::string &path,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->RmDir( path, handler, timeout );

    Message            *msg;
    ClientRmdirRequest *req;
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid  = kXR_rmdir;
    req->dlen       = path.length();
    msg->Append( path.c_str(), path.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Remove a directory - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::RmDir( const std::string &path,
                                  uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = RmDir( path, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Change access mode on a directory or a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::ChMod( const std::string &path,
                                  Access::Mode       mode,
                                  ResponseHandler   *handler,
                                  uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->ChMod( path, mode, handler, timeout );

    Message            *msg;
    ClientChmodRequest *req;
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid  = kXR_chmod;
    req->mode       = mode;
    req->dlen       = path.length();
    msg->Append( path.c_str(), path.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Change access mode on a directory or a file - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::ChMod( const std::string &path,
                                  Access::Mode       mode,
                                  uint16_t           timeout )
  {
    SyncResponseHandler handler;
    Status st = ChMod( path, mode, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Check if the server is alive - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Ping( ResponseHandler *handler,
                                 uint16_t        timeout )
  {
    if( pPlugIn )
      return pPlugIn->Ping( handler, timeout );

    Message           *msg;
    ClientPingRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid  = kXR_ping;
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Check if the server is alive - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Ping( uint16_t timeout  )
  {
    SyncResponseHandler handler;
    Status st = Ping( &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForStatus( &handler );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for a path - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Stat( const std::string &path,
                                 ResponseHandler   *handler,
                                 uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->Stat( path, handler, timeout );

    Message           *msg;
    ClientStatRequest *req;
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid  = kXR_stat;
    req->options    = 0;
    req->dlen       = path.length();
    msg->Append( path.c_str(), path.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for a path - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Stat( const std::string  &path,
                                 StatInfo          *&response,
                                 uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = Stat( path, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for a path - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::StatVFS( const std::string &path,
                                    ResponseHandler   *handler,
                                    uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->StatVFS( path, handler, timeout );

    Message           *msg;
    ClientStatRequest *req;
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid  = kXR_stat;
    req->options    = kXR_vfs;
    req->dlen       = path.length();
    msg->Append( path.c_str(), path.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Obtain status information for a path - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::StatVFS( const std::string  &path,
                                    StatInfoVFS       *&response,
                                    uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = StatVFS( path, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Obtain server protocol information - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Protocol( ResponseHandler *handler,
                                     uint16_t         timeout )
  {
    if( pPlugIn )
      return pPlugIn->Protocol( handler, timeout );

    Message               *msg;
    ClientProtocolRequest *req;
    MessageUtils::CreateRequest( msg, req );

    req->requestid = kXR_protocol;
    req->clientpv  = kXR_PROTOCOLVERSION;
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // Obtain server protocol information - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Protocol( ProtocolInfo *&response,
                                     uint16_t       timeout )
  {
    SyncResponseHandler handler;
    Status st = Protocol( &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // List entries of a directory - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::DirList( const std::string   &path,
                                    DirListFlags::Flags  flags,
                                    ResponseHandler     *handler,
                                    uint16_t             timeout )
  {
    if( pPlugIn )
      return pPlugIn->DirList( path, flags, handler, timeout );

    Message           *msg;
    ClientStatRequest *req;
    MessageUtils::CreateRequest( msg, req, path.length() );

    req->requestid  = kXR_dirlist;
    req->dlen       = path.length();
    msg->Append( path.c_str(), path.length(), 24 );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  // List entries of a directory - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::DirList( const std::string    &path,
                                    DirListFlags::Flags   flags,
                                    DirectoryList       *&response,
                                    uint16_t              timeout )
  {
    //--------------------------------------------------------------------------
    // We do the deep locate and ask all the returned servers for the list
    //--------------------------------------------------------------------------
    if( flags & DirListFlags::Locate )
    {
      //------------------------------------------------------------------------
      // Locate all the disk servers holding the directory
      //------------------------------------------------------------------------
      LocationInfo *locations;
      std::string locatePath = "*"; locatePath += path;
      XRootDStatus st = DeepLocate( locatePath, OpenFlags::None, locations );

      if( !st.IsOK() )
        return st;

      if( locations->GetSize() == 0 )
      {
        delete locations;
        return XRootDStatus( stError, errNotFound );
      }

      //------------------------------------------------------------------------
      // Ask each server for a directory list
      //------------------------------------------------------------------------
      flags &= ~DirListFlags::Locate;
      FileSystem    *fs;
      DirectoryList *currentResp  = 0;
      uint32_t       errors       = 0;
      uint32_t       numLocations = locations->GetSize();
      bool           partial      = false;

      response = new DirectoryList();
      response->SetParentName( path );

      for( uint32_t i = 0; i < locations->GetSize(); ++i )
      {
        fs = new FileSystem( locations->At(i).GetAddress() );
        st = fs->DirList( path, flags, currentResp, timeout );
        if( !st.IsOK() )
        {
          ++errors;
          delete fs;
          continue;
        }

        if( st.code == suPartial )
          partial = true;

        DirectoryList::Iterator it;

        for( it = currentResp->Begin(); it != currentResp->End(); ++it )
        {
          response->Add( *it );
          *it = 0;
        }

        delete fs;
        delete currentResp;
        fs          = 0;
        currentResp = 0;
      }
      delete locations;

      if( errors || partial )
      {
        if( errors == numLocations )
          return st;
        return XRootDStatus( stOK, suPartial );
      }
      return XRootDStatus();
    };

    //--------------------------------------------------------------------------
    // We just ask the current server
    //--------------------------------------------------------------------------
    SyncResponseHandler handler;
    XRootDStatus st = DirList( path, flags, &handler, timeout );
    if( !st.IsOK() )
      return st;

    st = MessageUtils::WaitForResponse( &handler, response );
    if( !st.IsOK() )
      return st;

    //--------------------------------------------------------------------------
    // Do the stats on all the entries if necessary
    //--------------------------------------------------------------------------
    if( !(flags & DirListFlags::Stat) )
      return st;

    uint32_t quota = response->GetSize() <= 1024 ? response->GetSize() : 1024;
    RequestSync sync( response->GetSize(), quota );
    for( uint32_t i = 0; i < response->GetSize(); ++i )
    {
      std::string fullPath = response->GetParentName()+response->At(i)->GetName();
      ResponseHandler *handler = new DirListStatHandler( response, i, &sync );
      st = Stat( fullPath, handler, timeout );
      if( !st.IsOK() )
      {
        sync.TaskDone( false );
        delete handler;
      }
      sync.WaitForQuota();
    }
    sync.WaitForAll();

    if( sync.FailureCount() )
      return XRootDStatus( stOK, suPartial );

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Send info to the server - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::SendInfo( const std::string &info,
                                     ResponseHandler   *handler,
                                     uint16_t           timeout )
  {
    if( pPlugIn )
      return pPlugIn->SendInfo( info, handler, timeout );

    Message          *msg;
    ClientSetRequest *req;
    const char *prefix    = "monitor info ";
    size_t      prefixLen = strlen( prefix );
    MessageUtils::CreateRequest( msg, req, info.length()+prefixLen );

    req->requestid  = kXR_set;
    req->dlen       = info.length()+prefixLen;
    msg->Append( prefix, prefixLen, 24 );
    msg->Append( info.c_str(), info.length(), 24+prefixLen );
    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //----------------------------------------------------------------------------
  //! Send info to the server - sync
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::SendInfo( const std::string  &info,
                                     Buffer            *&response,
                                     uint16_t            timeout )
  {
    SyncResponseHandler handler;
    Status st = SendInfo( info, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Prepare one or more files for access - async
  //----------------------------------------------------------------------------
  XRootDStatus FileSystem::Prepare( const std::vector<std::string> &fileList,
                                    PrepareFlags::Flags             flags,
                                    uint8_t                         priority,
                                    ResponseHandler                *handler,
                                    uint16_t                        timeout )
  {
    if( pPlugIn )
      return pPlugIn->Prepare( fileList, flags, priority, handler, timeout );

    std::vector<std::string>::const_iterator it;
    std::string                              list;
    for( it = fileList.begin(); it != fileList.end(); ++it )
    {
      list += *it;
      list += "\n";
    }
    list.erase( list.length()-1, 1 );

    Message              *msg;
    ClientPrepareRequest *req;
    MessageUtils::CreateRequest( msg, req, list.length() );

    req->requestid  = kXR_prepare;
    req->options    = flags;
    req->prty       = priority;
    req->dlen       = list.length();

    msg->Append( list.c_str(), list.length(), 24 );

    MessageSendParams params; params.timeout = timeout;
    MessageUtils::ProcessSendParams( params );
    XRootDTransport::SetDescription( msg );

    return Send( msg, handler, params );
  }

  //------------------------------------------------------------------------
  //! Prepare one or more files for access - sync
  //------------------------------------------------------------------------
  XRootDStatus FileSystem::Prepare( const std::vector<std::string>  &fileList,
                                    PrepareFlags::Flags              flags,
                                    uint8_t                          priority,
                                    Buffer                         *&response,
                                    uint16_t                         timeout )
  {
    SyncResponseHandler handler;
    Status st = Prepare( fileList, flags, priority, &handler, timeout );
    if( !st.IsOK() )
      return st;

    return MessageUtils::WaitForResponse( &handler, response );
  }

  //----------------------------------------------------------------------------
  // Assign a load balancer if it has not already been assigned
  //----------------------------------------------------------------------------
  void FileSystem::AssignLoadBalancer( const URL &url )
  {
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    if( pLoadBalancerLookupDone )
      return;

    log->Dump( FileSystemMsg, "[0x%x@%s] Assigning %s as load balancer", this,
               pUrl->GetHostId().c_str(), url.GetHostId().c_str() );

    delete pUrl;
    pUrl = new URL( url );
    pLoadBalancerLookupDone = true;
  }

  //----------------------------------------------------------------------------
  // Send a message in a locked environment
  //----------------------------------------------------------------------------
  Status FileSystem::Send( Message                 *msg,
                           ResponseHandler         *handler,
                           const MessageSendParams &params )
  {
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    log->Dump( FileSystemMsg, "[0x%x@%s] Sending %s", this,
               pUrl->GetHostId().c_str(), msg->GetDescription().c_str() );

    if( !pLoadBalancerLookupDone )
      handler = new AssignLBHandler( this, handler );

    return MessageUtils::SendMessage( *pUrl, msg, handler, params );
  }
}
