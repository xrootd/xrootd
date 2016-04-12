#include "XrdFileCache.hh"

#include "XrdOss/XrdOss.hh"
#include "XrdOss/XrdOssCache.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOuca2x.hh"

#include "XrdOfs/XrdOfsConfigPI.hh"
#include "XrdVersion.hh"

#include <fcntl.h>
using namespace XrdFileCache;

XrdVERSIONINFO(XrdOucGetCache2, XrdFileCache);
/* Function: xdlib

   Purpose:  To parse the directive: decisionlib <path> [<parms>]

             <path>  the path of the decision library to be used.
             <parms> optional parameters to be passed.


   Output: true upon success or false upon failure.
 */
bool Cache::xdlib(XrdOucStream &Config)
{
   const char*  val;

   std::string libp;
   if (!(val = Config.GetWord()) || !val[0])
   {
      clLog()->Info(XrdCl::AppMsg, " Cache::Config() decisionlib not specified; always caching files");
      return true;
   }
   else
   {
      libp = val;
   }

   const char* params;
   params = (val[0]) ?  Config.GetWord() : 0;

   XrdOucPinLoader* myLib = new XrdOucPinLoader(&m_log, 0, "decisionlib",
                                                libp.c_str());

   Decision *(*ep)(XrdSysError&);
   ep = (Decision *(*)(XrdSysError&))myLib->Resolve("XrdFileCacheGetDecision");
   if (!ep) {myLib->Unload(true); return false;}

   Decision * d = ep(m_log);
   if (!d)
   {
      clLog()->Error(XrdCl::AppMsg, "Cache::Config() decisionlib was not able to create a decision object");
      return false;
   }
   if (params)
      d->ConfigDecision(params);

   m_decisionpoints.push_back(d);
   clLog()->Info(XrdCl::AppMsg, "Cache::Config() successfully created decision lib from %s", libp.c_str());
   return true;
}

//______________________________________________________________________________

bool Cache::Config(XrdSysLogger *logger, const char *config_filename, const char *parameters)
{
   m_log.logger(logger);

   const char * cache_env;
   if (!(cache_env = getenv("XRDPOSIX_CACHE")) || !*cache_env)
      XrdOucEnv::Export("XRDPOSIX_CACHE", "mode=s&optwr=0");

   XrdOucEnv myEnv;
   XrdOucStream Config(&m_log, getenv("XRDINSTANCE"), &myEnv, "=====> ");

   if (!config_filename || !*config_filename)
   {
      clLog()->Warning(XrdCl::AppMsg, "Cache::Config() configuration file not specified.");
      return false;
   }

   int fd;
   if ( (fd = open(config_filename, O_RDONLY, 0)) < 0)
   {
      clLog()->Error(XrdCl::AppMsg, "Cache::Config() can't open configuration file %s", config_filename);
      return false;
   }

   Config.Attach(fd);

   // Obtain plugin configurator
   XrdOfsConfigPI *ofsCfg = XrdOfsConfigPI::New(config_filename,&Config,&m_log,
                                                &XrdVERSIONINFOVAR(XrdOucGetCache2));
   if (!ofsCfg) return false;


   if (ofsCfg->Load(XrdOfsConfigPI::theOssLib))  {
      ofsCfg->Plugin(m_output_fs);
      XrdOssCache_FS* ocfs = XrdOssCache::Find("public");
      ocfs->Add(m_configuration.m_cache_dir.c_str());
   }
   else
   {
      clLog()->Error(XrdCl::AppMsg, "Cache::Config() Unable to create an OSS object");
      m_output_fs = 0;
      return false;
   }


   // Actual parsing of the config file.
   bool retval = true;
   char *var;
   while((var = Config.GetMyFirstWord()))
   {
      if (!strcmp(var,"pfc.osslib"))
      {
         ofsCfg->Parse(XrdOfsConfigPI::theOssLib);
      }
      else if (!strcmp(var,"pfc.decisionlib"))
      {
         xdlib(Config);
      }
      else if (!strncmp(var,"pfc.", 4))
      {
         retval = ConfigParameters(std::string(var+4), Config);
      }

      if (!retval)
      {
         retval = false;
         clLog()->Error(XrdCl::AppMsg, "Cache::Config() error in parsing");
         break;
      }

   }

   Config.Close();
   // sets default value for disk usage
   if (m_configuration.m_diskUsageLWM < 0 || m_configuration.m_diskUsageHWM < 0)
   {
      XrdOssVSInfo sP;
      if (m_output_fs->StatVS(&sP, "public", 1) >= 0) {
         m_configuration.m_diskUsageLWM = static_cast<long long>(0.90 * sP.Total + 0.5);
         m_configuration.m_diskUsageHWM = static_cast<long long>(0.95 * sP.Total + 0.5);
         clLog()->Debug(XrdCl::AppMsg, "Default disk usage [%lld, %lld]", m_configuration.m_diskUsageLWM, m_configuration.m_diskUsageHWM);
      }
   }

   // get number of available RAM blocks after process configuration
   m_configuration.m_NRamBuffers = static_cast<int>(m_configuration.m_RamAbsAvailable/ m_configuration.m_bufferSize);
   if (retval)
   {
      int loff = 0;
      char buff[2048];
      loff = snprintf(buff, sizeof(buff), "result\n"
               "\tpfc.blocksize %lld\n"
               "\tpfc.prefetch %d\n"
               "\tpfc.nramblocks %d\n\n",
               m_configuration.m_bufferSize,
               m_configuration.m_prefetch, // AMT not sure what parsing should be
               m_configuration.m_NRamBuffers );

      if (m_configuration.m_hdfsmode)
      {
         char buff2[512];
         snprintf(buff2, sizeof(buff2), "\tpfc.hdfsmode hdfsbsize %lld\n", m_configuration.m_hdfsbsize);
         loff += snprintf(&buff[loff], strlen(buff2), "%s", buff2);
      } 

      char  unameBuff[256];
      if (m_configuration.m_username.empty()) {
	XrdOucUtils::UserName(getuid(), unameBuff, sizeof(unameBuff));
	m_configuration.m_username = unameBuff;
      }
      else {
        snprintf(unameBuff, sizeof(unameBuff), "\tpfc.user %s \n", m_configuration.m_username.c_str());
        loff += snprintf(&buff[loff], strlen(unameBuff), "%s", unameBuff);
      }
     
      m_log.Emsg("Config", buff);
   }

   m_log.Emsg("Config", "Configuration =  ", retval ? "Success" : "Fail");

   if (ofsCfg) delete ofsCfg;
   return retval;
}

//______________________________________________________________________________


bool Cache::ConfigParameters(std::string part, XrdOucStream& config )
{   
   printf("part %s \n", part.c_str());
   XrdSysError err(0, "");
   if ( part == "user" )
   {
      m_configuration.m_username = config.GetWord();
   }
   else if ( part == "cachedir" )
   {
      m_configuration.m_cache_dir = config.GetWord();
   }
   else if ( part == "diskusage" )
   {
      std::string minV = config.GetWord();
      std::string maxV = config.GetWord();
      if (!minV.empty() && !maxV.empty()) {
         XrdOssVSInfo sP;
         if (m_output_fs->StatVS(&sP, "public", 1) >= 0)
         {
            if (::isalpha(*(minV.rbegin())) && ::isalpha(*(minV.rbegin()))) {
               if ( XrdOuca2x::a2sz(m_log, "Error getting disk usage low watermark",  minV.c_str(), &m_configuration.m_diskUsageLWM, 0, sP.Total) 
                 || XrdOuca2x::a2sz(m_log, "Error getting disk usage high watermark", maxV.c_str(), &m_configuration.m_diskUsageHWM, 0, sP.Total))
               {
                  return false;
               }
            }
            else 
            {
               char* eP;
               errno = 0;
               float lwmf = strtod(minV.c_str(), &eP);
               if (errno || eP == minV.c_str()) {
                    m_log.Emsg("Factory::ConfigParameters() error parsing diskusage parameter ", minV.c_str());
                    return false;
               }
               float hwmf = strtod(maxV.c_str(), &eP);
               if (errno || eP == maxV.c_str()) {
                  m_log.Emsg("Factory::ConfigParameters() error parsing diskusage parameter ", maxV.c_str());
                  return false;
               }

               m_configuration.m_diskUsageLWM = static_cast<long long>(sP.Total * lwmf + 0.5);
               m_configuration.m_diskUsageHWM = static_cast<long long>(sP.Total * hwmf + 0.5);
            }
         }
      }
   }
   else if  ( part == "blocksize" )
   {
      long long minBSize = 64 * 1024;
      long long maxBSize = 16 * 1024 * 1024;
      if ( XrdOuca2x::a2sz(m_log, "get block size", config.GetWord(), &m_configuration.m_bufferSize, minBSize, maxBSize))
      {
         return false;
      }
   }
   else if (part == "prefetch" )
   {
       const char* params =  config.GetWord();
       if (params) {
           int p = ::atoi(config.GetWord());
           if (p > 0) {
               printf("prefetch enabled, max blocks per file=%d\n", p);
               m_configuration.m_prefetch_max_blocks = p;
           } else {
               m_log.Emsg("Config", "Prefetch is disabled");
               m_configuration.m_prefetch_max_blocks = 0;
           }
       }
       {
           m_log.Emsg("Config", "Error setting prefetch level.");
           return false;
       }
   }
   else if (part == "nram" )
   {
      long long minRAM = 1024* 1024 * 1024;;
      long long maxRAM = 100 * minRAM;
      if ( XrdOuca2x::a2sz(m_log, "get RAM available", config.GetWord(), &m_RamAbsAvailable, minRAM, maxRAM))
      {
         return false;
      }
   }
   else if ( part == "hdfsmode" )
   {
      m_configuration.m_hdfsmode = true;

      const char* params =  config.GetWord();
      if (params) {
         if (!strncmp("hdfsbsize", params, 9)) {
            long long minBlSize = 128 * 1024;
            long long maxBlSize = 1024 * 1024 * 1024;
            params = config.GetWord();
            if ( XrdOuca2x::a2sz(m_log, "Error getting file fragment size", params, &m_configuration.m_hdfsbsize, minBlSize, maxBlSize))
            {
               return false;
            }
         }
         else {
            m_log.Emsg("Config", "Error setting the fragment size parameter name");
            return false;        
         }
      }         
   }
   else
   {
      m_log.Emsg("Cache::ConfigParameters() unmatched pfc parameter", part.c_str());
      return false;
   }

   assert ( config.GetWord() == 0 && "Cache::ConfigParameters() lost argument"); 

   return true;
}
