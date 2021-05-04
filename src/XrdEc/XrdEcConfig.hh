//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
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
//------------------------------------------------------------------------------

#ifndef SRC_XRDEC_XRDECCONFIG_HH_
#define SRC_XRDEC_XRDECCONFIG_HH_

#include "XrdEc/XrdEcRedundancyProvider.hh"
#include "XrdEc/XrdEcObjCfg.hh"

#include <string>
#include <unordered_map>

namespace XrdEc
{
  //---------------------------------------------------------------------------
  //! Global configuration for the EC module
  //---------------------------------------------------------------------------
  class Config
  {
    public:

      //-----------------------------------------------------------------------
      //! Singleton access
      //-----------------------------------------------------------------------
      static Config& Instance()
      {
        static Config config;
        return config;
      }

      //-----------------------------------------------------------------------
      //! Get redundancy provider for given data object configuration
      //-----------------------------------------------------------------------
      RedundancyProvider& GetRedundancy( const ObjCfg &objcfg )
      {
        std::string key;
        key += std::to_string( objcfg.nbchunks );
        key += ':';
        key += std::to_string( objcfg.nbparity );
        key += '-';
        key += std::to_string( uint8_t( objcfg.datasize ) );

        std::unique_lock<std::mutex> lck( mtx );
        auto itr = redundancies.find( key );
        if( itr == redundancies.end() )
        {
          auto p = redundancies.emplace( std::piecewise_construct,
                                         std::forward_as_tuple(key), 
                                         std::forward_as_tuple(objcfg) );
          return p.first->second;
        }
        else
          return itr->second;
      }

      bool enable_plugins;

    private:

      std::unordered_map<std::string, RedundancyProvider> redundancies;
      std::mutex mtx;

      //-----------------------------------------------------------------------
      //! Constructor
      //-----------------------------------------------------------------------
      Config() : enable_plugins( true )
      {
      }

      Config( const Config& ) = delete;            //< Copy constructor
      Config( Config&& ) = delete;                 //< Move constructor
      Config& operator=( const Config& ) = delete; //< Move assigment operator
      Config& operator=( Config&& ) = delete;      //< Copy assigment operator
  };
}


#endif /* SRC_XRDEC_XRDECCONFIG_HH_ */
