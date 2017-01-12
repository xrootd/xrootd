#include "XrdFileCache.hh"
#include "XrdFileCacheTrace.hh"

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
   if (! (val = Config.GetWord()) || ! val[0])
   {
      TRACE(Info," Cache::Config() decisionlib not specified; always caching files");
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
   if (! ep) {myLib->Unload(true); return false; }

   Decision * d = ep(m_log);
   if (! d)
   {
      TRACE(Error, "Cache::Config() decisionlib was not able to create a decision object");
      return false;
   }
   if (params)
      d->ConfigDecision(params);

   m_decisionpoints.push_back(d);
   return true;
}

/* Function: xtrace

   Purpose:  To parse the directive: trace <level>
   Output: true upon success or false upon failure.
 */
bool Cache::xtrace(XrdOucStream &Config)
{
   char  *val;
   static struct traceopts {const char *opname; int opval; } tropts[] =
   {
      {"none",    0},
      {"error",   1},
      {"warning", 2},
      {"info",    3},
      {"debug",   4},
      {"dump",    5}
   };
   int numopts = sizeof(tropts)/sizeof(struct traceopts);

   if (! (val = Config.GetWord()))
   {m_log.Emsg("Config", "trace option not specified"); return 1; }

   for (int i = 0; i < numopts; i++)
   {
      if (! strcmp(val, tropts[i].opname))
      {
         m_trace->What = tropts[i].opval;
         return true;
      }
   }
   return false;
}

//______________________________________________________________________________

bool Cache::Config(XrdSysLogger *logger, const char *config_filename, const char *parameters)
{
   m_log.logger(logger);

   const char * cache_env;
   if (! (cache_env = getenv("XRDPOSIX_CACHE")) || ! *cache_env)
      XrdOucEnv::Export("XRDPOSIX_CACHE", "mode=s&optwr=0");

   XrdOucEnv myEnv;
   XrdOucStream Config(&m_log, getenv("XRDINSTANCE"), &myEnv, "=====> ");

   if (! config_filename || ! *config_filename)
   {
      TRACE(Error, "Cache::Config() configuration file not specified.");
      return false;
   }

   int fd;
   if ( (fd = open(config_filename, O_RDONLY, 0)) < 0)
   {
      TRACE( Error, "Cache::Config() can't open configuration file " << config_filename);
      return false;
   }

   Config.Attach(fd);

   // Obtain plugin configurator
   XrdOfsConfigPI *ofsCfg = XrdOfsConfigPI::New(config_filename,&Config,&m_log,
                                                &XrdVERSIONINFOVAR(XrdOucGetCache2));
   if (! ofsCfg) return false;

   TmpConfiguration tmpc;

   if (ofsCfg->Load(XrdOfsConfigPI::theOssLib))
   {
      ofsCfg->Plugin(m_output_fs);
   }
   else
   {
      TRACE(Error, "Cache::Config() Unable to create an OSS object");
      m_output_fs = 0;
      return false;
   }


   // Actual parsing of the config file.
   bool retval = true;
   char *var;
   while((var = Config.GetMyFirstWord()))
   {
      if (! strcmp(var,"pfc.osslib"))
      {
         ofsCfg->Parse(XrdOfsConfigPI::theOssLib);
      }
      else if (! strcmp(var,"pfc.decisionlib"))
      {
         retval = xdlib(Config);
      }
      else if (! strcmp(var,"pfc.trace"))
      {
         retval = xtrace(Config);
      }
      else if (! strncmp(var,"pfc.", 4))
      {
         retval = ConfigParameters(std::string(var+4), Config, tmpc);
      }

      if ( ! retval)
      {
         retval = false;
         TRACE(Error, "Cache::Config() error in parsing");
         break;
      }

   }

   Config.Close();

   // sets default value for disk usage
   {
      XrdOssVSInfo sP;
      if (m_output_fs->StatVS(&sP, m_configuration.m_data_space.c_str(), 1) < 0)
      {
         m_log.Emsg("Cache::ConfigParameters() error obtaining stat info for space ", m_configuration.m_data_space.c_str());
         return false;
      }

      if (::isalpha(*(tmpc.m_diskUsageLWM.rbegin())) && ::isalpha(*(tmpc.m_diskUsageHWM.rbegin())))
      {
         if (XrdOuca2x::a2sz(m_log, "Error getting disk usage low watermark",  tmpc.m_diskUsageLWM.c_str(), &m_configuration.m_diskUsageLWM, 0, sP.Total) ||
             XrdOuca2x::a2sz(m_log, "Error getting disk usage high watermark", tmpc.m_diskUsageHWM.c_str(), &m_configuration.m_diskUsageHWM, 0, sP.Total))
         {
            return false;
         }
      }
      else
      {
         char* eP;
         errno = 0;
         double lwmf = strtod(tmpc.m_diskUsageLWM.c_str(), &eP);
         if (errno || eP == tmpc.m_diskUsageLWM.c_str())
         {
            m_log.Emsg("Cache::ConfigParameters() error parsing diskusage parameter ", tmpc.m_diskUsageLWM.c_str());
            return false;
         }
         double hwmf = strtod(tmpc.m_diskUsageHWM.c_str(), &eP);
         if (errno || eP == tmpc.m_diskUsageHWM.c_str())
         {
            m_log.Emsg("Cache::ConfigParameters() error parsing diskusage parameter ", tmpc.m_diskUsageHWM.c_str());
            return false;
         }

         m_configuration.m_diskUsageLWM = static_cast<long long>(sP.Total * lwmf + 0.5);
         m_configuration.m_diskUsageHWM = static_cast<long long>(sP.Total * hwmf + 0.5);
      }
   }

   // get number of available RAM blocks after process configuration
   if (m_configuration.m_RamAbsAvailable == 0)
   {
      TRACE(Error, "RAM usage not specified. Please set pfc.ram value in configuration file.");
      return false;
   }
   m_configuration.m_NRamBuffers = static_cast<int>(m_configuration.m_RamAbsAvailable/ m_configuration.m_bufferSize);

   // Set tracing to debug if this is set in environment
   char* cenv = getenv("XRDDEBUG");
   if (cenv && ! strcmp(cenv,"1")) m_trace->What = 4;

   if (retval)
   {
      int loff = 0;
      char buff[2048];
      float rg =  (m_configuration.m_RamAbsAvailable)/float(1024*1024*1024);
      loff = snprintf(buff, sizeof(buff), "Config effective %s pfc configuration:\n"
                      "       pfc.blocksize %lld\n"
                      "       pfc.prefetch %zu\n"
                      "       pfc.ram %.fg\n"
                      "       pfc.diskusage %lld %lld sleep %d\n"
                      "       pfc.spaces %s %s\n"
                      "       pfc.trace %d",
                      config_filename,
                      m_configuration.m_bufferSize,
                      m_configuration.m_prefetch_max_blocks,
                      rg,
                      m_configuration.m_diskUsageLWM,
                      m_configuration.m_diskUsageHWM,
                      m_configuration.m_purgeInterval,
                      m_configuration.m_data_space.c_str(),
                      m_configuration.m_meta_space.c_str(),
                      m_trace->What);

      if (m_configuration.m_hdfsmode)
      {
         char buff2[512];
         snprintf(buff2, sizeof(buff2), "\tpfc.hdfsmode hdfsbsize %lld\n", m_configuration.m_hdfsbsize);
         loff += snprintf(&buff[loff], strlen(buff2), "%s", buff2);
      }

      char unameBuff[256];
      if (m_configuration.m_username.empty())
      {
         XrdOucUtils::UserName(getuid(), unameBuff, sizeof(unameBuff));
         m_configuration.m_username = unameBuff;
      }
      else
      {
         snprintf(unameBuff, sizeof(unameBuff), "\tpfc.user %s \n", m_configuration.m_username.c_str());
         loff += snprintf(&buff[loff], strlen(unameBuff), "%s", unameBuff);
      }

      m_log.Say( buff);
   }

   m_log.Say("------ File Caching Proxy interface initialization ", retval ? "completed" : "failed");

   if (ofsCfg) delete ofsCfg;

   return retval;
}

//______________________________________________________________________________


bool Cache::ConfigParameters(std::string part, XrdOucStream& config, TmpConfiguration &tmpc)
{
   XrdSysError err(0, "");
   if ( part == "user" )
   {
      m_configuration.m_username = config.GetWord();
   }
   else if ( part == "diskusage" )
   {
      tmpc.m_diskUsageLWM = config.GetWord();
      tmpc.m_diskUsageHWM = config.GetWord();

      if (tmpc.m_diskUsageHWM.empty())
      {
         m_log.Emsg("Config", "Error: diskusage parameter requires two arguments.");
         return false;
      }
      const char *p = config.GetWord();
      if (p && strcmp(p, "sleep") == 0)
      {
         p = config.GetWord();
         if (XrdOuca2x::a2i(m_log, "Error getting purge interval", p, &m_configuration.m_purgeInterval, 60, 3600))
         {
            return false;
         }
      }
   }
   else if  ( part == "blocksize" )
   {
      long long minBSize = 64 * 1024;
      long long maxBSize = 16 * 1024 * 1024;
      if (XrdOuca2x::a2sz(m_log, "get block size", config.GetWord(), &m_configuration.m_bufferSize, minBSize, maxBSize))
      {
         return false;
      }
   }
   else if ( part == "prefetch" )
   {
      const char* params =  config.GetWord();
      if (params)
      {
         int p = ::atoi(params);
         if (p > 0)
         {
            // m_log.Emsg("prefetch enabled, max blocks per file ", params);
            m_configuration.m_prefetch_max_blocks = p;
         }
         else
         {
            m_log.Emsg("Config", "Prefetch is disabled");
            m_configuration.m_prefetch_max_blocks = 0;
         }
      }
      else
      {
         m_log.Emsg("Config", "Error setting prefetch level.");
         return false;
      }
   }
   else if ( part == "ram" )
   {
      long long minRAM = 1024 * 1024 * 1024;
      long long maxRAM = 256 * minRAM;
      if ( XrdOuca2x::a2sz(m_log, "get RAM available", config.GetWord(), &m_configuration.m_RamAbsAvailable, minRAM, maxRAM))
      {
         return false;
      }
   }
   else if ( part == "spaces" )
   {
      const char *par;
      par = config.GetWord();
      if (par) m_configuration.m_data_space = par;
      par = config.GetWord();
      if (par) m_configuration.m_meta_space = par;
      else
      {
         m_log.Emsg("Config", "spacenames requires two parameters: <data-space> <metadata-space>.");
         return false;
      }
   }
   else if ( part == "hdfsmode" )
   {
      m_configuration.m_hdfsmode = true;

      const char* params = config.GetWord();
      if (params)
      {
         if (! strncmp("hdfsbsize", params, 9))
         {
            long long minBlSize =  32 * 1024;
            long long maxBlSize = 128 * 1024 * 1024;
            params = config.GetWord();
            if ( XrdOuca2x::a2sz(m_log, "Error getting file fragment size", params, &m_configuration.m_hdfsbsize, minBlSize, maxBlSize))
            {
               return false;
            }
         }
         else
         {
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

   assert (config.GetWord() == 0 && "Cache::ConfigParameters() lost argument");

   return true;
}
