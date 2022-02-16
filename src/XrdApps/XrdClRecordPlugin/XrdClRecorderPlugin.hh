//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
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

#pragma once
#include "XrdCl/XrdClPlugInInterface.hh"

namespace XrdCl
{
//------------------------------------------------------------------------------
//! XrdCl recorder plug-in factory
//------------------------------------------------------------------------------
class RecorderFactory : public PlugInFactory
{
  public:
    //----------------------------------------------------------------------------
    //! Constructor
    //!
    //! @param config map containing configuration parameters
    //----------------------------------------------------------------------------
    RecorderFactory( const std::map<std::string, std::string>* config )
    {
      if( config )
      {
        auto itr = config->find( "output" );
        Recorder::SetOutput( itr != config->end() ? itr->second : "" );
      }
    }

    //----------------------------------------------------------------------------
    //! Destructor
    //----------------------------------------------------------------------------
    virtual ~RecorderFactory()
    {
    }

    //----------------------------------------------------------------------------
    //! Create a file plug-in for the given URL
    //----------------------------------------------------------------------------
    virtual FilePlugIn* CreateFile(const std::string& url)
    {
      std::unique_ptr<Recorder> ptr( new Recorder() );
      if( !ptr->IsValid() )
        return nullptr;
      return static_cast<FilePlugIn*>( ptr.release() );
    }

    //----------------------------------------------------------------------------
    //! Create a file system plug-in for the given URL
    //----------------------------------------------------------------------------
    virtual FileSystemPlugIn* CreateFileSystem(const std::string& url)
    {
      Log* log = DefaultEnv::GetLog();
      log->Error(1, "FileSystem plugin implementation not supported");
      return static_cast<FileSystemPlugIn*>(0);
    }
};

} // namespace xrdcl_proxy
