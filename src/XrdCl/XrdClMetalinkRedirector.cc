/*
 * XrdClMetalinkRedirector.cc
 *
 *  Created on: May 2, 2016
 *      Author: simonm
 */

#include "XrdClMetalinkRedirector.hh"

#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClFileStateHandler.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClPostMaster.hh"

#include "XrdXml/XrdXmlMetaLink.hh"

#include "XProtocol/XProtocol.hh"

#include <arpa/inet.h>
#include <fcntl.h>

namespace XrdCl
{

  void DeallocArgs( XRootDStatus *status, AnyObject *response,
      HostList *hostList )
  {
    delete status;
    delete response;
    delete hostList;
  }

  //----------------------------------------------------------------------------
  // Read metalink response handler.
  //
  // If the whole file has been read triggers parsing and
  // initializes the Redirector, otherwise triggers another
  // read.
  //----------------------------------------------------------------------------
  class MetalinkReadHandler: public ResponseHandler
  {
    public:
      //----------------------------------------------------------------------------
      // Constructor
      //----------------------------------------------------------------------------
      MetalinkReadHandler( MetalinkRedirector *mr, ResponseHandler *userHandler,
          const std::string &content = "" ) :
          pRedirector( mr ), pUserHandler( userHandler ), pBuffer(
              new char[DefaultCPChunkSize] ), pContent( content )
      {
      }

      //----------------------------------------------------------------------------
      // Destructor
      //----------------------------------------------------------------------------
      virtual ~MetalinkReadHandler()
      {
        delete[] pBuffer;
      }

      //----------------------------------------------------------------------------
      // Handle the response
      //----------------------------------------------------------------------------
      virtual void HandleResponse( XRootDStatus *status, AnyObject *response )
      {
        try
        {
          // check the status
          if( !status->IsOK() )
            throw status;
          // we don't need it anymore
          delete status;
          // make sure we got a response
          if( !response )
            throw new XRootDStatus( stError, errInternal );
          // make sure the response is not empty
          ChunkInfo * info = 0;
          response->Get( info );
          if( !info )
            throw new XRootDStatus( stError, errInternal );
          uint32_t bytesRead = info->length;
          uint64_t offset = info->offset + bytesRead;
          pContent += std::string( pBuffer, bytesRead );
          // are we done ?
          if( bytesRead > 0 )
          {
            // lets try to read another chunk
            MetalinkReadHandler * mrh = new MetalinkReadHandler( pRedirector,
                pUserHandler, pContent );
            XRootDStatus st = pRedirector->pFile->Read( offset,
                DefaultCPChunkSize, mrh->GetBuffer(), mrh );
            if( !st.IsOK() )
            {
              delete mrh;
              throw new XRootDStatus( st );
            }
          }
          else // we have the whole metalink file
          {
            // we don't need the File object anymore
            delete pRedirector->pFile;
            pRedirector->pFile = 0;
            // now we can parse the metalink file
            XRootDStatus st = pRedirector->Parse( pContent );
            // now with the redirector fully initialized we can handle pending requests
            pRedirector->FinalizeInitialization();
            // we are done, pass the status to the user (whatever it is)
            if( pUserHandler )
              pUserHandler->HandleResponse( new XRootDStatus( st ), 0 );
          }
          // clean up
          delete response;
        }
        catch( XRootDStatus *status )
        {
          pRedirector->FinalizeInitialization( *status );
          // if we were not able to read from the metalink,
          // propagate the error to the user handler
          if( pUserHandler )
            pUserHandler->HandleResponse( status, 0 );
          else
            DeallocArgs( status, response, 0 );
        }

        delete this;
      }

      //----------------------------------------------------------------------------
      // Get the receive-buffer
      //----------------------------------------------------------------------------
      char * GetBuffer()
      {
        return pBuffer;
      }

    private:

      MetalinkRedirector *pRedirector;
      ResponseHandler *pUserHandler;
      char *pBuffer;
      std::string pContent;
  };

  //----------------------------------------------------------------------------
  // Open metalink response handler.
  //
  // After successful open trrigers a read.
  //----------------------------------------------------------------------------
  class MetalinkOpenHandler: public ResponseHandler
  {
    public:
      //----------------------------------------------------------------------------
      // Constructor
      //----------------------------------------------------------------------------
      MetalinkOpenHandler( MetalinkRedirector *mr,
          ResponseHandler *userHandler ) :
          pRedirector( mr ), pUserHandler( userHandler )
      {
      }

      //----------------------------------------------------------------------------
      // Handle the response
      //----------------------------------------------------------------------------
      virtual void HandleResponseWithHosts( XRootDStatus *status,
          AnyObject *response, HostList *hostList )
      {
        try
        {
          if( status->IsOK() )
          {
            delete status;
            // download the content
            MetalinkReadHandler *mrh = new MetalinkReadHandler( pRedirector,
                pUserHandler );
            XRootDStatus st = pRedirector->pFile->Read( 0, DefaultCPChunkSize,
                mrh->GetBuffer(), mrh );
            if( !st.IsOK() )
            {
              delete mrh;
              throw new XRootDStatus( stError, errInternal );
            } else
            {
              delete response;
              delete hostList;
            }
          } else
            throw status;
        } catch( XRootDStatus *status )
        {
          pRedirector->FinalizeInitialization( *status );
          // if we were not able to schedule a read
          // pass an error to the user handler
          if( pUserHandler )
            pUserHandler->HandleResponseWithHosts( status, response, hostList );
          else
            DeallocArgs( status, response, hostList );
        }

        delete this;
      }

    private:

      MetalinkRedirector *pRedirector;
      ResponseHandler *pUserHandler;
  };

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  MetalinkRedirector::MetalinkRedirector( const std::string & url ) :
      pUrl( url ), pFile( new File( File::DisableVirtRedirect ) ), pReady(
          false ), pFileSize( -1 )
  {
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  MetalinkRedirector::~MetalinkRedirector()
  {
    delete pFile;
  }

  //----------------------------------------------------------------------------
  // Initializes the object with the content of the metalink file
  //----------------------------------------------------------------------------
  XRootDStatus MetalinkRedirector::Load( ResponseHandler *userHandler )
  {
    MetalinkOpenHandler *handler = new MetalinkOpenHandler( this, userHandler );
    XRootDStatus st = pFile->Open( pUrl, OpenFlags::Read, Access::None, handler,
        0 );
    if( !st.IsOK() )
      delete handler;

    return st;
  }

  //----------------------------------------------------------------------------
  // Parses the metalink file
  //----------------------------------------------------------------------------
  XRootDStatus MetalinkRedirector::Parse( const std::string &metalink )
  {
    Log *log = DefaultEnv::GetLog();
    Env *env = DefaultEnv::GetEnv();
    std::string glfnRedirector;
    env->GetString( "GlfnRedirector", glfnRedirector );
    // parse the metalink
    XrdXmlMetaLink parser( "root:xroot:file:", "xroot:",
        glfnRedirector.empty() ? 0 : glfnRedirector.c_str() );
    int size = 0;
    XrdOucFileInfo **fileInfos = parser.ConvertAll( metalink.c_str(), size,
        metalink.size() );
    if( !fileInfos )
    {
      int ecode;
      const char * etxt = parser.GetStatus( ecode );
      log->Error( UtilityMsg,
          "Failed to parse the metalink file: %s (error code: %d)", etxt,
          ecode );
      return XRootDStatus( stError, errDataError, 0,
          "Malformed or corrupted metalink file." );
    }
    // we are expecting just one file per metalink (as agreed with RUCIO)
    if( size != 1 )
    {
      log->Error( UtilityMsg, "Expected only one file per metalink." );
      return XRootDStatus( stError, errDataError );
    }

    InitCksum( fileInfos );
    InitReplicas( fileInfos );
    pTarget = fileInfos[0]->GetTargetName();
    pFileSize = fileInfos[0]->GetSize();

    XrdXmlMetaLink::DeleteAll( fileInfos, size );

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Finalize the initialization process:
  // - mark as ready
  // - setup the status
  // - and handle pending redirects
  //----------------------------------------------------------------------------
  void MetalinkRedirector::FinalizeInitialization( const XRootDStatus &status )
  {
    XrdSysMutexHelper scopedLock( pMutex );
    pReady = true;
    pStatus = status;
    // Handle pending redirects (those that were
    // submitted before the metalink has been loaded)
    while( !pPendingRedirects.empty() )
    {
      const Message *msg = pPendingRedirects.front().first;
      IncomingMsgHandler *handler = pPendingRedirects.front().second;
      pPendingRedirects.pop_front();
      if( !handler || !msg )
        continue;
      HandleRequestImpl( msg, handler );
    }
  }

  //----------------------------------------------------------------------------
  // Generates redirect response for the given request
  //----------------------------------------------------------------------------
  Message* MetalinkRedirector::GetResponse( const Message *msg ) const
  {
    if( !pStatus.IsOK() )
      return GetErrorMsg( msg, "Could not load the Metalink file.",
          static_cast<XErrorCode>( XProtocol::mapError( pStatus.errNo ) ) );
    const ClientRequestHdr *req =
        reinterpret_cast<const ClientRequestHdr*>( msg->GetBuffer() );
    // get the redirect location
    std::string replica;
    if( !GetReplica( msg, replica ).IsOK() )
      return GetErrorMsg( msg, "Metalink: no more replicas to try.", kXR_noReplicas );
    Message *resp = new Message( sizeof(ServerResponse) );
    ServerResponse* response =
        reinterpret_cast<ServerResponse*>( resp->GetBuffer() );
    response->hdr.status = kXR_redirect;
    response->hdr.streamid[0] = req->streamid[0];
    response->hdr.streamid[1] = req->streamid[1];
    response->hdr.dlen = 4 + replica.size(); // 4 bytes are reserved for port number
    response->body.redirect.port = -1; // this indicates that the full URL will be given in the 'host' field
    memcpy( response->body.redirect.host, replica.c_str(), replica.size() );
    return resp;
  }

  //----------------------------------------------------------------------------
  // Generates error response for the given request
  //----------------------------------------------------------------------------
  Message* MetalinkRedirector::GetErrorMsg( const Message *msg,
      const std::string &errMsg, XErrorCode code ) const
  {
    const ClientRequestHdr *req =
        reinterpret_cast<const ClientRequestHdr*>( msg->GetBuffer() );

    Message* resp = new Message( sizeof(ServerResponse) );
    ServerResponse* response =
        reinterpret_cast<ServerResponse*>( resp->GetBuffer() );

    response->hdr.status = kXR_error;
    response->hdr.streamid[0] = req->streamid[0];
    response->hdr.streamid[1] = req->streamid[1];
    response->hdr.dlen = 4 + errMsg.size();
    response->body.error.errnum = htonl( code );
    memcpy( response->body.error.errmsg, errMsg.c_str(), errMsg.size() );

    return resp;
  }

  //----------------------------------------------------------------------------
  // Creates an instant redirect response for the given message
  // or an error response if there are no more replicas to try.
  // The virtual response is being handled by the given handler.
  //----------------------------------------------------------------------------
  XRootDStatus MetalinkRedirector::HandleRequestImpl( const Message *msg,
      IncomingMsgHandler *handler )
  {
    Message *resp = GetResponse( msg );
    JobManager *jobMan = DefaultEnv::GetPostMaster()->GetJobManager();
    RedirectJob *job = new RedirectJob( handler );
    jobMan->QueueJob( job, resp );
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // If the MetalinkRedirector is initialized creates an instant
  // redirect response, otherwise queues the request until initialization
  // is done.
  //----------------------------------------------------------------------------
  XRootDStatus MetalinkRedirector::HandleRequest( const Message *msg,
      IncomingMsgHandler *handler )
  {
    XrdSysMutexHelper scopedLck( pMutex );
    // if the metalink data haven't been loaded yet, make it pending
    if( !pReady )
    {
      pPendingRedirects.push_back(
          std::make_pair( msg, handler ) );;
      return XRootDStatus();
    }
    // otherwise generate a virtual response
    return HandleRequestImpl( msg, handler );
  }

  //----------------------------------------------------------------------------
  //! Count how many replicas do we have left to try for given request
  //----------------------------------------------------------------------------
  int MetalinkRedirector::Count( Message *req ) const
  {
    ReplicaList::const_iterator itr = GetReplica( req );
    return pReplicas.end() - itr;
  }

  //----------------------------------------------------------------------------
  // Gets the file checksum if specified in the metalink
  //----------------------------------------------------------------------------
  void MetalinkRedirector::InitCksum( XrdOucFileInfo **fileInfos )
  {
    const char *chvalue = 0, *chtype = 0;
    while( ( chtype = fileInfos[0]->GetDigest( chvalue ) ) )
    {
      pChecksums[chtype] = chvalue;
    }
  }

  //----------------------------------------------------------------------------
  // Initializes replica list
  //----------------------------------------------------------------------------
  void MetalinkRedirector::InitReplicas( XrdOucFileInfo **fileInfos )
  {
    URL replica;
    const char *url = 0;
    while( ( url = fileInfos[0]->GetUrl() ) )
    {
      replica = URL( url );
      if( !replica.IsValid() || replica.GetURL().size() > 4096 )
        continue; // this is the internal limit (defined in the protocol)
      pReplicas.push_back( replica.GetURL() );
    }
  }

  //----------------------------------------------------------------------------
  //! Get the next replica for the given message
  //----------------------------------------------------------------------------
  XRootDStatus MetalinkRedirector::GetReplica( const Message *msg,
      std::string &replica ) const
  {
    ReplicaList::const_iterator itr = GetReplica( msg );
    if( itr == pReplicas.end() )
      return XRootDStatus( stError, errNotFound );

    replica = *itr;
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  //! Get the next replica for the given message
  //----------------------------------------------------------------------------
  MetalinkRedirector::ReplicaList::const_iterator
  MetalinkRedirector::GetReplica( const Message *msg ) const
  {
    if( pReplicas.empty() )
      return pReplicas.cend();

    std::string tried;
    if( !GetCgiInfo( msg, "tried", tried ).IsOK() )
      return pReplicas.cbegin();

    ReplicaList triedList;
    Utils::splitString( triedList, tried, "," );
    std::set<std::string> triedSet( triedList.begin(), triedList.end() );

    ReplicaList::const_iterator itr = pReplicas.begin();
    for( ; itr != pReplicas.end(); ++itr )
    {
      URL url( *itr );
      if( !triedSet.count( url.GetHostName() ) ) break;
    }

    return itr;
  }

  //----------------------------------------------------------------------------
  //! Extracts an element from url cgi
  //----------------------------------------------------------------------------
  XRootDStatus MetalinkRedirector::GetCgiInfo( const Message *msg,
      const std::string &key, std::string &value ) const
  {
    const ClientRequestHdr *req =
        reinterpret_cast<const ClientRequestHdr*>( msg->GetBuffer() );
    kXR_int32 dlen = msg->IsMarshalled() ? ntohl( req->dlen ) : req->dlen;
    std::string url( msg->GetBuffer( 24 ), dlen );
    size_t pos = url.find( '?' );
    if( pos == std::string::npos )
      return XRootDStatus( stError );
    size_t start = url.find( key, pos );
    if( start == std::string::npos )
      return XRootDStatus( stError );
    start += key.size() + 1; // the +1 stands for the '=' sign that is not part of the key
    size_t end = url.find( '&', start );
    if( end == std::string::npos )
      end = url.size();
    value = url.substr( start, end - start );
    return XRootDStatus();
  }

} /* namespace XrdCl */
