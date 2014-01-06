#ifndef __XRDFILECACHE_FACTORY_HH__
#define __XRDFILECACHE_FACTORY_HH__
//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University  
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman           
//----------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------

#include <string>
#include <vector>

#include <XrdSys/XrdSysPthread.hh>
#include <XrdOuc/XrdOucCache.hh>
#include "XrdVersion.hh" 
class XrdOucStream;
class XrdSysError;

#include "XrdFileCacheDecision.hh"

namespace XrdFileCache
{


struct Configuration
{
   Configuration(): 
      m_prefetchFileBlocks(false),
      m_temp_directory("/var/tmp/xrootd-file-cache"),
      m_username("nobody"),
      m_lwm(0.95),
      m_hwm(0.9) {}

    bool m_prefetchFileBlocks;
    std::string m_config_filename;
    std::string m_temp_directory;
    std::string m_username;
    std::string m_osslib_name;
    float m_lwm;
    float m_hwm;
};


class Factory : public XrdOucCache
{
public:
    Factory();

    XrdOucCacheIO *
    Attach(XrdOucCacheIO *, int Options=0) {return NULL; }

    int
    isAttached() {return false; }

    bool Config(XrdSysLogger *logger, const char *config_filename, const char *parameters);

    virtual XrdOucCache *Create(Parms &, XrdOucCacheIO::aprParms *aprP=0);

    XrdOss*GetOss() const {return m_output_fs; }
    XrdSysError& GetSysError() {return m_log;}

    bool Decide(std::string &);

    static Factory &GetInstance();

    static  bool   VCheck(XrdVersionInfo &urVersion) {return true;}

    const Configuration& RefConfiguration() const { return m_configuration; };

    void TempDirCleanup();

private:
    bool ConfigParameters(const char *);
    bool ConfigXeq(char *, XrdOucStream &);
    bool xolib(XrdOucStream &);
    bool xdlib(XrdOucStream &);

    static XrdSysMutex m_factory_mutex;
    static Factory * m_factory;

    XrdSysError m_log;
    XrdOucCacheStats m_stats;
    XrdOss *m_output_fs;

    std::vector<XrdFileCache::Decision*> m_decisionpoints;

    Configuration m_configuration;   
};

}

#endif
