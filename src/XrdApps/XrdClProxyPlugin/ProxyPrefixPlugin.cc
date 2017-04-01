//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Elvin Sindrilaru <esindril@cern.ch>
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

#include "ProxyPrefixPlugin.hh"
#include "ProxyPrefixFile.hh"
#include "XrdVersion.hh"
#include "XrdSys/XrdSysDNS.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include <stdlib.h>
#include <cstdio>

XrdVERSIONINFO(XrdClGetPlugIn, XrdClGetPlugIn)

extern "C"
{
  void* XrdClGetPlugIn(const void* arg)
  {
    const std::map<std::string, std::string>* config =
      static_cast< const std::map<std::string, std::string>* >(arg);
    return static_cast<void*>(new xrdcl_proxy::ProxyFactory(config));
  }
}

namespace xrdcl_proxy
{
//------------------------------------------------------------------------------
// Construtor
//------------------------------------------------------------------------------
ProxyFactory::ProxyFactory(const std::map<std::string, std::string>* config)
{
  XrdCl::Log* log = XrdCl::DefaultEnv::GetLog();
  // If any of the parameters specific to this plugin are present then export
  // them as env variables to be used later on if not already set.
  if (config) {
    // When C++11 is here:
    // std::list<std::string> lst_envs {"XROOT_PROXY", "xroot_proxy",
    //                                  "XROOT_PROXY_EXCL_DOMAINS",
    //                                  "xroot_proxy_excl_domains};
    std::list<std::string> lst_envs;
    lst_envs.push_back("XROOT_PROXY");
    lst_envs.push_back("xroot_proxy");
    lst_envs.push_back("XROOT_PROXY_EXCL_DOMAINS");
    lst_envs.push_back("xroot_proxy_excl_domains");

    for (std::list<std::string>::iterator it_env = lst_envs.begin();
	 it_env != lst_envs.end(); ++it_env) {
      std::map<std::string, std::string>::const_iterator it_map =
	config->find(*it_env);

      if (it_map != config->end() && !it_map->second.empty()) {
	if (setenv(it_map->first.c_str(), it_map->second.c_str(), 0)) {
	  log->Error(1, "Failed to set env variable %s from the configuration"
		     " file", it_map->first.c_str());
	}
      }
    }
  }
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
ProxyFactory::~ProxyFactory() {}

//------------------------------------------------------------------------------
// Create a file plug-in for the given URL
//------------------------------------------------------------------------------
XrdCl::FilePlugIn*
ProxyFactory::CreateFile(const std::string& url)
{
  return static_cast<XrdCl::FilePlugIn*>(new ProxyPrefixFile());
}

//------------------------------------------------------------------------------
// Create a file system plug-in for the given URL
//------------------------------------------------------------------------------
XrdCl::FileSystemPlugIn*
ProxyFactory::CreateFileSystem(const std::string& url)
{
  XrdCl::Log* log = XrdCl::DefaultEnv::GetLog();
  log->Error(1, "FileSystem plugin implementation not suppoted");
  return static_cast<XrdCl::FileSystemPlugIn*>(0);
}
} // namespace xrdcl_proxy
