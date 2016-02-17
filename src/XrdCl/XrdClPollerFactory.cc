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

#include "XrdCl/XrdClPollerFactory.hh"
#include "XrdCl/XrdClPollerBuiltIn.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include <map>
#include <vector>

//------------------------------------------------------------------------------
// Poller creators
//------------------------------------------------------------------------------
namespace
{
  XrdCl::Poller *createBuiltIn()
  {
    return new XrdCl::PollerBuiltIn();
  }
};

namespace XrdCl
{
  //------------------------------------------------------------------------
  // Create a poller object, try in order of preference
  //------------------------------------------------------------------------
  Poller *PollerFactory::CreatePoller( const std::string &preference )
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Create a list of known pollers
    //--------------------------------------------------------------------------
    typedef std::map<std::string, Poller *(*)()> PollerMap;
    PollerMap pollerMap;
    pollerMap["built-in"] = createBuiltIn;

    //--------------------------------------------------------------------------
    // Print the list of available pollers
    //--------------------------------------------------------------------------
    PollerMap::iterator it;
    std::string available;
    for( it = pollerMap.begin(); it != pollerMap.end(); ++it )
    {
      available += it->first; available += ", ";
    }
    if( !available.empty() )
      available.erase( available.length()-2, 2 );
    log->Debug( PollerMsg, "Available pollers: %s", available.c_str() );

    //--------------------------------------------------------------------------
    // Try to create a poller
    //--------------------------------------------------------------------------
    if( preference.empty() )
    {
      log->Error( PollerMsg, "Poller preference list is empty" );
      return 0;
    }
    log->Debug( PollerMsg, "Attempting to create a poller according to "
                "preference: %s", preference.c_str() );

    std::vector<std::string> prefs;
    std::vector<std::string>::iterator itP;
    Utils::splitString( prefs, preference, "," );
    for( itP = prefs.begin(); itP != prefs.end(); ++itP )
    {
      it = pollerMap.find( *itP );
      if( it == pollerMap.end() )
      {
        log->Debug( PollerMsg, "Unable to create poller: %s",
                    itP->c_str() );
        continue;
      }
      log->Debug( PollerMsg, "Creating poller: %s", itP->c_str() );
      return (*it->second)();
    }

    return 0;
  }
}
