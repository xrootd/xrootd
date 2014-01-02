
#ifndef __XRDFILECACHE_FACTORY_HH__
#define __XRDFILECACHE_FACTORY_HH__
/******************************************************************************/
/*                                                                            */
/* (c) 2012 University of Nebraksa-Lincoln                                    */
/*     by Brian Bockelman                                                     */
/*                                                                            */
/******************************************************************************/

#include <string>
#include <vector>

//#include "XrdFileCacheFwd.hh"
#include <XrdSys/XrdSysPthread.hh>
#include <XrdOuc/XrdOucCache.hh>
#include "XrdVersion.hh" 
class XrdOucStream;
class XrdSysError;
class Decision;

namespace XrdFileCache
{

class Cache;

class Factory : public XrdOucCache
{
    friend class IO;

public:
    Factory();

    XrdOucCacheIO *
    Attach(XrdOucCacheIO *, int Options=0) {return NULL; }

    int
    isAttached() {return false; }

    bool Config(XrdSysLogger *logger, const char *config_filename, const char *parameters);

    virtual XrdOucCache *Create(Parms &, XrdOucCacheIO::aprParms *aprP=0);

    bool PrefetchFileBlocks() const {return m_prefetchFileBlocks; }
    const std::string &GetUsername() const {return m_username; }
    const std::string GetTempDirectory() const {return m_temp_directory; }
    XrdOss*GetOss() const {return m_output_fs; }
    XrdSysError& GetSysError() {return m_log;}

    bool Decide(std::string &);

    void TempDirCleanup();
    static Factory &GetInstance();

    static  bool   VCheck(XrdVersionInfo &urVersion) {return true;}
protected:
   // PrefetchPtr GetPrefetch(XrdOucCacheIO &, std::string& filePath);

private:
    bool ConfigParameters(const char *);
    bool ConfigXeq(char *, XrdOucStream &);
    bool xolib(XrdOucStream &);
    bool xdlib(XrdOucStream &);
    bool xexpire(XrdOucStream &);

    static XrdSysMutex m_factory_mutex;
    static Factory * m_factory;

    XrdSysError m_log;
    XrdOucCacheStats m_stats;
   //    PrefetchWeakPtrMap m_file_map;
    XrdOss *m_output_fs;
    std::vector<Decision*> m_decisionpoints;

    // configuration
    bool m_prefetchFileBlocks;
    std::string m_osslib_name;
    std::string m_config_filename;
    std::string m_temp_directory;
    std::string m_username;
    float m_lwm;
    float m_hwm;

};

}

#endif
