/*
 * XrdClRedirectorRegister.cc
 *
 *  Created on: May 23, 2016
 *      Author: simonm
 */

#include "XrdClRedirectorRegistry.hh"

#include "XrdCl/XrdClMetalinkRedirector.hh"

#include <arpa/inet.h>

namespace XrdCl
{

void DeallcArgs( XRootDStatus *status, AnyObject *response, HostList *hostList )
{
  delete status;
  delete response;
  delete hostList;
}

class LoadHandler : public ResponseHandler
{
  public:

    LoadHandler( ResponseHandler *userHandler, const URL &url) : pUserHandler( userHandler ), pUrl( url ) {}

    virtual void HandleResponseWithHosts( XRootDStatus *status, AnyObject *response, HostList *hostList )
    {
      if( !status->IsOK() )
      {
        // remove the respective redirector from registry
        RedirectorRegistry &registry   = RedirectorRegistry::Instance();
        VirtualRedirector  *redirector = registry.Get( pUrl );
        delete redirector;
        registry.Erase( pUrl );
        // call the user handler if present
        if( pUserHandler ) pUserHandler->HandleResponseWithHosts( status, response, hostList );
        // otherwise deallocate the function arguments
        else DeallcArgs( status, response, hostList );
      }
      else DeallcArgs( status, response, hostList );

      delete this;
    }

  private:

    ResponseHandler *pUserHandler;
    const URL pUrl;
};

class SyncLoadHandler : public ResponseHandler
{
  public:

    SyncLoadHandler( SyncResponseHandler *syncHandler, const URL &url) : pSyncHandler( syncHandler ), pUrl( url ) {}

    virtual void HandleResponseWithHosts( XRootDStatus *status, AnyObject *response, HostList *hostList )
    {
      if( !status->IsOK() )
      {
        // remove the respective redirector from registry
        RedirectorRegistry &registry   = RedirectorRegistry::Instance();
        VirtualRedirector  *redirector = registry.Get( pUrl );
        delete redirector;
        registry.Erase( pUrl );
      }
      // call the sync handler if present
      if( pSyncHandler ) pSyncHandler->HandleResponseWithHosts( status, response, hostList );
      // otherwise deallocate the function arguments
      else DeallcArgs( status, response, hostList );

      delete this;
    }

  private:

    SyncResponseHandler *pSyncHandler;
    const URL pUrl;
};

RedirectorRegistry& RedirectorRegistry::Instance()
{
  static RedirectorRegistry redirector;
  return redirector;
}

RedirectorRegistry::~RedirectorRegistry()
{
  std::map< std::string, VirtualRedirector* >::iterator itr;
  for( itr = pRegistry.begin(); itr != pRegistry.end(); ++itr )
    delete itr->second;
}

XRootDStatus RedirectorRegistry::RegisterImpl( const URL &url, ResponseHandler *handler )
{
  XrdSysMutexHelper scopedLock( pMutex );
  // get the key and check if it is already in the registry
  const std::string key = url.GetLocation();
  if( pRegistry.find( key ) != pRegistry.end() ) return XRootDStatus();
  // If it is a Metalink create a MetalinkRedirector
  if( url.IsMetalink() )
  {
    MetalinkRedirector *redirector = new MetalinkRedirector( key );
    XRootDStatus st = redirector->Load( handler );
    if( !st.IsOK() )
      delete redirector;
    else
      pRegistry[key] = redirector;
    return st;
  }
  else
    // so far we only support Metalink metadata format
    return XRootDStatus( stError, errNotSupported );
}

XRootDStatus RedirectorRegistry::Register( const URL &url, ResponseHandler *handler )
{
  LoadHandler *loadHandler = new LoadHandler(handler, url );
  XRootDStatus st = RegisterImpl( url, loadHandler );
  if( !st.IsOK() ) delete loadHandler;
  return st;
}

XRootDStatus RedirectorRegistry::Register( const URL &url )
{
  SyncResponseHandler handler;
  SyncLoadHandler *syncHandler = new SyncLoadHandler( &handler, url );
  Status st = RegisterImpl( url, syncHandler );
  if( !st.IsOK() )
  {
    delete syncHandler;
    return st;
  }

  return MessageUtils::WaitForStatus( &handler );
}

void RedirectorRegistry::Erase( const URL &url )
{
  XrdSysMutexHelper scopedLock( pMutex );
  pRegistry.erase( url.GetURL() );
}

VirtualRedirector* RedirectorRegistry::Get( const URL &url ) const
{
  XrdSysMutexHelper scopedLock( pMutex );
  // get the key and return the value if it is in the registry
  // offset 24 is where the path has been stored
  const std::string key = url.GetLocation();
  std::map< std::string, VirtualRedirector* >::const_iterator itr = pRegistry.find( key );
  if( itr != pRegistry.end() )
    return itr->second;
  // otherwise return null
  return 0;
}

} /* namespace XrdCl */
