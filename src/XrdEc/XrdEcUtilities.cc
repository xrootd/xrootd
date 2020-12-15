/*
 * XrdEcUtilities.cc
 *
 *  Created on: Jan 10, 2019
 *      Author: simonm
 */



#include "XrdEc/XrdEcUtilities.hh"
#include "XrdCl/XrdClCheckSumManager.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdOuc/XrdOucCRC32C.hh"

#include <sstream>

namespace XrdEc
{
  std::string CalcChecksum( const char *buffer, uint64_t size )
  {
    uint32_t cksum = crc32c( 0, buffer, size );
    std::stringstream ss;
    ss << std::hex << cksum;
    return "crc32c:" + ss.str();
  }

  LocationStatus ToLocationStatus( const std::string &str )
  {
    if( str == "rw" ) return rw;
    if( str == "ro" ) return ro;
    if( str == "drain" ) return drain;
    if( str == "off" ) return off;
    throw std::exception(); // TODO
  }

  //------------------------------------------------------------------------
  // Find a new location (host) for given chunk.
  //------------------------------------------------------------------------
  XrdCl::OpenFlags::Flags Place( const ObjCfg                &objcfg,
                                 uint8_t                      chunkid,
                                 placement_t                 &placement,
                                 std::default_random_engine  &generator,
                                 const placement_group       &plgr,
                                 bool                         relocate )
  {
    static std::uniform_int_distribution<uint32_t>  distribution( 0, plgr.size() - 1 );

    bool exists = !placement.empty() && !placement[chunkid].empty();

    XrdCl::OpenFlags::Flags flags = XrdCl::OpenFlags::Write |
        ( exists ? XrdCl::OpenFlags::Delete : XrdCl::OpenFlags::New );

    if( !relocate && exists ) return flags;

    if( placement.empty() ) placement.resize( objcfg.nbchunks, "" );

    std::string host;
    LocationStatus lst;
    do
    {
      auto &tpl = plgr[distribution( generator )];
      host = std::get<0>( tpl );
      lst  = std::get<1>( tpl );
    }
    while( std::count( placement.begin(), placement.end(), host ) && lst == rw );

    placement[chunkid] = host;

    return flags;
  }

  placement_t GeneratePlacement( const ObjCfg           &objcfg,
                                 const std::string      &blkname,
                                 const placement_group  &plgr,
                                 bool                    write   )
  {
    static std::hash<std::string>  strhash;
    std::default_random_engine generator( strhash( blkname ) );
    std::uniform_int_distribution<uint32_t>  distribution( 0, plgr.size() - 1 );

    placement_t placement;
    size_t      off = 0;

    while( placement.size() < objcfg.nbchunks )
    {
      // check if the host is available
      auto &tpl = plgr[distribution( generator )];
      auto lst = std::get<1>( tpl );
      if( lst == LocationStatus::off ||
          ( write && lst != LocationStatus::rw ) )
      {
        ++off;
        if( plgr.size() - off < objcfg.nbchunks ) throw std::exception(); // TODO
        continue;
      }
      // check if the host is already in our placement
      auto &host = std::get<0>( tpl );
      if( std::count( placement.begin(), placement.end(), host ) ) continue;
      // add new location
      placement.push_back( host );
    }

    return std::move( placement );
  }

  placement_t GetSpares( const placement_group &plgr,
                         const placement_t     &placement,
                         bool                   write )
  {
    placement_t spares;
    spares.reserve( plgr.size() - placement.size() );

    for( auto &tpl : plgr )
    {
      auto &host = std::get<0>( tpl );
      if( std::count( placement.begin(), placement.end(), host ) ) continue;
      auto lst = std::get<1>( tpl );
      if( lst == LocationStatus::off || ( write && lst != LocationStatus::rw ) ) continue;
      spares.emplace_back( host );
    }

    return std::move( spares );
  }
}
