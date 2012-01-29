//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClUtils.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // LocationInfo constructor
  //----------------------------------------------------------------------------
  LocationInfo::LocationInfo( const char *data )
  {
    ParseServerResponse( data );
  }

  //----------------------------------------------------------------------------
  // Parse the server location response
  //----------------------------------------------------------------------------
  void LocationInfo::ParseServerResponse( const char *data )
  {
    Log *log = DefaultEnv::GetLog();
    log->Info( XRootDMsg, "Got location info: %s", data );

    //--------------------------------------------------------------------------
    // Split the locations
    //--------------------------------------------------------------------------
    std::vector<std::string>           locations;
    std::vector<std::string>::iterator it;
    Utils::splitString( locations, data, " " );
    for( it = locations.begin(); it != locations.end(); ++it )
      ProcessLocation( *it );
  }

  //----------------------------------------------------------------------------
  // Process location
  //----------------------------------------------------------------------------
  void LocationInfo::ProcessLocation( std::string &location )
  {
    if( location.length() < 17 )
      return;

    //--------------------------------------------------------------------------
    // Decode location type
    //--------------------------------------------------------------------------
    LocationInfo::LocationType type;
    switch( location[0] )
    {
      case 'M':
        type = LocationInfo::ManagerOnline;
        break;
      case 'm':
        type = LocationInfo::ManagerPending;
        break;
      case 'S':
        type = LocationInfo::ServerOnline;
        break;
      case 's':
        type = LocationInfo::ServerPending;
        break;
      default:
        return;
    }

    //--------------------------------------------------------------------------
    // Decode access type
    //--------------------------------------------------------------------------
    LocationInfo::AccessType access;
    switch( location[1] )
    {
      case 'r':
        access = LocationInfo::Read;
        break;
      case 'w':
        access = LocationInfo::ReadWrite;
        break;
      default:
        return;
    }

    //--------------------------------------------------------------------------
    // Push the location info
    //--------------------------------------------------------------------------
    pLocations.push_back( Location( location.substr(2), type, access ) );
  }

  //----------------------------------------------------------------------------
  // Parse the stat info returned by the server
  //----------------------------------------------------------------------------
  void StatInfo::ParseResponse( std::string srvResponse )
  {
  }
}
