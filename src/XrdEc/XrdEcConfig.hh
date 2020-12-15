/*
 * XrdEcConfig.hh
 *
 *  Created on: Dec 4, 2018
 *      Author: simonm
 */

#ifndef SRC_XRDEC_XRDECCONFIG_HH_
#define SRC_XRDEC_XRDECCONFIG_HH_

#include "XrdEc/XrdEcRedundancyProvider.hh"
#include "XrdEc/XrdEcObjCfg.hh"

#include <stdint.h>
#include <string>
#include <unordered_map>

namespace XrdEc
{
  class Config
  {
    public:

      static Config& Instance()
      {
        static Config config;
        return config;
      }

      ~Config()
      {
      }

      RedundancyProvider& GetRedundancy( const ObjCfg &objcfg )
      {
        std::string key;
        key += std::to_string( objcfg.nbchunks );
        key += ':';
        key += std::to_string( objcfg.nbparity );
        key += '-';
        key += std::to_string( uint8_t( objcfg.datasize ) );

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

      uint8_t            maxrelocate;
      std::string        ckstype;
      uint8_t            repairthreads;
      std::string        headnode;

    private:

      std::unordered_map<std::string, RedundancyProvider> redundancies;

      Config() : maxrelocate( 10 ),
                 ckstype( "crc32" ),
                 repairthreads( 4 ),
                 headnode( "eospps.cern.ch" )
      {
      }

      Config( const Config& ) = delete;

      Config( Config&& ) = delete;

      Config& operator=( const Config& ) = delete;

      Config& operator=( Config&& ) = delete;
  };
}


#endif /* SRC_XRDEC_XRDECCONFIG_HH_ */
