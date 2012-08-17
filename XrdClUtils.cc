//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClUtils.hh"
#include "XrdSys/XrdSysDNS.hh"
#include <algorithm>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Resolve IP addresses
  //----------------------------------------------------------------------------
  Status Utils::GetHostAddresses( std::vector<sockaddr_in> &addresses,
                                  const URL                &url )
  {
    //--------------------------------------------------------------------------
    // The address resolution algorithm in XRootD has a weird interface
    // so we need to limit the number of possible IP addresses for
    // a given hostname to 25. Why? Because.
    //--------------------------------------------------------------------------
    sockaddr_in *sa = (sockaddr_in*)malloc( sizeof(sockaddr_in) * 25 );
    int numAddr = XrdSysDNS::getHostAddr( url.GetHostName().c_str(),
                                          (sockaddr*)sa, 25 );
    if( numAddr == 0 )
    {
      free( sa );
      return Status( stError, errInvalidAddr );
    }

    addresses.resize( numAddr );
    std::vector<sockaddr_in>::iterator it = addresses.begin();;
    for( int i = 0; i < numAddr; ++i, ++it )
    {
      memcpy( &(*it), &sa[i], sizeof( sockaddr_in ) );
      it->sin_port = htons( (unsigned short) url.GetPort() );
    }
    free( sa );

    std::random_shuffle( addresses.begin(), addresses.end() );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Log all the addresses on the list
  //----------------------------------------------------------------------------
  void Utils::LogHostAddresses( Log                      *log,
                                uint64_t                  type,
                                const std::string        &hostId,
                                std::vector<sockaddr_in> &addresses )
  {
    std::string addrStr;
    std::vector<sockaddr_in>::iterator it;
    for( it = addresses.begin(); it != addresses.end(); ++it )
    {
      char nameBuff[256];
      XrdSysDNS::IPFormat( (sockaddr*)&(*it), nameBuff, sizeof(nameBuff) );
      addrStr += nameBuff;
      addrStr += ", ";
    }
    addrStr.erase( addrStr.length()-2, 2 );
    log->Debug( type, "[%s] Found %d address(es): %s",
                      hostId.c_str(), addresses.size(), addrStr.c_str() );
  }
}
