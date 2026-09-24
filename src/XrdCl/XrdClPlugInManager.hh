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

#ifndef __XRD_CL_PLUGIN_MANAGER__
#define __XRD_CL_PLUGIN_MANAGER__

#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class PlugInManagerFixture;

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Manage client-side plug-ins and match them agains URLs
  //----------------------------------------------------------------------------
  class PlugInManager
  {
    friend class ::PlugInManagerFixture;

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      PlugInManager();

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~PlugInManager();

      //------------------------------------------------------------------------
      //! Register a plug-in factory for the given url, registering a 0 pointer
      //! removes the factory for the url
      //------------------------------------------------------------------------
      bool RegisterFactory( const std::string &url,
                            PlugInFactory     *factory );

      //------------------------------------------------------------------------
      //! Register a plug-in factory applying to all URLs, registering
      //! a 0 pointer removes the factory
      //------------------------------------------------------------------------
      bool RegisterDefaultFactory( PlugInFactory *factory );

      //------------------------------------------------------------------------
      //! Retrieve the plug-in factory for the given URL
      //!
      //! @return you do not own the returned memory
      //------------------------------------------------------------------------
      PlugInFactory *GetFactory( const std::string url );

      //------------------------------------------------------------------------
      //! Process user environment to load plug-in settings.
      //!
      //! This will try to load a default plug-in from a library pointed to
      //! by the XRD_PLUGIN envvar. If this fails it will scan the configuration
      //! files located in:
      //!
      //! 1) directory pointed to by XRD_PLUGINCONFDIR envvar
      //! 2) user directory:   ~/.xrootd/client.plugins.d/
      //! 3) system directory: /etc/xrootd/client.plugins.d/
      //!
      //! In that order.
      //!
      //! The configuration files contain lines with key-value pairs in the
      //! form of 'key=value'.
      //!
      //! Mandatory keys are:
      //! url - a semicolon separated list of URLs the plug-in applies to
      //! lib - plugin library to be loaded
      //! enable - determines whether the plug-in should be enabled or not
      //!
      //! You may use any other keys for your own purposes.
      //!
      //! The first directory with a setting for a URL decides it, and the
      //! directories after it cannot change it. This applies to disabled
      //! plug-ins as well. Within one directory, the config files are
      //! processed in alphabetic order, and a setting for a URL found later
      //! supersedes the previous one. However, only the first enabled
      //! plug-in for all URLs ('url = *') is loaded. Any setting applied via
      //! environment or config files supersedes any setting done
      //! programmatically.
      //!
      //! The plug-in library must implement the following C function:
      //!
      //! @code{.cpp}
      //! extern "C"
      //! {
      //!   void *XrdClGetPlugIn( const void *arg )
      //!   {
      //!     return __your_plug_in_factory__;
      //!   }
      //! }
      //! @endcode
      //!
      //! where arg is a const pointer to std::map<std::string, std::string>
      //! containing the plug-in configuration.
      //------------------------------------------------------------------------
      void ProcessEnvironmentSettings();

    private:
      typedef void *(*PlugInFunc_t)( const void *arg );

      struct FactoryHelper
      {
        FactoryHelper(): plugin(0), factory(0), isEnv(false), counter(0) {}
        ~FactoryHelper()
        {
          delete factory;
          if(plugin) plugin->Unload();
          delete plugin;
        }
        XrdOucPinLoader *plugin;
        PlugInFactory   *factory;
        bool             isEnv;
        uint32_t         counter;
      };

      //------------------------------------------------------------------------
      //! Process the configuration directory and load plug in definitions
      //------------------------------------------------------------------------
      void ProcessConfigDir( const std::string &dir );

      //------------------------------------------------------------------------
      //! Process the configuration directories in order of priority, the
      //! first directory with a setting for a URL decides it
      //------------------------------------------------------------------------
      void ProcessConfigDirs( const std::vector<std::string> &dirs );

      //------------------------------------------------------------------------
      //! Process the configuration directory, skipping the URLs in settled,
      //! and add the URLs decided by this directory to settled
      //------------------------------------------------------------------------
      void ProcessConfigDir( const std::string     &dir,
                             std::set<std::string> &settled );

      //------------------------------------------------------------------------
      //! Process a plug-in config file and load the plug-in if possible
      //------------------------------------------------------------------------
      void ProcessPlugInConfig( const std::string &confFile );

      //------------------------------------------------------------------------
      //! Process a plug-in config file, skipping the URLs in settled, and add
      //! the URLs it decides to decided
      //------------------------------------------------------------------------
      void ProcessPlugInConfig( const std::string           &confFile,
                                const std::set<std::string> &settled,
                                std::set<std::string>       &decided );

      //------------------------------------------------------------------------
      //! Load the plug-in and create the factory
      //------------------------------------------------------------------------
      std::pair<XrdOucPinLoader*,PlugInFactory*> LoadFactory(
        const std::string                        &lib,
        const std::map<std::string, std::string> &config );

      //------------------------------------------------------------------------
      //! Register factory, if successful it actuires ownership of the objects
      //! @return true if successfully registered
      //------------------------------------------------------------------------
      bool RegisterFactory( const std::string &urlString,
                            const std::string &lib,
                            PlugInFactory     *factory,
                            XrdOucPinLoader   *plugin );

      //------------------------------------------------------------------------
      //! Normalize a URL
      //------------------------------------------------------------------------
      std::string NormalizeURL( const std::string url );

      std::map<std::string, FactoryHelper*>  pFactoryMap;
      FactoryHelper                         *pDefaultFactory;
      XrdSysMutex                            pMutex;
  };
}

#endif // __XRD_CL_PLUGIN_MANAGER__
