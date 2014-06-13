//------------------------------------------------------------------------------
// Copyright (c) 2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClPlugInManager.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdSys/XrdSysPwd.hh"

#include <unistd.h>
#include <sys/types.h>
#include <vector>
#include <string>
#include <algorithm>

XrdVERSIONINFOREF( XrdCl );

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  PlugInManager::PlugInManager():
    pDefaultFactory(0)
  {
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  PlugInManager:: ~PlugInManager()
  {
    std::map<std::string, FactoryHelper*>::iterator it;
    for( it = pFactoryMap.begin(); it != pFactoryMap.end(); ++it )
    {
      it->second->counter--;
      if( it->second->counter == 0 )
        delete it->second;
    }

    delete pDefaultFactory;
  }

  //----------------------------------------------------------------------------
  // Register a plug-in favtory for the given url
  //----------------------------------------------------------------------------
  bool PlugInManager::RegisterFactory( const std::string &url,
                                       PlugInFactory     *factory )
  {
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    std::string normUrl = NormalizeURL( url );
    if( normUrl == "" )
      return false;

    std::map<std::string, FactoryHelper*>::iterator it;
    it = pFactoryMap.find( normUrl );
    if( it != pFactoryMap.end() )
    {
      if( it->second->isEnv )
        return false;

      // we don't need to check the counter because it's valid only
      // for environment plugins which cannot be replaced via
      // this method
      delete it->second;
    }

    if( !factory )
    {
      log->Debug( PlugInMgrMsg, "Removing the factory for %s",
                  normUrl.c_str() );
      pFactoryMap.erase( it );
      return true;
    }

    log->Debug( PlugInMgrMsg, "Registering a factory for %s",
                normUrl.c_str() );

    FactoryHelper *h = new FactoryHelper();
    h->factory = factory;
    h->counter = 1;
    pFactoryMap[normUrl] = h;
    return true;
  }

  //------------------------------------------------------------------------
  //! Register a plug-in factory applying to all URLs
  //------------------------------------------------------------------------
  bool PlugInManager::RegisterDefaultFactory( PlugInFactory *factory )
  {
    Log *log = DefaultEnv::GetLog();
    XrdSysMutexHelper scopedLock( pMutex );

    if( pDefaultFactory && pDefaultFactory->isEnv )
      return false;

    delete pDefaultFactory;
    pDefaultFactory = 0;

    if( factory )
    {
      log->Debug( PlugInMgrMsg, "Registering a default factory" );
      pDefaultFactory = new FactoryHelper;
      pDefaultFactory->factory = factory;
    }
    else
      log->Debug( PlugInMgrMsg, "Removing the default factory" );

    return true;
  }

  //----------------------------------------------------------------------------
  // Retrieve the plug-in factory for the given URL
  //----------------------------------------------------------------------------
  PlugInFactory *PlugInManager::GetFactory( const std::string url )
  {
    XrdSysMutexHelper scopedLock( pMutex );

    if( pDefaultFactory && pDefaultFactory->isEnv )
      return pDefaultFactory->factory;

    std::string normUrl = NormalizeURL( url );
    if( normUrl.empty() )
    {
      if( pDefaultFactory )
        return pDefaultFactory->factory;
      return 0;
    }

    std::map<std::string, FactoryHelper*>::iterator it;
    it = pFactoryMap.find( normUrl );
    if( it != pFactoryMap.end() && it->second->isEnv )
      return it->second->factory;

    if( pDefaultFactory )
      return pDefaultFactory->factory;

    if( it != pFactoryMap.end() )
      return it->second->factory;

    return 0;
  }

  //----------------------------------------------------------------------------
  // Process user environment to load plug-in settings.
  //----------------------------------------------------------------------------
  void PlugInManager::ProcessEnvironmentSettings()
  {
    XrdSysMutexHelper scopedLock( pMutex );
    Log *log = DefaultEnv::GetLog();
    Env *env = DefaultEnv::GetEnv();

    log->Debug( PlugInMgrMsg, "Initializing plug-in manager..." );

    //--------------------------------------------------------------------------
    // Check if a default plug-in has been specified in the environment
    //--------------------------------------------------------------------------
    bool loadConfigs = true;
    std::string defaultPlugIn = DefaultPlugIn;
    env->GetString( "PlugIn", defaultPlugIn );
    if( !defaultPlugIn.empty() )
    {
      loadConfigs = false;
      log->Debug( PlugInMgrMsg, "Loading default plug-in from %s...",
                  defaultPlugIn.c_str());

      std::pair<XrdSysPlugin*, PlugInFactory *> pg = LoadFactory(
        defaultPlugIn, std::map<std::string, std::string>() );

      if( !pg.first )
      {
        log->Debug( PlugInMgrMsg, "Failed to load default plug-in from %s",
                    defaultPlugIn.c_str());
        loadConfigs = false;
      }

      pDefaultFactory = new FactoryHelper();
      pDefaultFactory->factory = pg.second;
      pDefaultFactory->plugin  = pg.first;
    }

    //--------------------------------------------------------------------------
    // If there is no default plug-in or it is invalid then load plug-in config
    // files
    //--------------------------------------------------------------------------
    if( loadConfigs )
    {
      log->Debug( PlugInMgrMsg,
                  "No default plug-in, loading plug-in configs..." );

      ProcessConfigDir( "/etc/xrootd/client.plugins.d" );

      XrdSysPwd pwdHandler;
      passwd *pwd = pwdHandler.Get( getuid() );
      std::string userPlugIns = pwd->pw_dir;
      userPlugIns += "/.xrootd/client.plugins.d";
      ProcessConfigDir( userPlugIns );

      std::string customPlugIns = DefaultPlugInConfDir;
      env->GetString( "PlugInConfDir", customPlugIns );
      if( !customPlugIns.empty() )
        ProcessConfigDir( customPlugIns );
    }
  }

  //----------------------------------------------------------------------------
  // Process the configuration directory and load plug in definitions
  //----------------------------------------------------------------------------
  void PlugInManager::ProcessConfigDir( const std::string &dir )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( PlugInMgrMsg, "Processing plug-in definitions in %s...",
                dir.c_str());

    std::vector<std::string> entries;
    std::vector<std::string>::iterator it;
    Status st = Utils::GetDirectoryEntries( entries, dir );
    if( !st.IsOK() )
    {
      log->Debug( PlugInMgrMsg, "Unable to process directory %s: %s",
                  dir.c_str(), st.ToString().c_str() );
      return;
    }
    std::sort( entries.begin(), entries.end() );

    for( it = entries.begin(); it != entries.end(); ++it )
    {
      std::string confFile = dir + "/" + *it;
      std::string suffix   = ".conf";
      if( confFile.length() <= suffix.length() )
        continue;
      if( !std::equal( suffix.rbegin(), suffix.rend(), confFile.rbegin() ) )
        continue;

      ProcessPlugInConfig( confFile );
   }
  }

  //----------------------------------------------------------------------------
  // Process a plug-in config file and load the plug-in if possible
  //----------------------------------------------------------------------------
  void PlugInManager::ProcessPlugInConfig( const std::string &confFile )
  {
    Log *log = DefaultEnv::GetLog();
    log->Dump( PlugInMgrMsg, "Processing: %s", confFile.c_str() );

    //--------------------------------------------------------------------------
    // Read the config
    //--------------------------------------------------------------------------
    std::map<std::string, std::string> config;
    Status st = Utils::ProcessConfig( config, confFile );
    if( !st.IsOK() )
    {
      log->Debug( PlugInMgrMsg, "Unable process config %s: %s",
                  confFile.c_str(), st.ToString().c_str() );
      return;
    }

    const char *keys[] = { "url", "lib", "enable", 0 };
    for( int i = 0; keys[i]; ++i )
    {
      if( config.find( keys[i] ) == config.end() )
      {
        log->Debug( PlugInMgrMsg, "Unable to find '%s' key in the config file "
                    "%s, ignoring this config", keys[i], confFile.c_str() );
        return;
      }
    }

    //--------------------------------------------------------------------------
    // Attempt to load the plug in and place it in the map
    //--------------------------------------------------------------------------
    std::string url = config["url"];
    std::string lib = config["lib"];
    std::string enable = config["enable"];

    log->Dump( PlugInMgrMsg, "Settings from '%s': url='%s', lib='%s', "
               "enable='%s'", confFile.c_str(), url.c_str(), lib.c_str(),
               enable.c_str() );

    std::pair<XrdSysPlugin*, PlugInFactory *> pg;
    pg.first = 0; pg.second = 0;
    if( enable == "true" )
    {
      log->Debug( PlugInMgrMsg, "Trying to load a plug-in for '%s' from '%s'",
                  url.c_str(), lib.c_str() );

      pg = LoadFactory( lib, config );

      if( !pg.first )
        return;
    }
    else
      log->Debug( PlugInMgrMsg, "Trying to disable plug-in for '%s'",
                  url.c_str() );

    if( !RegisterFactory( url, lib, pg.second, pg.first ) )
    {
      delete pg.first;
      delete pg.second;
    }
  }

  //----------------------------------------------------------------------------
  // Load the plug-in and create the factory
  //----------------------------------------------------------------------------
  std::pair<XrdSysPlugin*,PlugInFactory*> PlugInManager::LoadFactory(
    const std::string &lib, const std::map<std::string, std::string> &config )
  {
    Log *log = DefaultEnv::GetLog();

    char errorBuff[1024];
    XrdSysPlugin *pgHandler = new XrdSysPlugin( errorBuff, 1024,
                                                lib.c_str(), lib.c_str(),
                                                &XrdVERSIONINFOVAR( XrdCl ) );

    PlugInFunc_t pgFunc = (PlugInFunc_t)pgHandler->getPlugin(
      "XrdClGetPlugIn", false, false );

    if( !pgFunc )
    {
      log->Debug( PlugInMgrMsg, "Error while loading %s: %s", lib.c_str(),
                  errorBuff );
      return std::make_pair<XrdSysPlugin*, PlugInFactory*>( 0, 0 );
    }

    PlugInFactory *f = (PlugInFactory*)pgFunc( &config );

    if( !f )
    {
      delete pgHandler;
      return std::make_pair<XrdSysPlugin*, PlugInFactory*>( 0, 0 );
    }

    return std::make_pair( pgHandler, f );
  }

  //----------------------------------------------------------------------------
  // Handle factory - register it or free all the memory
  //----------------------------------------------------------------------------
  bool PlugInManager::RegisterFactory( const std::string &urlString,
                                       const std::string &lib,
                                       PlugInFactory     *factory,
                                       XrdSysPlugin      *plugin )
  {
    //--------------------------------------------------------------------------
    // Process and normalize the URLs
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    std::vector<std::string> urls;
    std::vector<std::string> normalizedURLs;
    std::vector<std::string>::iterator it;

    Utils::splitString( urls, urlString, ";" );

    for( it = urls.begin(); it != urls.end(); ++it )
    {
      std::string normURL = NormalizeURL( *it );
      if( normURL == "" )
      {
        log->Debug( PlugInMgrMsg, "Url cannot be normalized: '%s', ignoring",
                    it->c_str() );
        continue;
      }
      normalizedURLs.push_back( normURL );
    }

    std::sort( normalizedURLs.begin(), normalizedURLs.end() );
    std::unique( normalizedURLs.begin(), normalizedURLs.end() );

    if( normalizedURLs.empty() )
      return false;

    //--------------------------------------------------------------------------
    // Insert or remove from the map
    //--------------------------------------------------------------------------
    FactoryHelper *h = 0;

    if( factory )
    {
      h = new FactoryHelper();
      h->isEnv   = true;
      h->counter = normalizedURLs.size();
      h->plugin  = plugin;
      h->factory = factory;
    }

    std::map<std::string, FactoryHelper*>::iterator mapIt;
    for( it = normalizedURLs.begin(); it != normalizedURLs.end(); ++it )
    {
      mapIt = pFactoryMap.find( *it );
      if( mapIt != pFactoryMap.end() )
      {
        mapIt->second->counter--;
        if( mapIt->second->counter == 0 )
          delete mapIt->second;
      }

      if( h )
      {
        log->Debug( PlugInMgrMsg, "Registering a factory for %s from %s",
                    it->c_str(), lib.c_str() );
        pFactoryMap[*it] = h;
      }
      else
      {
        if( mapIt != pFactoryMap.end() )
        {
          log->Debug( PlugInMgrMsg, "Removing the factory for %s",
                      it->c_str() );
          pFactoryMap.erase( mapIt );
        }
      }
    }

    return true;
  }

  //----------------------------------------------------------------------------
  // Normalize a URL
  //----------------------------------------------------------------------------
  std::string PlugInManager::NormalizeURL( const std::string url )
  {
    URL urlObj = url;
    if( !urlObj.IsValid() )
      return "";
    std::ostringstream o;
    o << urlObj.GetProtocol() << "://" << urlObj.GetHostName() << ":";
    o << urlObj.GetPort();
    return o.str();
  }
};
