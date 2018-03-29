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

   char params[4096];
   if (val[0])
      Config.GetRest(params, 4096);
   else
      params[0] = 0;

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
   if (params[0])
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
   const char *theINS = getenv("XRDINSTANCE");

// Indicate whether or not we are a client instance
//
   m_isClient = (strncmp("*client ", theINS, 8) != 0);

   XrdOucEnv myEnv;
   XrdOucStream Config(&m_log, theINS, &myEnv, "=====> ");

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

   // minimize buffersize in case of client caching
   if ( m_isClient) {
      m_configuration.m_bufferSize = 256 * 1024 * 124;
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

   // sets flush frequency
   {
      if (::isalpha(*(tmpc.m_flushRaw.rbegin())))
      {
         if (XrdOuca2x::a2sz(m_log, "Error getting number of blocks to flush",  tmpc.m_flushRaw.c_str(), &m_configuration.m_flushCnt, 100*m_configuration.m_bufferSize , 5000*m_configuration.m_bufferSize))
         {
            return false;
         }
         m_configuration.m_flushCnt /= m_configuration.m_bufferSize;
      }
      else
      {
         m_configuration.m_flushCnt = ::atol(tmpc.m_flushRaw.c_str());
      }
   }

   
   // get number of available RAM blocks after process configuration
   if (m_configuration.m_RamAbsAvailable == 0)
   {
      m_configuration.m_RamAbsAvailable = m_isClient ? 256ll * 1024 * 1024 : 1024 * 1024 * 1024;
      char buff2[1024];
      snprintf(buff2, sizeof(buff2), "RAM usage is not specified. Default value %s is used.", m_isClient ? "256m" : "8g");
      TRACE(Warning, buff2);
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
                      "       pfc.trace %d\n"
                      "       pfc.flush %lld",
                      config_filename,
                      m_configuration.m_bufferSize,
                      m_configuration.m_prefetch_max_blocks,
                      rg,
                      m_configuration.m_diskUsageLWM,
                      m_configuration.m_diskUsageHWM,
                      m_configuration.m_purgeInterval,
                      m_configuration.m_data_space.c_str(),
                      m_configuration.m_meta_space.c_str(),
                      m_trace->What,
                      m_configuration.m_flushCnt);



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
         loff += snprintf(buff + loff, sizeof(buff) - loff, "%s", unameBuff);
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
   else if ( part == "prefetch" || part == "nramprefetch" )
   {
      if (part == "nramprefetch")
      {
         m_log.Emsg("Config", "pfc.nramprefetch is deprecated, please use pfc.prefetch instead. Replacing the directive internally.");
      }

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
   else if ( part == "nramread" )
   {
      m_log.Emsg("Config", "pfc.nramread is deprecated, please use pfc.ram instead. Ignoring this directive.");
      config.GetWord(); // Ignoring argument.
   }
   else if ( part == "ram" )
   {
      long long minRAM = m_isClient ? 256 * 1024 * 1024 : 1024 * 1024 * 1024;
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
   else if ( part == "hdfsmode" || part == "filefragmentmode" )
   {
      if (part == "filefragmentmode")
      {
         m_log.Emsg("Config", "pfc.filefragmentmode is deprecated, please use pfc.hdfsmode instead. Replacing the directive internally.");
      }
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
   else if ( part == "flush" )
   {
      tmpc.m_flushRaw = config.GetWord();
   }
   else
   {
      m_log.Emsg("Cache::ConfigParameters() unmatched pfc parameter", part.c_str());
      return false;
   }

   return true;
}

//______________________________________________________________________________


void Cache::EnvInfo(XrdOucEnv &theEnv)
{
// Extract out the pointer to the scheduler
//
   schedP = (XrdScheduler *)theEnv.GetPtr("XrdScheduler*");
}
