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

#include <sstream>
#include <fcntl.h>
#include <stdio.h>
#include <map>


#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOss/XrdOss.hh"
#if !defined(HAVE_VERSIONS)
#include "XrdOss/XrdOssApi.hh"
#endif
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdClient/XrdClient.hh"
#include "XrdVersion.hh"
#include "XrdPosix/XrdPosixXrootd.hh"
#include "XrdCl/XrdClDefaultEnv.hh"

#include "XrdFileCache.hh"
#include "XrdFileCacheFactory.hh"
#include "XrdFileCachePrefetch.hh"
#include "XrdFileCacheLog.hh"



using namespace XrdFileCache;

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(Config);

XrdVERSIONINFO(XrdOucGetCache, first_cache_imp_alja);

// Copy/paste from XrdOss/XrdOssApi.cc.  Unfortunately, this function
// is not part of the stable API for extension writers, necessitating
// the copy/paste.
//

Factory * Factory::m_factory = NULL;
XrdSysMutex Factory::m_factory_mutex;

XrdOss *
XrdOssGetSS(XrdSysLogger *Logger, const char *config_fn,
            const char *OssLib, const char *OssParms)
{
    static XrdOssSys myOssSys;
    extern XrdSysError OssEroute;
    XrdSysPlugin *myLib;
    XrdOss *(*ep)(XrdOss *, XrdSysLogger *, const char *, const char *);

    XrdSysError err(Logger, "XrdOssGetSS");

// If no library has been specified, return the default object
//
#if defined(HAVE_VERSIONS)
    if (!OssLib)
        OssLib = "libXrdOfs.so"
#else
    if (!OssLib || !*OssLib)
    {
        err.Emsg("GetOSS", "Attempting to initiate default OSS object.");
        if (myOssSys.Init(Logger, config_fn)) return 0;
        else return (XrdOss *)&myOssSys;
    }
#endif

// Create a plugin object
//
    OssEroute.logger(Logger);
    OssEroute.Emsg("XrdOssGetSS", "Initializing OSS lib from ", OssLib);
#if defined(HAVE_VERSIONS)
    if (!(myLib = new XrdSysPlugin(&OssEroute, OssLib, "osslib",
                                   myOssSys.myVersion))) return 0;
#else
    if (!(myLib = new XrdSysPlugin(&OssEroute, OssLib))) return 0;
#endif

// Now get the entry point of the object creator
//
    ep = (XrdOss *(*)(XrdOss *, XrdSysLogger *, const char *, const char *))
             (myLib->getPlugin("XrdOssGetStorageSystem"));
    if (!ep) return 0;

// Get the Object now
//
#if defined(HAVE_VERSIONS)
    myLib->Persist(); delete myLib;
#endif
    return ep((XrdOss *)&myOssSys, Logger, config_fn, OssParms);
}


void*
TempDirCleanupThread(void* cache_void)
{    Factory::GetInstance().TempDirCleanup();
    return NULL;
}


Factory::Factory()
    : m_log(0, "XFC_")
{
    Dbg = kInfo;
}

extern "C"
{
XrdOucCache *
XrdOucGetCache(XrdSysLogger *logger,
               const char   *config_filename,
               const char   *parameters)
{   
    XrdSysError err(0, "");
    err.logger(logger);
    err.Emsg("Retrieve", "Retrieving a caching proxy factory.");
    Factory &factory = Factory::GetInstance();
    if (!factory.Config(logger, config_filename, parameters))
    {
        err.Emsg("Retrieve", "Error - unable to create a factory.");
        return NULL;
    }
    err.Emsg("Retrieve", "Success - returning a factory.");


    pthread_t tid;
    XrdSysThread::Run(&tid, TempDirCleanupThread, NULL, 0, "XrdFileCache TempDirCleanup");
    return &factory;
}
}

Factory &
Factory::GetInstance()
{
    if (m_factory == NULL)
        m_factory = new Factory();
    return *m_factory;
}

XrdOucCache *
Factory::Create(Parms & parms, XrdOucCacheIO::aprParms * prParms)
{
    aMsg(kInfo, "Factory::Create() new cache object");
    return new Cache(m_stats);
}

bool
Factory::Config(XrdSysLogger *logger, const char *config_filename, const char *parameters)
{
    m_log.logger(logger);

    const char * cache_env;
    if (!(cache_env = getenv("XRDPOSIX_CACHE")) || !*cache_env)
        XrdOucEnv::Export("XRDPOSIX_CACHE", "mode=s&optwr=0");

    XrdOucEnv myEnv;
    XrdOucStream Config(&m_log, getenv("XRDINSTANCE"), &myEnv, "=====> ");

    if (!config_filename || !*config_filename)
    {
        aMsg(kWarning, "Factory::Config() configuration file not specified.");
        return false;
    }

    int fd;
    if ( (fd = open(config_filename, O_RDONLY, 0)) < 0)
    {
        aMsg(kError, "Factory::Config() can't open configuration file %s", config_filename);
        return false;
    }

    Config.Attach(fd);

    // Actual parsing of the config file.
    bool retval = true;
    int retc;
    char *var;
    while((var = Config.GetMyFirstWord()))
    {
        if ((strncmp(var, "oss.", 4) == 0) && (!ConfigXeq(var+4, Config)))
        {
            Config.Echo();
            retval = false;
            break;
        }
        if ((strncmp(var, "pss.", 4) == 0) && (!ConfigXeq(var+4, Config)))
        {
            Config.Echo();
            retval = false;
            break;
        }
    }

    if ((retc = Config.LastError()))
    {
        retval = false;
        aMsg(kError, "Factory::Config() error in parsing");
    }

    Config.Close();


    if (retval)
        retval = ConfigParameters(parameters);

    aMsg(kInfo,"Factory::Config() Cache user name %s", m_configuration.m_username.c_str());
    aMsg(kInfo,"Factory::Config() Cache temporary directory %s", m_configuration.m_cache_dir.c_str());
    aMsg(kInfo,"Factory::Config() Cache debug level %d", Dbg);
           
    if (retval)
    {
        XrdOss *output_fs = XrdOssGetSS(m_log.logger(), config_filename, m_configuration.m_osslib_name.c_str(), NULL);
        if (!output_fs) {
            aMsg(kError, "Factory::Config() Unable to create a OSS object");
            retval = false;
        }
        m_output_fs = output_fs;
    }

    aMsg(kInfo, "Factory::Config() Configuration = %s ", retval ? "Success" : "Fail");

    return retval;
}

bool
Factory::ConfigXeq(char *var, XrdOucStream &Config)
{
    TS_Xeq("osslib",        xolib);
    TS_Xeq("decisionlib",  xdlib);
    return true;
}

/* Function: xolib

   Purpose:  To parse the directive: osslib <path> [<parms>]

             <path>  the path of the oss library to be used.
             <parms> optional parameters to be passed.

   Output: true upon success or false upon failure.
 */
bool
Factory::xolib(XrdOucStream &Config)
{
    char *val, parms[2048];
    int pl;

    if (!(val = Config.GetWord()) || !val[0])
    {
        aMsg(kInfo, "Factory::Config() osslib not specified");
        return false;
    }

    strcpy(parms, val);
    pl = strlen(val);
    *(parms+pl) = ' ';
    if (!Config.GetRest(parms+pl+1, sizeof(parms)-pl-1))
    {
        aMsg(kError, "Factory::Config() osslib parameters too long");
        return false;
    }

    m_configuration.m_osslib_name = parms;
    return true;
}


/* Function: xdlib

   Purpose:  To parse the directive: decisionlib <path> [<parms>]

             <path>  the path of the decision library to be used.
             <parms> optional parameters to be passed.


   Output: true upon success or false upon failure.
 */
bool
Factory::xdlib(XrdOucStream &Config)
{
    const char*  val;

    std::string libp;
    if (!(val = Config.GetWord()) || !val[0])
    {
        aMsg(kInfo, " Factory:;Config() decisionlib not specified; always caching files");
        return true;
    }
    else
    {
        libp = val;
    }

    const char* params;
    params = (val[0]) ?  Config.GetWord() : 0;

#if defined(HAVE_VERSIONS)
    XrdSysPlugin* myLib = new XrdSysPlugin(&m_log, libp.c_str(), "decisionlib", NULL);
#else
    XrdSysPlugin* myLib = new XrdSysPlugin(&m_log, libp.c_str());
#endif
    Decision *(*ep)(XrdSysError&);
    ep = (Decision *(*)(XrdSysError&))myLib->getPlugin("XrdFileCacheGetDecision");
    if (!ep) return false;

    Decision * d = ep(m_log);
    if (!d)
    {
        aMsg(kError, "Factory::Config() decisionlib was not able to create a decision object");
        return false;
    }
    if (params)
        d->ConfigDecision(params);

    m_decisionpoints.push_back(d);
    return true;
}

bool
Factory::ConfigParameters(const char * parameters)
{
    if (!parameters || (!(*parameters)))
    {
        return true;
    }

    istringstream is(parameters);
    string part;
    while (getline(is, part, ' '))
    {
        // cout << part << endl;
        if ( part == "-prefetchFileBlocks" ) 
        {
            m_configuration.m_prefetchFileBlocks = true;
            aMsg(kInfo, "Factory::ConfigParameters() enable block prefetch.");
        }
        else if ( part == "-user" )
        {
            getline(is, part, ' ');
            m_configuration.m_username = part.c_str();
            aMsg(kInfo, "Factory::ConfigParameters() set user to %s", m_configuration.m_username.c_str());
        }
        else if  ( part == "-cacheDir" )
        {
            getline(is, part, ' ');
            m_configuration.m_cache_dir = part.c_str();
            aMsg(kInfo, "Factory::ConfigParameters() set temp. directory to %s", m_configuration.m_cache_dir.c_str());
        }
        else if  ( part == "-debug" )
        {
            getline(is, part, ' ');
            Dbg = (LogLevel)atoi(part.c_str());
        }
        else if  ( part == "-lwm" )
        {
            getline(is, part, ' ');
            m_configuration.m_lwm = ::atof(part.c_str());
            aMsg(kInfo, "Factory::ConfigParameters() lwm = %f", m_configuration.m_lwm);
        }
        else if  ( part == "-hwm" )
        {
            getline(is, part, ' ');
            m_configuration.m_hwm = ::atof(part.c_str());
            aMsg(kInfo, "Factory::ConfigParameters() hwm = %f", m_configuration.m_hwm);
        }
        else if  ( part == "-bufferSize" )
        {
            getline(is, part, ' ');
            // prefetch buffer size is long long becuse of possible problems 
            // after multiplcation, but in this stepe it is save to use atoi 
            m_configuration.m_bufferSize = ::atoi(part.c_str());
            aMsg(kInfo, "Factory::ConfigParameters() bufferSize = %lld", m_configuration.m_bufferSize);
        }
        else if  ( part == "-blockSize" )
        {
            getline(is, part, ' ');
            m_configuration.m_blockSize = ::atoi(part.c_str());
            aMsg(kInfo, "Factory::ConfigParameters() blockSize = %lld", m_configuration.m_blockSize);
        }
    }

    return true;
}

bool
Factory::Decide(std::string &filename)
{  
    //  decision_vt::iterator it =  m_decisionpoints.begin();

    if(! m_decisionpoints.empty()) 
    {
        std::vector<Decision*>::const_iterator it;
        for ( it = m_decisionpoints.begin(); it != m_decisionpoints.end(); ++it)
        {
            XrdFileCache::Decision *d = *it;
            if (!d) continue;
            if (!d->Decide(filename, *m_output_fs))
            {
                return false;
            }
        }
    }
 
    return true;
}


//______________________________________________________________________________


void
FillFileMapRecurse( XrdOssDF* df, const std::string& path, std::map<std::string, time_t>& fcmap)
{
   char buff[256];
   XrdOucEnv env;
   int rdr;
   const size_t InfoExtLen = sizeof(XrdFileCache::Info::m_infoExtension);// cached var

   Factory& factory = Factory::GetInstance();
   while ( (rdr = df->Readdir(&buff[0], 256)) >= 0)
   {
      // printf("readdir [%s]\n", buff);
      std::string np = path + "/" + std::string(buff);
      size_t fname_len = strlen(&buff[0]);
      if (fname_len == 0  )
      {
         // std::cout << "Finish read dir.[" << np <<"] Break loop \n";
         break;
      }

      if (strncmp("..", &buff[0], 2) && strncmp(".", &buff[0], 1))
      {
         XrdOssDF* dh = factory.GetOss()->newDir(factory.RefConfiguration().m_username.c_str());   
         XrdOssDF* fh = factory.GetOss()->newFile(factory.RefConfiguration().m_username.c_str());   

         if (fname_len > InfoExtLen && strncmp(&buff[fname_len - InfoExtLen ], XrdFileCache::Info::m_infoExtension , InfoExtLen) == 0)
         {
            fh->Open((np).c_str(),O_RDONLY, 0600, env);
            Info cinfo;
            time_t accessTime;
            cinfo.Read(fh);
            if (cinfo.getLatestAttachTime(accessTime, fh))
            {
               aMsg(kDebug, "FillFileMapRecurse() checking %s accessTime %d ", buff, (int)accessTime);
               fcmap[np] = accessTime;
            }
            else
            {
               aMsg(kWarning, "FillFileMapRecurse() could not get access time for %s \n", np.c_str());
            }
         }
         else if ( dh->Opendir(np.c_str(), env)  >= 0 )
         {
            FillFileMapRecurse(dh, np, fcmap);
         }

         delete dh; dh = 0;
         delete df; df = 0;
      }
   }
}


void
Factory::TempDirCleanup()
{
    // check state every sleepts seconds
    const static int sleept = 180;

    struct stat fstat;
    XrdOucEnv env;

    XrdOss* oss =  Factory::GetInstance().GetOss();
    XrdOssDF* dh = oss->newDir(m_configuration.m_username.c_str());
    while (1)
    {     
        // get amout of space to erase
        long long bytesToRemove = 0;
        struct statvfs fsstat;
        if(statvfs(m_configuration.m_cache_dir.c_str(), &fsstat) < 0 ) {
            aMsg(kError, "Factory::TempDirCleanup() can't get statvfs for dir [%s] \n", m_configuration.m_cache_dir.c_str());
            exit(1);
        }
        else
        {
            float oc = 1 - float(fsstat.f_bfree)/fsstat.f_blocks;
            aMsg(kInfo, "Factory::TempDirCleanup() occupade disk space == %f", oc);
            if (oc > m_configuration.m_hwm) {
                bytesToRemove = fsstat.f_bsize*fsstat.f_blocks*(oc - m_configuration.m_lwm);
                aMsg(kInfo, "Factory::TempDirCleanup() need space for  %lld bytes", bytesToRemove);
            }
        }

        if (bytesToRemove > 0)
        {
            typedef std::map<std::string, time_t> fcmap_t;
            fcmap_t fcmap;
            // make a sorted map of file patch by access time
            if (dh->Opendir(m_configuration.m_cache_dir.c_str(), env) >= 0) {
                FillFileMapRecurse(dh, m_configuration.m_cache_dir, fcmap);

                // loop over map and remove files with highest value of access time
                for (fcmap_t::iterator i = fcmap.begin(); i != fcmap.end(); ++i)
                {  
                    std::string path = i->first;
                    // remove info file
                    if (oss->Stat(path.c_str(), &fstat) == XrdOssOK)
                    {
                        bytesToRemove -= fstat.st_size;
                        oss->Unlink(path.c_str());
                        aMsg(kInfo, "Factory::TempDirCleanup() removed %s size %lld ", path.c_str(), fstat.st_size);
                    }

                    // remove data file
                    path = path.substr(0, path.size() - strlen(XrdFileCache::Info::m_infoExtension));
                    if (oss->Stat(path.c_str(), &fstat) == XrdOssOK)
                    {
                        bytesToRemove -= fstat.st_size;
                        oss->Unlink(path.c_str());
                        aMsg(kInfo, "Factory::TempDirCleanup() removed %s size %lld ", path.c_str(), fstat.st_size);
                    }
                    if (bytesToRemove <= 0) 
                        break;
                }
            }
        }
        sleep(sleept);
    }
    dh->Close();
    delete dh; dh =0;
}
   
