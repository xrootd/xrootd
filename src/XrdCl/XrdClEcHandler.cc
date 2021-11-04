/*
 * XrdClEcHandler.cc
 *
 *  Created on: Nov 2, 2021
 *      Author: simonm
 */



#include "XrdCl/XrdClEcHandler.hh"

namespace XrdCl
{
  EcHandler* GetEcHandler( const URL &headnode, const URL &redirurl )
  {
    const URL::ParamsMap &params = redirurl.GetParams();
    // make sure all the xrdec. tokens are present and the values are sane
    URL::ParamsMap::const_iterator itr = params.find( "xrdec.nbdta" );
    if( itr == params.end() ) return nullptr;
    uint8_t nbdta = std::stoul( itr->second );

    itr = params.find( "xrdec.nbprt" );
    if( itr == params.end() ) return nullptr;
    uint8_t nbprt = std::stoul( itr->second );

    itr = params.find( "xrdec.blksz" );
    if( itr == params.end() ) return nullptr;
    uint64_t blksz = std::stoul( itr->second );

    itr = params.find( "xrdec.plgr" );
    if( itr == params.end() ) return nullptr;
    std::vector<std::string> plgr;
    Utils::splitString( plgr, itr->second, "," );
    if( plgr.size() < nbdta + nbprt ) return nullptr;

    itr = params.find( "xrdec.objid" );
    if( itr == params.end() ) return nullptr;
    std::string objid = itr->second;

    itr = params.find( "xrdec.format" );
    if( itr == params.end() ) return nullptr;
    size_t format = std::stoul( itr->second );
    if( format != 1 ) return nullptr; // TODO use constant

    std::vector<std::string> dtacgi;
    itr = params.find( "xrdec.dtacgi" );
    if( itr != params.end() )
    {
      Utils::splitString( dtacgi, itr->second, "," );
      if( plgr.size() != dtacgi.size() ) return nullptr;
    }

    std::vector<std::string> mdtacgi;
    itr = params.find( "xrdec.mdtacgi" );
    if( itr != params.end() )
    {
      Utils::splitString( mdtacgi, itr->second, "," );
      if( plgr.size() != mdtacgi.size() ) return nullptr;
    }

    itr = params.find( "xrdec.cosc" );
    if( itr == params.end() ) return nullptr;
    std::string cosc_str = itr->second;
    if( cosc_str != "true" && cosc_str != "false" ) return nullptr;
    bool cosc = cosc_str == "true";

    itr = params.find( "xrdec.cksum" );
    if( cosc && itr == params.end() ) return nullptr;
    std::string ckstype = itr->second;

    std::string chdigest;
    itr = params.find( "xrdec.chdigest" );
    if( itr == params.end() )
      chdigest = "crc32c";
    else
      chdigest = itr->second;
    bool usecrc32c = ( chdigest == "crc32c" );

    XrdEc::ObjCfg *objcfg = new XrdEc::ObjCfg( objid, nbdta, nbprt, blksz / nbdta, usecrc32c );
    objcfg->plgr    = std::move( plgr );
    objcfg->dtacgi  = std::move( dtacgi );
    objcfg->mdtacgi = std::move( mdtacgi );

    std::unique_ptr<CheckSumHelper> cksHelper( cosc ? new CheckSumHelper( "", ckstype ) : nullptr );
    if( cksHelper )
    {
      auto st = cksHelper->Initialize();
      if( !st.IsOK() ) return nullptr;
    }

    return new EcHandler( headnode, objcfg, std::move( cksHelper ) );
  }

}

