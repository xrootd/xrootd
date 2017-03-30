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
    return static_cast<void*>(new xrdcl_proxy::ProxyFactory());
  }
}

namespace xrdcl_proxy
{
//------------------------------------------------------------------------------
// Construtor
//------------------------------------------------------------------------------
ProxyFactory::ProxyFactory()
{
  //XrdCl::Log* log = XrdCl::DefaultEnv::GetLog();
  //log->Debug(1, "ProxyFactory constructor");
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
