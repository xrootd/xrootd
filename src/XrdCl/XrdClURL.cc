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

#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClUtils.hh"

#include <cstdlib>
#include <vector>
#include <sstream>
#include <algorithm>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  URL::URL():
    pPort( 1094 )
  {
  }

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  URL::URL( const std::string &url ):
    pPort( 1094 )
  {
    FromString( url );
  }

  URL::URL( const char *url ) : pPort( 1094 )
  {
    FromString( url );
  }

  //----------------------------------------------------------------------------
  // Parse URL - it is rather trivial and horribly slow but probably there
  // is not need to have anything more fancy
  //----------------------------------------------------------------------------
  bool URL::FromString( const std::string &url )
  {
    Log *log = DefaultEnv::GetLog();

    Clear();

    if( url.length() == 0 )
    {
      log->Error( UtilityMsg, "The given URL is empty" );
      return false;
    }

    //--------------------------------------------------------------------------
    // Extract the protocol, assume file:// if none found
    //--------------------------------------------------------------------------
    size_t pos = url.find( "://" );

    std::string current;
    if( pos != std::string::npos )
    {
      pProtocol = url.substr( 0, pos );
      current   = url.substr( pos+3 );
    }
    else if( url[0] == '/' )
    {
      pProtocol = "file";
      current   = url;
    }
    else if( url[0] == '-' )
    {
      pProtocol = "stdio";
      current   = "-";
      pPort     = 0;
    }
    else
    {
      pProtocol = "root";
      current   = url;
    }

    //--------------------------------------------------------------------------
    // If the protocol is HTTP or HTTPS, change the default port number
    //--------------------------------------------------------------------------
    if (pProtocol == "http") {
      pPort = 80;
    }
    if (pProtocol == "https") {
      pPort = 443;
    }

    //--------------------------------------------------------------------------
    // Extract host info and path
    //--------------------------------------------------------------------------
    std::string path;
    std::string hostInfo;

    if( pProtocol == "stdio" )
      path = current;
    else if( pProtocol == "file")
    {
      if( current[0] == '/' )
        current = "localhost" + current;
      pos = current.find( '/' );
      if( pos == std::string::npos )
        hostInfo = current;
      else
      {
        hostInfo = current.substr( 0, pos );
        path     = current.substr( pos );
      }
    }
    else
    {
      pos = current.find( '/' );
      if( pos == std::string::npos )
        hostInfo = current;
      else
      {
        hostInfo = current.substr( 0, pos );
        path     = current.substr( pos+1 );
      }
    }

    if( !ParseHostInfo( hostInfo ) )
    {
      Clear();
      return false;
    }

    if( !ParsePath( path ) )
    {
      Clear();
      return false;
    }

    ComputeURL();

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
               "Path:      %s\n",
               url.c_str(), pProtocol.c_str(), pUserName.c_str(),
               pPassword.c_str(), pHostName.c_str(), pPort, pPath.c_str() );
    return true;
  }

  //----------------------------------------------------------------------------
  // Parse host info
  //----------------------------------------------------------------------------
  bool URL::ParseHostInfo( const std::string hostInfo )
  {
    if( pProtocol == "stdio" )
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
      hostPort = hostInfo.substr( pos+1 );
      pos = userPass.find( ":" );

      //------------------------------------------------------------------------
      // It's both username and password
      //------------------------------------------------------------------------
      if( pos != std::string::npos )
      {
        pUserName = userPass.substr( 0, pos );
        pPassword = userPass.substr( pos+1 );
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
        size_t pos2 = pHostName.find( "[::ffff" );
        size_t pos3 = pHostName.find( "[::" );
        if( pos != std::string::npos && pos3 != std::string::npos &&
            pos2 == std::string::npos )
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

    ComputeHostId();
    return true;
  }

  //----------------------------------------------------------------------------
  // Parse path
  //----------------------------------------------------------------------------
  bool URL::ParsePath( const std::string &path )
  {
    size_t pos = path.find( "?" );
    if( pos != std::string::npos )
    {
      pPath = path.substr( 0, pos );
      SetParams( path.substr( pos+1, path.length() ) );
    }
    else
      pPath = path;

    std::string::iterator back = pPath.end() - 1;
    if( pProtocol == "file" && *back == '/' )
      pPath.erase( back );

    ComputeURL();
    return true;
  }

  //----------------------------------------------------------------------------
  // Get path with params
  //----------------------------------------------------------------------------
  std::string URL::GetPathWithParams() const
  {
    std::ostringstream o;
    if( !pPath.empty() )
      o << pPath;

    o << GetParamsAsString();
    return o.str();
  }

  //------------------------------------------------------------------------
  //! Get the path with params, filteres out 'xrdcl.'
  //------------------------------------------------------------------------
  std::string URL::GetPathWithFilteredParams() const
  {
    std::ostringstream o;
    if( !pPath.empty() )
      o << pPath;

    o << GetParamsAsString( true );
    return o.str();
  }

  //------------------------------------------------------------------------
  //! Get protocol://host:port/path
  //------------------------------------------------------------------------
  std::string URL::GetLocation() const
  {
    std::ostringstream o;
    o << pProtocol << "://";
    if( pProtocol == "file" )
      o << pHostName;
    else
      o << pHostName << ":" << pPort << "/";
    o << pPath;
    return o.str();
  }

  //------------------------------------------------------------------------
  // Get the URL params as string
  //------------------------------------------------------------------------
  std::string URL::GetParamsAsString() const
  {
    return GetParamsAsString( false );
  }

  //------------------------------------------------------------------------
  //! Get the URL params as string
  //------------------------------------------------------------------------
  std::string URL::GetParamsAsString( bool filter ) const
  {
    if( pParams.empty() )
      return "";

    std::ostringstream o;
    o << "?";
    ParamsMap::const_iterator it;
    for( it = pParams.begin(); it != pParams.end(); ++it )
    {
      // we filter out client specific parameters
      if( filter && it->first.compare( 0, 6, "xrdcl." ) == 0 )
        continue;
      if( it != pParams.begin() ) o << "&";
      o << it->first << "=" << it->second;
    }
    return o.str();
  }

  //------------------------------------------------------------------------
  // Set params
  //------------------------------------------------------------------------
  void URL::SetParams( const std::string &params )
  {
    pParams.clear();
    std::string p = params;

    if( p.empty() )
      return;

    if( p[0] == '?' )
      p.erase( 0, 1 );

    std::vector<std::string>           paramsVect;
    std::vector<std::string>::iterator it;
    Utils::splitString( paramsVect, p, "&" );
    for( it = paramsVect.begin(); it != paramsVect.end(); ++it )
    {
      if( it->empty() ) continue;
      size_t pos = it->find( "=" );
      if( pos == std::string::npos )
        pParams[*it] = "";
      else
        pParams[it->substr(0, pos)] = it->substr( pos+1, it->length() );
    }
  }

  //----------------------------------------------------------------------------
  // Clear the fields
  //----------------------------------------------------------------------------
  void URL::Clear()
  {
    pHostId.clear();
    pProtocol.clear();
    pUserName.clear();
    pPassword.clear();
    pHostName.clear();
    pPort = 1094;
    pPath.clear();
    pParams.clear();
    pURL.clear();
  }

  //----------------------------------------------------------------------------
  // Check validity
  //----------------------------------------------------------------------------
  bool URL::IsValid() const
  {
    if( pProtocol.empty() )
      return false;
    if( pProtocol == "file" && pPath.empty() )
      return false;
    if( pProtocol == "stdio" && pPath != "-" )
      return false;
    if( pProtocol != "file" && pProtocol != "stdio" && pHostName.empty() )
      return false;
    return true;
  }

  bool URL::IsMetalink() const
  {
    Env *env = DefaultEnv::GetEnv();
    int mlProcessing = DefaultMetalinkProcessing;
    env->GetInt( "MetalinkProcessing", mlProcessing );
    if( !mlProcessing ) return false;
    return PathEndsWith( ".meta4" ) || PathEndsWith( ".metalink" );
  }

  bool URL::IsLocalFile() const
  {
    return pProtocol == "file" && pHostName == "localhost";
  }

  //------------------------------------------------------------------------
  // Does the protocol indicate encryption
  //------------------------------------------------------------------------
  bool URL::IsSecure() const
  {
    return ( pProtocol == "roots" || pProtocol == "xroots" );
  }

  //------------------------------------------------------------------------
  // Is the URL used in TPC context
  //------------------------------------------------------------------------
  bool URL::IsTPC() const
  {
    ParamsMap::const_iterator itr = pParams.find( "xrdcl.intent" );
    if( itr != pParams.end() )
      return itr->second == "tpc";
    return false;
  }

  bool URL::PathEndsWith(const std::string & sufix) const
  {
    if (sufix.size() > pPath.size()) return false;
    return std::equal(sufix.rbegin(), sufix.rend(), pPath.rbegin() );
  }

  //------------------------------------------------------------------------
  //Get the host part of the URL (user:password\@host:port) plus channel
  //specific CGI (xrdcl.identity & xrd.gsiusrpxy)
  //------------------------------------------------------------------------
  std::string URL::GetChannelId() const
  {
    std::string ret = pProtocol + "://" + pHostId;
    bool hascgi = false;

    std::string keys[] = { "xrdcl.intent",
                           "xrd.gsiusrpxy",
                           "xrd.gsiusrcrt",
                           "xrd.gsiusrkey",
                           "xrd.sss",
                           "xrd.k5ccname" };
    size_t size = sizeof( keys ) / sizeof( std::string );

    for( size_t i = 0; i < size; ++i )
    {
      ParamsMap::const_iterator itr = pParams.find( keys[i] );
      if( itr != pParams.end() )
      {
        ret += hascgi ? '&' : '?';
        ret += itr->first;
        ret += '=';
        ret += itr->second;
        hascgi = true;
      }
    }

    return ret;
  }

  //----------------------------------------------------------------------------
  // Recompute the host id
  //----------------------------------------------------------------------------
  void URL::ComputeHostId()
  {
    std::ostringstream o;
    if( !pUserName.empty() )
    {
      o << pUserName;
      if( !pPassword.empty() )
        o << ":" << pPassword;
      o << "@";
    }
    if( pProtocol == "file" )
      o << pHostName;
    else
      o << pHostName << ":" << pPort;
    pHostId = o.str();
  }

  //----------------------------------------------------------------------------
  // Recreate the url
  //----------------------------------------------------------------------------
  void URL::ComputeURL()
  {
    if( !IsValid() )
      pURL = "";

    std::ostringstream o;
    if( !pProtocol.empty() )
      o << pProtocol << "://";

    if( !pUserName.empty() )
    {
      o << pUserName;
      if( !pPassword.empty() )
        o << ":" << pPassword;
      o << "@";
    }

    if( !pHostName.empty() )
    {
      if( pProtocol == "file" )
        o << pHostName;
      else
        o << pHostName << ":" << pPort << "/";
    }

    o << GetPathWithParams();

    pURL = o.str();
  }
}
