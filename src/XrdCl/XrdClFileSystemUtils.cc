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

#include "XrdCl/XrdClFileSystemUtils.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClUglyHacks.hh"
#include "XrdCl/XrdClURL.hh"

#include <vector>
#include <utility>
#include <memory>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Recursively get space information for given path
  //----------------------------------------------------------------------------
  XRootDStatus FileSystemUtils::GetSpaceInfo( SpaceInfo         *&result,
                                              FileSystem         *fs,
                                              const std::string  &path )
  {
    //--------------------------------------------------------------------------
    // Locate all the disk servers containing the space
    //--------------------------------------------------------------------------
    LocationInfo *locationInfo = 0;
    XRootDStatus st = fs->DeepLocate( path, OpenFlags::None, locationInfo );
    if( !st.IsOK() )
      return st;

    XRDCL_SMART_PTR_T<LocationInfo> locationInfoPtr( locationInfo );

    bool partial = st.code == suPartial ? true : false;

    std::vector<std::pair<std::string, uint64_t> > resp;
    resp.push_back( std::make_pair( std::string("oss.space"), (uint64_t)0 ) );
    resp.push_back( std::make_pair( std::string("oss.free"), (uint64_t)0 ) );
    resp.push_back( std::make_pair( std::string("oss.used"), (uint64_t)0 ) );
    resp.push_back( std::make_pair( std::string("oss.maxf"), (uint64_t)0 ) );

    //--------------------------------------------------------------------------
    // Loop over the file servers and get the space info from each of them
    //--------------------------------------------------------------------------
    LocationInfo::Iterator it;
    Buffer pathArg; pathArg.FromString( path );
    for( it = locationInfo->Begin(); it != locationInfo->End(); ++it )
    {
      //------------------------------------------------------------------------
      // Query the server
      //------------------------------------------------------------------------
      Buffer *spaceInfo = 0;
      FileSystem fs1( it->GetAddress() );
      st = fs1.Query( QueryCode::Space, pathArg, spaceInfo );
      if( !st.IsOK() )
        return st;

      XRDCL_SMART_PTR_T<Buffer> spaceInfoPtr( spaceInfo );

      //------------------------------------------------------------------------
      // Parse the cgi
      //------------------------------------------------------------------------
      std::string fakeUrl = "root://fake/fake?" + spaceInfo->ToString();
      URL url( fakeUrl );

      if( !url.IsValid() )
        return XRootDStatus( stError, errInvalidResponse );

      URL::ParamsMap params = url.GetParams();

      //------------------------------------------------------------------------
      // Convert and add up the params
      //------------------------------------------------------------------------
      st = XRootDStatus( stError, errInvalidResponse );
      for( size_t i = 0; i < resp.size(); ++i )
      {
        URL::ParamsMap::iterator paramIt = params.find( resp[i].first );
        if( paramIt == params.end() ) return st;
        char *res;
        uint64_t num = ::strtoll( paramIt->second.c_str(), &res, 0 );
        if( *res != 0 ) return st;
        if( resp[i].first == "oss.maxf" )
          { if( num > resp[i].second ) resp[i].second = num; }
        else
          resp[i].second += num;
      }
    }

    result = new SpaceInfo( resp[0].second, resp[1].second, resp[2].second,
                            resp[3].second );

    st = XRootDStatus(); if( partial ) st.code = suPartial;
    return st;
  }
}
