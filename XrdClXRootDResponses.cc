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

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClUtils.hh"
#include <cstdlib>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // LocationInfo constructor
  //----------------------------------------------------------------------------
  LocationInfo::LocationInfo( const char *data )
  {
    if( data )
      ParseServerResponse( data );
  }

  //----------------------------------------------------------------------------
  // Parse the server location response
  //----------------------------------------------------------------------------
  void LocationInfo::ParseServerResponse( const char *data )
  {
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
  // StatInfo constructor
  //----------------------------------------------------------------------------
  StatInfo::StatInfo( const char *data ):
    pSize( 0 ),
    pFlags( 0 ),
    pModTime( 0 )
  {
    ParseServerResponse( data );
  }

  //----------------------------------------------------------------------------
  // Parse the stat info returned by the server
  //----------------------------------------------------------------------------
  void StatInfo::ParseServerResponse( const char *data )
  {
    std::vector<std::string> chunks;
    Utils::splitString( chunks, data, " " );

    if( chunks.size() < 4 )
      return;

    pId = chunks[0];

    char *result;
    pSize = ::strtoll( chunks[1].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pSize = 0;
      return;
    }

    pFlags = ::strtol( chunks[2].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pFlags = 0;
      return;
    }

    pModTime = ::strtoll( chunks[3].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pModTime = 0;
      return;
    }
  }

  //----------------------------------------------------------------------------
  // StatInfo constructor
  //----------------------------------------------------------------------------
  StatInfoVFS::StatInfoVFS( const char *data ):
    pNodesRW( 0 ),
    pFreeRW( 0 ),
    pUtilizationRW( 0 ),
    pNodesStaging( 0 ),
    pFreeStaging( 0 ),
    pUtilizationStaging( 0 )
  {
    ParseServerResponse( data );
  }

  //----------------------------------------------------------------------------
  // Parse the stat info returned by the server
  //----------------------------------------------------------------------------
  void StatInfoVFS::ParseServerResponse( const char *data )
  {
    std::vector<std::string> chunks;
    Utils::splitString( chunks, data, " " );

    if( chunks.size() < 6 )
      return;

    char *result;
    pNodesRW = ::strtoll( chunks[0].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pNodesRW = 0;
      return;
    }

    pFreeRW = ::strtoll( chunks[1].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pFreeRW = 0;
      return;
    }

    pUtilizationRW = ::strtol( chunks[2].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pUtilizationRW = 0;
      return;
    }

    pNodesStaging = ::strtoll( chunks[3].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pNodesStaging = 0;
      return;
    }

    pFreeStaging = ::strtoll( chunks[4].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pFreeStaging = 0;
      return;
    }

    pUtilizationStaging = ::strtol( chunks[5].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pUtilizationStaging = 0;
      return;
    }
  }

  //----------------------------------------------------------------------------
  // DirectoryList constructor
  //----------------------------------------------------------------------------
  DirectoryList::DirectoryList( const std::string &hostId,
                                const std::string &parent,
                                const char        *data )
  {
    pParent = parent;
    if( pParent[pParent.length()-1] != '/' )
      pParent += "/";

    if( data )
      ParseServerResponse( hostId, data );
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  DirectoryList::~DirectoryList()
  {
    for( Iterator it = pDirList.begin(); it != pDirList.end(); ++it )
      delete *it;
  }

  //----------------------------------------------------------------------------
  // Parse the directory list
  //----------------------------------------------------------------------------
  void DirectoryList::ParseServerResponse( const std::string &hostId,
                                           const char *data )
  {
    std::vector<std::string>           entries;
    std::vector<std::string>::iterator it;
    Utils::splitString( entries, data, "\n" );

    for( it = entries.begin(); it != entries.end(); ++it )
      Add( new ListEntry( hostId, *it ) );
  }
}
