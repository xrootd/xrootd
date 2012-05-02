//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClUtils.hh"

#include <cstdlib>
#include <vector>
#include <sstream>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  URL::URL( const std::string &url, int port  ):
    pIsValid( false ), pUrl( url ), pPort( port )
  {
    ParseUrl();
  }

  //----------------------------------------------------------------------------
  // Parse URL - it is rather trivial and horribly slow but probably there
  // is not need to have anything more fancy
  //----------------------------------------------------------------------------
  void URL::ParseUrl()
  {
    Log *log = DefaultEnv::GetLog();

    if( pUrl.length() == 0 )
    {
      log->Error( UtilityMsg, "The given URL is empty" );
      return;
    }

    //--------------------------------------------------------------------------
    // Flags - if they are true and the corresponding field is empty
    // or uninitialized then the URL is malformed
    //--------------------------------------------------------------------------
    bool hasProtocol = false;
    bool hasUserName = false;
    bool hasPassword = false;
    bool hasPath     = false;

    //--------------------------------------------------------------------------
    // Extract the protocol
    //--------------------------------------------------------------------------
    size_t pos          = pUrl.find( "://" );
    size_t currentStart = 0;
    if( pos != std::string::npos )
    {
      pProtocol = pUrl.substr( 0, pos );
      currentStart = pos+3;
      hasProtocol = true;
    }

    //--------------------------------------------------------------------------
    // Separate the user-pass-host-port parts
    //--------------------------------------------------------------------------
    pos = pUrl.find( "/", currentStart );
    if( pos != std::string::npos )
      hasPath = true;

    std::string userHost = pUrl.substr( currentStart, pos-currentStart );
    currentStart = pos+1;

    //--------------------------------------------------------------------------
    // Do we have username and password?
    //--------------------------------------------------------------------------
    pos = userHost.find( "@" );
    std::string hostPort;
    if( pos != std::string::npos )
    {
      std::string userPass = userHost.substr( 0, pos );
      hostPort = userHost.substr( pos+1, userHost.length() );
      pos = userPass.find( ":" );
      if( pos != std::string::npos )
      {
        pUserName = userPass.substr( 0, pos );
        pPassword = userPass.substr( pos+1, userPass.length() );
        hasPassword = true;
      }
      else
        pUserName = userPass;
      hasUserName = true;
    }
    else
      hostPort = userHost;

    //--------------------------------------------------------------------------
    // Deal with hostname - IPv6 encoded address RFC 2732
    //--------------------------------------------------------------------------
    if( hostPort.length() >= 3 && hostPort[0] == '[' )
    {
      pos = hostPort.find( "]" );
      if( pos != std::string::npos )
      {
        pHostName = hostPort.substr( 0, pos+1 );
        hostPort.erase( 0, pos+2 );

        //----------------------------------------------------------------------
        // Check if we're IPv6 encoded IPv4
        //----------------------------------------------------------------------
        pos = pHostName.find( "." );
        if( pos != std::string::npos )
        {
          pHostName.erase( 0, 3 );
          pHostName.erase( pHostName.length()-1, 1 );
        }
      }
    }
    else
    {
      pos = hostPort.find( ":" );
      if( pos != std::string::npos )
      {
        pHostName = hostPort.substr( 0, pos );
        hostPort.erase( 0, pos+1 );
      }
      else
      {
        pHostName = hostPort;
        hostPort  = "";
      }

    }

    //--------------------------------------------------------------------------
    // Deal with port number
    //--------------------------------------------------------------------------
    if( !hostPort.empty() )
    {
      char *result;
      pPort = ::strtol( hostPort.c_str(), &result, 0 );
      if( *result != 0 )
        pPort = -1;
    }

    //--------------------------------------------------------------------------
    // Deal with the path
    //--------------------------------------------------------------------------
    if( hasPath )
    {
      pPathWithParams = pUrl.substr( currentStart, pUrl.length() );
      pos = pPathWithParams.find( "?" );
      if( pos != std::string::npos )
      {
        pPath = pPathWithParams.substr( 0, pos );
        std::string paramsStr
          = pPathWithParams.substr( pos+1, pPathWithParams.length() );

        //----------------------------------------------------------------------
        // Parse parameters
        //----------------------------------------------------------------------
        std::vector<std::string>           params;
        std::vector<std::string>::iterator it;
        Utils::splitString( params, paramsStr, "&" );
        for( it = params.begin(); it != params.end(); ++it )
        {
          pos = it->find( "=" );
          if( pos == std::string::npos )
            pParams[*it] = "";
          else
            pParams[it->substr(0, pos)] = it->substr( pos+1, it->length() );
        }
      }
      else
        pPath = pPathWithParams;
    }

    //--------------------------------------------------------------------------
    // Create the host id
    //--------------------------------------------------------------------------
    std::ostringstream o;
    if( pUserName.length() )
      o << pUserName << "@";
    o << pHostName << ":" << pPort;
    pHostId = o.str();

    //--------------------------------------------------------------------------
    // Dump the url
    //--------------------------------------------------------------------------
    log->Dump( UtilityMsg,
               "URL: %s\n"
               "Protocol:  %s\n"
               "User Name: %s\n"
               "Password:  %s\n"
               "Host Name: %s\n"
               "Port:      %d\n"
               "Path:      %s\n"
               "Full path: %s",
               pUrl.c_str(), pProtocol.c_str(), pUserName.c_str(),
               pPassword.c_str(), pHostName.c_str(), pPort, pPath.c_str(),
               pPathWithParams.c_str() );

    //--------------------------------------------------------------------------
    // Check validity
    //--------------------------------------------------------------------------
    if( hasProtocol && pProtocol.length() == 0 ) return;
    if( hasUserName && pUserName.length() == 0 ) return;
    if( hasPassword && pPassword.length() == 0 ) return;
    if( pHostName.length() == 0 ) return;
    if( pPort == -1 ) return;

    pIsValid = true;

  }
}
