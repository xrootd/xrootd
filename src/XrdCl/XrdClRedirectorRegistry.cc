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

RedirectorRegistry& RedirectorRegistry::Instance()
{
  static RedirectorRegistry redirector;
  return redirector;
}

RedirectorRegistry::~RedirectorRegistry()
{
  RedirectorMap::iterator itr;
  for( itr = pRegistry.begin(); itr != pRegistry.end(); ++itr )
    delete itr->second.first;
}

XRootDStatus RedirectorRegistry::RegisterImpl( const URL &url, ResponseHandler *handler )
{
  // we can only create a virtual redirector if
  // a path to a metadata file has been provided
  if( url.GetPath().empty() ) return XRootDStatus( stError, errNotSupported );
  XrdSysMutexHelper scopedLock( pMutex );
  // get the key and check if it is already in the registry
  const std::string key = url.GetLocation();
  RedirectorMap::iterator itr = pRegistry.find( key );
  if( itr != pRegistry.end() )
  {
    // increment user counter
    ++itr->second.second;
    if( handler ) handler->HandleResponseWithHosts( new XRootDStatus(), 0, 0 );
    return XRootDStatus( stOK, suAlreadyDone );
  }
  // If it is a Metalink create a MetalinkRedirector
  if( url.IsMetalink() )
  {
    MetalinkRedirector *redirector = new MetalinkRedirector( key );
    XRootDStatus st = redirector->Load( handler );
    if( !st.IsOK() )
      delete redirector;
    else
      pRegistry[key] = std::pair<VirtualRedirector*, size_t>( redirector, 1 );
    return st;
  }
  else
    // so far we only support Metalink metadata format
    return XRootDStatus( stError, errNotSupported );
}

XRootDStatus RedirectorRegistry::Register( const URL &url )
{
  return RegisterImpl( url, 0 );
}

XRootDStatus RedirectorRegistry::RegisterAndWait( const URL &url )
{
  SyncResponseHandler handler;
  Status st = RegisterImpl( url, &handler );
  if( !st.IsOK() ) return st;
  return MessageUtils::WaitForStatus( &handler );
}

VirtualRedirector* RedirectorRegistry::Get( const URL &url ) const
{
  XrdSysMutexHelper scopedLock( pMutex );
  // get the key and return the value if it is in the registry
  // offset 24 is where the path has been stored
  const std::string key = url.GetLocation();
  RedirectorMap::const_iterator itr = pRegistry.find( key );
  if( itr != pRegistry.end() )
    return itr->second.first;
  // otherwise return null
  return 0;
}

//----------------------------------------------------------------------------
// Release the virtual redirector associated with the given URL
//----------------------------------------------------------------------------
void RedirectorRegistry::Release( const URL &url )
{
  XrdSysMutexHelper scopedLock( pMutex );
  // get the key and return the value if it is in the registry
  // offset 24 is where the path has been stored
  const std::string key = url.GetLocation();
  RedirectorMap::iterator itr = pRegistry.find( key );
  if( itr == pRegistry.end() ) return;
  // decrement user counter
  --itr->second.second;
  // if nobody is using it delete the object
  // and remove it from the registry
  if( !itr->second.second )
  {
    delete itr->second.first;
    pRegistry.erase( itr );
  }
}

} /* namespace XrdCl */
