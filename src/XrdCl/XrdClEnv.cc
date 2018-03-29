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

#include <cstdlib>

#include "XrdCl/XrdClEnv.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Get string
  //----------------------------------------------------------------------------
  bool Env::GetString( const std::string &key, std::string &value )
  {
    XrdSysRWLockHelper scopedLock( pLock );
    StringMap::iterator it;
    it = pStringMap.find( key );
    if( it == pStringMap.end() )
    {
      Log *log = DefaultEnv::GetLog();
      log->Debug( UtilityMsg,
                  "Env: trying to get a non-existent string entry: %s",
                  key.c_str() );
      return false;
    }
    value = it->second.first;
    return true;
  }

  //----------------------------------------------------------------------------
  // Put string
  //----------------------------------------------------------------------------
  bool Env::PutString( const std::string &key, const std::string &value )
  {
    XrdSysRWLockHelper scopedLock( pLock, false );

    //--------------------------------------------------------------------------
    // Insert the string if it's not there yet
    //--------------------------------------------------------------------------
    StringMap::iterator it;
    it = pStringMap.find( key );
    if( it == pStringMap.end() )
    {
      pStringMap[key] = std::make_pair( value, false );
      return true;
    }

    //--------------------------------------------------------------------------
    // The entry exists and it has been imported from the shell
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    if( it->second.second )
    {
      log->Debug( UtilityMsg,
                  "Env: trying to override a shell-imported string entry: %s",
                  key.c_str() );
      return false;
    }
    log->Debug( UtilityMsg,
                "Env: overriding entry: %s=\"%s\" with \"%s\"",
                key.c_str(), it->second.first.c_str(), value.c_str() );
    pStringMap[key] = std::make_pair( value, false );
    return true;
  }

  //----------------------------------------------------------------------------
  // Get int
  //----------------------------------------------------------------------------
  bool Env::GetInt( const std::string &key, int &value )
  {
    XrdSysRWLockHelper scopedLock( pLock );
    IntMap::iterator it;
    it = pIntMap.find( key );
    if( it == pIntMap.end() )
    {
      Log *log = DefaultEnv::GetLog();
      log->Debug( UtilityMsg,
                  "Env: trying to get a non-existent integer entry: %s",
                  key.c_str() );
      return false;
    }
    value = it->second.first;
    return true;
  }

  //----------------------------------------------------------------------------
  // Put int
  //----------------------------------------------------------------------------
  bool Env::PutInt( const std::string &key, int value )
  {
    XrdSysRWLockHelper scopedLock( pLock, false );

    //--------------------------------------------------------------------------
    // Insert the string if it's not there yet
    //--------------------------------------------------------------------------
    IntMap::iterator it;
    it = pIntMap.find( key );
    if( it == pIntMap.end() )
    {
      pIntMap[key] = std::make_pair( value, false );
      return true;
    }

    //--------------------------------------------------------------------------
    // The entry exists and it has been imported from the shell
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    if( it->second.second )
    {
      log->Debug( UtilityMsg,
                  "Env: trying to override a shell-imported integer entry: %s",
                  key.c_str() );
      return false;
    }
    log->Debug( UtilityMsg,
                "Env: overriding entry: %s=%d with %d",
                key.c_str(), it->second.first, value );

    pIntMap[key] = std::make_pair( value, false );
    return true;
  }

  //----------------------------------------------------------------------------
  // Import int
  //----------------------------------------------------------------------------
  bool Env::ImportInt( const std::string &key, const std::string &shellKey )
  {
    XrdSysRWLockHelper scopedLock( pLock, false );
    std::string strValue = GetEnv( shellKey );
    if( strValue == "" )
      return false;

    Log *log = DefaultEnv::GetLog();
    char *endPtr;
    int value = (int)strtol( strValue.c_str(), &endPtr, 0 );
    if( *endPtr )
    {
      log->Error( UtilityMsg,
                  "Env: Unable to import %s as %s: %s is not a proper integer",
                  shellKey.c_str(), key.c_str(), strValue.c_str() );
      return false;
    }

    log->Info( UtilityMsg, "Env: Importing from shell %s=%d as %s",
               shellKey.c_str(), value, key.c_str() );

    pIntMap[key] = std::make_pair( value, true );
    return true;
  }

  //----------------------------------------------------------------------------
  // Import string
  //----------------------------------------------------------------------------
  bool Env::ImportString( const std::string &key, const std::string &shellKey )
  {
    XrdSysRWLockHelper scopedLock( pLock, false );
    std::string value = GetEnv( shellKey );
    if( value == "" )
      return false;

    Log *log = DefaultEnv::GetLog();
    log->Info( UtilityMsg, "Env: Importing from shell %s=%s as %s",
               shellKey.c_str(), value.c_str(), key.c_str() );
    pStringMap[key] = std::make_pair( value, true );
    return true;
  }

  //----------------------------------------------------------------------------
  // Get a string from the environment
  //----------------------------------------------------------------------------
  std::string Env::GetEnv( const std::string &key )
  {
    char *var = getenv( key.c_str() );
    if( !var )
      return "";
    return var;
  }
}
