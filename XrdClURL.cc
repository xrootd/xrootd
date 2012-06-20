//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
    pIsValid( true ), pUrl( url ), pPort( port )
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
    // Extract the protocol, assume file:// if none found
    //--------------------------------------------------------------------------
    size_t pos          = pUrl.find( "://" );

    std::string current;
    if( pos != std::string::npos )
    {
      pProtocol = pUrl.substr( 0, pos );
      current   = pUrl.substr( pos+3, pUrl.length()-pos-3 );
    }
    else if( pUrl[0] == '/' )
    {
      pProtocol = "file";
      current   = pUrl;
    }
    else
    {
      pProtocol = "root";
      current   = pUrl;
    }

    //--------------------------------------------------------------------------
    // Extract host info and path
    //--------------------------------------------------------------------------
    std::string path;
    std::string hostInfo;

    if( pProtocol == "file" )
      path = current;
    else
    {
      pos = current.find( "/" );
      if( pos == std::string::npos )
        hostInfo = current;
      else
      {
        hostInfo = current.substr( 0, pos );
        path     = current.substr( pos+1, current.length()-pos );
      }
    }

    if( !ParseHostInfo( hostInfo ) )
      pIsValid = false;

    if( !ParsePath( path ) )
      pIsValid = false;

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
  }

  //----------------------------------------------------------------------------
  // Parse host info
  //----------------------------------------------------------------------------
  bool URL::ParseHostInfo( const std::string hostInfo )
  {
    if( pProtocol == "file" )
      return true;

    if( pProtocol.empty() || hostInfo.empty() )
      return false;

    size_t pos = hostInfo.find( "@" );
    std::string hostPort;

    //--------------------------------------------------------------------------
    // We have found username-password
    //--------------------------------------------------------------------------
    if( pos != std::string::npos )
    {
      std::string userPass = hostInfo.substr( 0, pos );
      hostPort = hostInfo.substr( pos+1, hostInfo.length() );
      pos = userPass.find( ":" );

      //------------------------------------------------------------------------
      // It's both username and password
      //------------------------------------------------------------------------
      if( pos != std::string::npos )
      {
        pUserName = userPass.substr( 0, pos );
        pPassword = userPass.substr( pos+1, userPass.length() );
        if( pPassword.empty() )
          return false;
      }
      //------------------------------------------------------------------------
      // It's just the user name
      //------------------------------------------------------------------------
      else
        pUserName = userPass;
      if( pUserName.empty() )
        return false;
    }

    //--------------------------------------------------------------------------
    // No username-password
    //--------------------------------------------------------------------------
    else
      hostPort = hostInfo;

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
      if( pHostName.empty() )
        return false;
    }

    //--------------------------------------------------------------------------
    // Deal with port number
    //--------------------------------------------------------------------------
    if( !hostPort.empty() )
    {
      char *result;
      pPort = ::strtol( hostPort.c_str(), &result, 0 );
      if( *result != 0 )
        return false;
    }

    //--------------------------------------------------------------------------
    // Create the host id
    //--------------------------------------------------------------------------
    std::ostringstream o;
    if( pUserName.length() )
      o << pUserName << "@";
    o << pHostName << ":" << pPort;
    pHostId = o.str();

    return true;
  }

  //----------------------------------------------------------------------------
  // Parse path
  //----------------------------------------------------------------------------
  bool URL::ParsePath( const std::string &path )
  {
    pPathWithParams = path;
    size_t pos = pPathWithParams.find( "?" );
    if( pos != std::string::npos )
    {
      pPath = pPathWithParams.substr( 0, pos );
      std::string paramsStr
        = pPathWithParams.substr( pos+1, pPathWithParams.length() );

      //------------------------------------------------------------------------
      // Parse parameters
      //------------------------------------------------------------------------
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
    return true;
  }
}
