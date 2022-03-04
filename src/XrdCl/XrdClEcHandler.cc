/*
 * XrdClEcHandler.cc
 *
 *  Created on: Nov 2, 2021
 *      Author: simonm
 */



#include "XrdCl/XrdClEcHandler.hh"

namespace XrdCl
{

  ServerSpaceInfo::ServerSpaceInfo() {
    if (getenv("XrdCl_EC_X_RATIO"))
    {
      xRatio = atoi(getenv("XrdCl_EC_X_RATIO"));
    }
    else
    {
      xRatio = 1;
    }
  };

  void ServerSpaceInfo::SelectLocations(XrdCl::LocationInfo &oldList,
                                        XrdCl::LocationInfo &newList,
                                        uint32_t n)
  {
    TryInitExportPaths();
    AddServers(oldList);
    UpdateSpaceInfo();
  
    lock.lock();
    if (oldList.GetSize() > n && ! BlindSelect())
    {
      for (uint32_t j=0; j<ServerList.size(); j++)
      {
        for (uint32_t i=0; i<oldList.GetSize(); i++)
        {
          if (ServerList[j].address == oldList.At(i).GetAddress() &&
              oldList.At(i).GetType() == XrdCl::LocationInfo::ServerOnline)
          {
            newList.Add(oldList.At(i));
            if (newList.GetSize() == n)
            {
              lock.unlock();
              return;
            }
          }
        }
      }
    }
    else
    {
      for (uint32_t i=0; i<oldList.GetSize(); i++)
      {
        if (Exists(oldList.At(i)) &&
            oldList.At(i).GetType() == XrdCl::LocationInfo::ServerOnline)
        {
          newList.Add(oldList.At(i));
        }
      }
    }
    lock.unlock();
  }
  
  void ServerSpaceInfo::Dump()
  {
    for (uint32_t j=0; j<ServerList.size(); j++)
    {
      ServerList[j].Dump();
    }
  };
  
  void ServerSpaceInfo::TryInitExportPaths()
  {
    if (initExportPaths) return;
    lock.lock();
    if (! initExportPaths && getenv("XRDEXPORTS"))
    {
      std::istringstream p(getenv("XRDEXPORTS"));
      std::string s;
      while(std::getline(p, s, ' '))
      {
        ExportPaths.push_back(s);
      }
      initExportPaths = true;
    }
    lock.unlock();
  };
  
  uint64_t ServerSpaceInfo::GetFreeSpace(const std::string addr)
  {
    XrdCl::FileSystem fs(addr);
    XrdCl::Buffer queryArgs(1024), *queryResp = nullptr;
  
    for (uint32_t i=0; i<ExportPaths.size(); i++)
    {
      queryArgs.FromString(ExportPaths[i]);
      XrdCl::XRootDStatus st = fs.Query(XrdCl::QueryCode::Space, queryArgs, queryResp, 0);
      if (st.IsOK())
      {
        std::string resp = queryResp->ToString();
        int b = resp.find("oss.free=", 0);
        int e = resp.find("&", b);
        uint64_t s = 0;
        std::stringstream sstream0( resp.substr(b+9, e-(b+9)) );
        sstream0 >> s;
        if (queryResp) delete queryResp;
        return s;
      }
      if (queryResp) delete queryResp;
    }
    return 0;
  };
  
  bool ServerSpaceInfo::BlindSelect() {
    auto ms_since_epoch = std::chrono::system_clock::now().time_since_epoch() /
                          std::chrono::nanoseconds(1);
    return (ms_since_epoch % 10 > xRatio ? true : false);
  };
  
  void ServerSpaceInfo::UpdateSpaceInfo()
  {
    if (! initExportPaths) return;
    time_t t = time(NULL);
    if (t < lastUpdateT + 300) return;
    lock.lock();
    if (t > lastUpdateT + 300)
    {
        for (uint32_t j=0; j<ServerList.size(); j++)
          ServerList[j].freeSpace = GetFreeSpace(ServerList[j].address);
        std::sort(ServerList.begin(), ServerList.end());
        lastUpdateT = t;
    }
    lock.unlock();
  };
  
  bool ServerSpaceInfo::Exists(XrdCl::LocationInfo::Location &loc)
  {
    for (uint32_t j=0; j<ServerList.size(); j++)
      if (loc.GetAddress() == ServerList[j].address)
        return true;
    return false;
  };
  
  void ServerSpaceInfo::AddServers(XrdCl::LocationInfo &locInfo)
  {
    lock.lock();
    for (uint32_t i=0; i<locInfo.GetSize(); i++)
    {
      if (Exists(locInfo.At(i))) continue;
      if (locInfo.At(i).GetType() == XrdCl::LocationInfo::ServerOnline)
      {
        FreeSpace s;
        s.address = locInfo.At(i).GetAddress();
        s.freeSpace = GetFreeSpace( s.address );
        ServerList.push_back(s);
        std::sort(ServerList.begin(), ServerList.end());
      }
    }
    lock.unlock();
  };

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

    std::string ckstype;
    itr = params.find( "xrdec.cksum" );
    if( cosc && itr == params.end() ) return nullptr;
    if( cosc )
      ckstype = itr->second;

    std::string chdigest;
    itr = params.find( "xrdec.chdigest" );
    if( itr == params.end() )
      chdigest = "crc32c";
    else
      chdigest = itr->second;
    bool usecrc32c = ( chdigest == "crc32c" );

    bool nomtfile = false;
    itr = params.find( "xrdec.nomtfile" );
    if( itr != params.end() )
      nomtfile = ( itr->second == "true" );

    XrdEc::ObjCfg *objcfg = new XrdEc::ObjCfg( objid, nbdta, nbprt, blksz / nbdta, usecrc32c );
    objcfg->plgr     = std::move( plgr );
    objcfg->dtacgi   = std::move( dtacgi );
    objcfg->mdtacgi  = std::move( mdtacgi );
    objcfg->nomtfile = nomtfile;

    std::unique_ptr<CheckSumHelper> cksHelper( cosc ? new CheckSumHelper( "", ckstype ) : nullptr );
    if( cksHelper )
    {
      auto st = cksHelper->Initialize();
      if( !st.IsOK() ) return nullptr;
    }

    return new EcHandler( headnode, objcfg, std::move( cksHelper ) );
  }

}

