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
  LocationInfo::LocationInfo()
  {
  }

  //----------------------------------------------------------------------------
  // Parse the server location response
  //----------------------------------------------------------------------------
  bool LocationInfo::ParseServerResponse( const char *data )
  {
    if( !data || strlen( data ) == 0 )
      return false;

    std::vector<std::string>           locations;
    std::vector<std::string>::iterator it;
    Utils::splitString( locations, data, " " );
    for( it = locations.begin(); it != locations.end(); ++it )
      if( ProcessLocation( *it ) == false )
        return false;
    return true;
  }

  //----------------------------------------------------------------------------
  // Process location
  //----------------------------------------------------------------------------
  bool LocationInfo::ProcessLocation( std::string &location )
  {
    if( location.length() < 5 )
      return false;

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
        return false;
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
        return false;
    }

    //--------------------------------------------------------------------------
    // Push the location info
    //--------------------------------------------------------------------------
    pLocations.push_back( Location( location.substr(2), type, access ) );

    return true;
  }

  //----------------------------------------------------------------------------
  // StatInfo constructor
  //----------------------------------------------------------------------------
  StatInfo::StatInfo():
    pSize( 0 ),
    pFlags( 0 ),
    pModTime( 0 )
  {
  }

  //----------------------------------------------------------------------------
  // Parse the stat info returned by the server
  //----------------------------------------------------------------------------
  bool StatInfo::ParseServerResponse( const char *data )
  {
    if( !data || strlen( data ) == 0 )
      return false;

    std::vector<std::string> chunks;
    Utils::splitString( chunks, data, " " );

    if( chunks.size() < 4 )
      return false;

    pId = chunks[0];

    char *result;
    pSize = ::strtoll( chunks[1].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pSize = 0;
      return false;
    }

    pFlags = ::strtol( chunks[2].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pFlags = 0;
      return false;
    }

    pModTime = ::strtoll( chunks[3].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pModTime = 0;
      return false;
    }

    return true;
  }

  //----------------------------------------------------------------------------
  // StatInfo constructor
  //----------------------------------------------------------------------------
  StatInfoVFS::StatInfoVFS():
    pNodesRW( 0 ),
    pFreeRW( 0 ),
    pUtilizationRW( 0 ),
    pNodesStaging( 0 ),
    pFreeStaging( 0 ),
    pUtilizationStaging( 0 )
  {
  }

  //----------------------------------------------------------------------------
  // Parse the stat info returned by the server
  //----------------------------------------------------------------------------
  bool StatInfoVFS::ParseServerResponse( const char *data )
  {
    if( !data || strlen( data ) == 0 )
      return false;

    std::vector<std::string> chunks;
    Utils::splitString( chunks, data, " " );

    if( chunks.size() < 6 )
      return false;

    char *result;
    pNodesRW = ::strtoll( chunks[0].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pNodesRW = 0;
      return false;
    }

    pFreeRW = ::strtoll( chunks[1].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pFreeRW = 0;
      return false;
    }

    pUtilizationRW = ::strtol( chunks[2].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pUtilizationRW = 0;
      return false;
    }

    pNodesStaging = ::strtoll( chunks[3].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pNodesStaging = 0;
      return false;
    }

    pFreeStaging = ::strtoll( chunks[4].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pFreeStaging = 0;
      return false;
    }

    pUtilizationStaging = ::strtol( chunks[5].c_str(), &result, 0 );
    if( *result != 0 )
    {
      pUtilizationStaging = 0;
      return false;
    }

    return true;
  }

  //----------------------------------------------------------------------------
  // DirectoryList constructor
  //----------------------------------------------------------------------------
  DirectoryList::DirectoryList()
  {
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
  bool DirectoryList::ParseServerResponse( const std::string &hostId,
                                           const char *data )
  {
    if( !data )
      return false;

    //--------------------------------------------------------------------------
    // Check what kind of response we're dealing with
    //--------------------------------------------------------------------------
    std::string dat          = data;
    std::string dStatPrefix = ".\n0 0 0 0";
    bool        isDStat     = false;

    if( !dat.compare( 0, dStatPrefix.size(), dStatPrefix ) )
      isDStat = true;

    std::vector<std::string>           entries;
    std::vector<std::string>::iterator it;
    Utils::splitString( entries, dat, "\n" );

    //--------------------------------------------------------------------------
    // Normal response
    //--------------------------------------------------------------------------
    if( !isDStat )
    {
      for( it = entries.begin(); it != entries.end(); ++it )
        Add( new ListEntry( hostId, *it ) );
      return true;
    }

    //--------------------------------------------------------------------------
    // kXR_dstat
    //--------------------------------------------------------------------------
    if( (entries.size() < 2) || (entries.size() % 2) )
      return false;

    it = entries.begin(); ++it; ++it;
    for( ; it != entries.end(); ++it )
    {
      ListEntry *entry = new ListEntry( hostId, *it );
      Add( entry );
      ++it;
      StatInfo *i = new StatInfo();
      entry->SetStatInfo( i );
      bool ok = i->ParseServerResponse( it->c_str() );
      if( !ok )
        return false;
    }
    return true;
  }
}
