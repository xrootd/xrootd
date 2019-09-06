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

bool Cache::cfg2bytes(const std::string &str, long long &store, long long totalSpace, const char *name)
{
   char errStr[1024];
   snprintf(errStr, 1024, "Cache::ConfigParameters() Error parsing parameter %s", name);

   if (::isalpha(*(str.rbegin())))
   {
      if (XrdOuca2x::a2sz(m_log, errStr, str.c_str(), &store, 0, totalSpace))
      {
         return false;
      }
   }
   else
   {
      char *eP;
      errno = 0;
      double frac = strtod(str.c_str(), &eP);
      if (errno || eP == str.c_str())
      {
         m_log.Emsg(errStr, str.c_str());
         return false;
      }

      store = static_cast<long long>(totalSpace * frac + 0.5);
   }

   if (store < 0 || store > totalSpace)
   {
     snprintf(errStr, 1024, "Cache::ConfigParameters() Error: parameter %s should be between 0 and total available disk space (%lld) - it is %lld (given as %s)",
              name, totalSpace, store, str.c_str());
     m_log.Emsg(errStr, "");
     return false;
   }

   return true;
}

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
/* Function: Config

   Purpose: To parse configuration file and configure Cache instance.
   Output:  true upon success or false upon failure.
 */
bool Cache::Config(const char *config_filename, const char *parameters)
{
   // Indicate whether or not we are a client instance
   const char *theINS = getenv("XRDINSTANCE");
   m_isClient = (theINS != 0 && strncmp("*client ", theINS, 8) == 0);

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

   // Obtain OFS configurator for OSS plugin.
   XrdOfsConfigPI *ofsCfg = XrdOfsConfigPI::New(config_filename,&Config,&m_log,
                                                &XrdVERSIONINFOVAR(XrdOucGetCache2));
   if (! ofsCfg) return false;

   TmpConfiguration tmpc;

   // Adjust default parameters for client/serverless caching
   if (m_isClient)
   {
      m_configuration.m_bufferSize     = 256 * 1024;
      m_configuration.m_wqueue_blocks  = 8;
      m_configuration.m_wqueue_threads = 1;
   }

   // Actual parsing of the config file.
   bool retval = true;
   char *var;
   while ((var = Config.GetMyFirstWord()))
   {
      if (! strcmp(var,"pfc.osslib"))
      {
         retval = ofsCfg->Parse(XrdOfsConfigPI::theOssLib);
      }
      else if (! strcmp(var,"pfc.decisionlib"))
      {
         retval = xdlib(Config);
      }
      else if (! strcmp(var,"pfc.trace"))
      {
         retval = xtrace(Config);
      }
      else if (! strcmp(var,"pfc.allow_xrdpfc_command"))
      {
         m_configuration.m_allow_xrdpfc_command = true;
      }
      else if (! strncmp(var,"pfc.", 4))
      {
         retval = ConfigParameters(std::string(var+4), Config, tmpc);
      }

      if ( ! retval)
      {
         TRACE(Error, "Cache::Config() error in parsing");
         break;
      }
   }

   Config.Close();

   // Load OSS plugin.
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

   // sets default value for disk usage
   XrdOssVSInfo sP;
   {
      if (m_output_fs->StatVS(&sP, m_configuration.m_data_space.c_str(), 1) < 0)
      {
         m_log.Emsg("Cache::ConfigParameters()", "error obtaining stat info for data space ", m_configuration.m_data_space.c_str());
         return false;
      }
      if (sP.Total < 10ll << 20)
      {
         m_log.Emsg("Cache::ConfigParameters()", "available data space is less than 10 MB (can be due to a mistake in oss.localroot directive) for space ", m_configuration.m_data_space.c_str());
         return false;
      }

      m_configuration.m_diskTotalSpace = sP.Total;

      if (cfg2bytes(tmpc.m_diskUsageLWM, m_configuration.m_diskUsageLWM, sP.Total, "lowWatermark") &&
          cfg2bytes(tmpc.m_diskUsageHWM, m_configuration.m_diskUsageHWM, sP.Total, "highWatermark"))
      {
         if (m_configuration.m_diskUsageLWM >= m_configuration.m_diskUsageHWM) {
            printf("GGGG %lld %lld\n", m_configuration.m_diskUsageLWM, m_configuration.m_diskUsageHWM);
            m_log.Emsg("Cache::ConfigParameters()", "pfc.diskusage should have lowWatermark < highWatermark.");
            retval = false;
         }
      }
      else retval = false;

      if ( ! tmpc.m_fileUsageMax.empty())
      {
        if (cfg2bytes(tmpc.m_fileUsageBaseline, m_configuration.m_fileUsageBaseline, sP.Total, "files baseline") &&
            cfg2bytes(tmpc.m_fileUsageNominal,  m_configuration.m_fileUsageNominal,  sP.Total, "files nominal")  &&
            cfg2bytes(tmpc.m_fileUsageMax,      m_configuration.m_fileUsageMax,      sP.Total, "files max"))
        {
          if (m_configuration.m_fileUsageBaseline >= m_configuration.m_fileUsageNominal ||
              m_configuration.m_fileUsageBaseline >= m_configuration.m_fileUsageMax     ||
              m_configuration.m_fileUsageNominal  >= m_configuration.m_fileUsageMax)
          {
            m_log.Emsg("Cache::ConfigParameters()", "pfc.diskusage files should have baseline < nominal < max.");
            retval = false;
          }
        }
        else retval = false;
      }
   }
   // sets flush frequency
   if ( ! tmpc.m_flushRaw.empty())
   {
      if (::isalpha(*(tmpc.m_flushRaw.rbegin())))
      {
         if (XrdOuca2x::a2sz(m_log, "Error getting number of bytes written before flush",  tmpc.m_flushRaw.c_str(),
                             &m_configuration.m_flushCnt,
                             100 * m_configuration.m_bufferSize , 100000 * m_configuration.m_bufferSize))
         {
            return false;
         }
         m_configuration.m_flushCnt /= m_configuration.m_bufferSize;
      }
      else
      {
         if (XrdOuca2x::a2ll(m_log, "Error getting number of blocks written before flush", tmpc.m_flushRaw.c_str(),
                             &m_configuration.m_flushCnt, 100, 100000))
         {
            return false;
         }
      }
   }

   
   // get number of available RAM blocks after process configuration
   if (m_configuration.m_RamAbsAvailable == 0)
   {
      m_configuration.m_RamAbsAvailable = m_isClient ? 256ll * 1024 * 1024 : 1024ll * 1024 * 1024;
      char buff[1024];
      snprintf(buff, sizeof(buff), "RAM usage pfc.ram is not specified. Default value %s is used.", m_isClient ? "256m" : "1g");
      m_log.Say("Config info: ", buff);
   }
   m_configuration.m_NRamBuffers = static_cast<int>(m_configuration.m_RamAbsAvailable / m_configuration.m_bufferSize);
   

   // Set tracing to debug if this is set in environment
   char* cenv = getenv("XRDDEBUG");
   if (cenv && ! strcmp(cenv,"1") && m_trace->What < 4) m_trace->What = 4;

   if (retval)
   {
      int loff = 0;
      char buff[2048];
      float rg =  (m_configuration.m_RamAbsAvailable)/float(1024*1024*1024);
      loff = snprintf(buff, sizeof(buff), "Config effective %s pfc configuration:\n"
                      "       pfc.blocksize %lld\n"
                      "       pfc.prefetch %d\n"
                      "       pfc.ram %.fg\n"
                      "       pfc.writequeue %d %d\n"
                      "       # Total available disk: %lld\n"
                      "       pfc.diskusage %lld %lld files %lld %lld %lld purgeinterval %d purgecoldfiles %d\n"
                      "       pfc.spaces %s %s\n"
                      "       pfc.trace %d\n"
                      "       pfc.flush %lld",
                      config_filename,
                      m_configuration.m_bufferSize,
                      m_configuration.m_prefetch_max_blocks,
                      rg,
                      m_configuration.m_wqueue_blocks, m_configuration.m_wqueue_threads,
                      sP.Total,
                      m_configuration.m_diskUsageLWM, m_configuration.m_diskUsageHWM,
                      m_configuration.m_fileUsageBaseline, m_configuration.m_fileUsageNominal, m_configuration.m_fileUsageMax,
                      m_configuration.m_purgeInterval, m_configuration.m_purgeColdFilesAge,
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

      m_log.Say(buff);
   }

   m_log.Say("------ File Caching Proxy interface initialization ", retval ? "completed" : "failed");

   if (ofsCfg) delete ofsCfg;

   return retval;
}

//______________________________________________________________________________


bool Cache::ConfigParameters(std::string part, XrdOucStream& config, TmpConfiguration &tmpc)
{
   struct ConfWordGetter
   {
      XrdOucStream &m_config;
      char         *m_last_word;

      ConfWordGetter(XrdOucStream& c) : m_config(c), m_last_word((char*)1) {}

      const char* GetWord() { if (HasLast()) m_last_word = m_config.GetWord(); return HasLast() ? m_last_word : ""; }
      bool        HasLast() { return (m_last_word != 0); }
   };

   ConfWordGetter cwg(config);

   XrdSysError err(0, "");
   if ( part == "user" )
   {
      m_configuration.m_username = cwg.GetWord();
      if ( ! cwg.HasLast())
      {
         m_log.Emsg("Config", "Error: pfc.user requires a parameter.");
         return false;
      }
   }
   else if ( part == "diskusage" )
   {
      tmpc.m_diskUsageLWM = cwg.GetWord();
      tmpc.m_diskUsageHWM = cwg.GetWord();

      if (tmpc.m_diskUsageHWM.empty())
      {
         m_log.Emsg("Config", "Error: pfc.diskusage parameter requires at least two arguments.");
         return false;
      }

      const char *p = 0;
      while ((p = cwg.GetWord()) && cwg.HasLast())
      {
         if (strcmp(p, "files") == 0)
         {
            tmpc.m_fileUsageBaseline = cwg.GetWord();
            tmpc.m_fileUsageNominal  = cwg.GetWord();
            tmpc.m_fileUsageMax      = cwg.GetWord();

            if ( ! cwg.HasLast())
            {
               m_log.Emsg("Config", "Error: pfc.diskusage files directive requires three arguments.");
               return false;
            }
         }
         else if (strcmp(p, "sleep") == 0 || strcmp(p, "purgeinterval") == 0)
         {
            if (strcmp(p, "sleep") == 0) m_log.Emsg("Config", "warning sleep directive is deprecated in pfc.diskusage. Please use purgeinterval instead.");

            if (XrdOuca2x::a2tm(m_log, "Error getting purgeinterval", cwg.GetWord(), &m_configuration.m_purgeInterval, 60, 3600))
            {
               return false;
            }
         }
         else if (strcmp(p, "purgecoldfiles") == 0)
         {
            if (XrdOuca2x::a2tm(m_log, "Error getting purgecoldfiles age ", cwg.GetWord(), &m_configuration.m_purgeColdFilesAge, 3600, 3600*24*360))
            {
               return false;
            }
            if (XrdOuca2x::a2i(m_log, "Error getting purgecoldfiles period", cwg.GetWord(), &m_configuration.m_purgeColdFilesPeriod, 1, 1000))
            {
               return false;
            }
         }
         else
         {
            m_log.Emsg("Config", "Error: diskusage stanza contains unknown directive", p);
         }
      }
   }
   else if  ( part == "blocksize" )
   {
      long long minBSize =   4 * 1024;
      long long maxBSize = 512 * 1024 * 1024;
      if (XrdOuca2x::a2sz(m_log, "get block size", cwg.GetWord(), &m_configuration.m_bufferSize, minBSize, maxBSize))
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

      if (XrdOuca2x::a2i(m_log, "Error setting prefetch block count", cwg.GetWord(), &m_configuration.m_prefetch_max_blocks, 0, 128))
      {
         return false;
      }

   }
   else if ( part == "nramread" )
   {
      m_log.Emsg("Config", "pfc.nramread is deprecated, please use pfc.ram instead. Ignoring this directive.");
      cwg.GetWord(); // Ignoring argument.
   }
   else if ( part == "ram" )
   {
      long long minRAM = m_isClient ? 256 * 1024 * 1024 : 1024 * 1024 * 1024;
      long long maxRAM = 256 * minRAM;
      if ( XrdOuca2x::a2sz(m_log, "get RAM available", cwg.GetWord(), &m_configuration.m_RamAbsAvailable, minRAM, maxRAM))
      {
         return false;
      }
   }
   else if ( part == "writequeue")
   {
      if (XrdOuca2x::a2i(m_log, "Error getting pfc.writequeue num-blocks", cwg.GetWord(), &m_configuration.m_wqueue_blocks, 1, 1024))
      {
         return false;
      }
      if (XrdOuca2x::a2i(m_log, "Error getting pfc.writequeue num-threads", cwg.GetWord(), &m_configuration.m_wqueue_threads, 1, 64))
      {
         return false;
      }
   }
   else if ( part == "spaces" )
   {
      m_configuration.m_data_space = cwg.GetWord();
      m_configuration.m_meta_space = cwg.GetWord();
      if ( ! cwg.HasLast())
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

      const char* params = cwg.GetWord();
      if (params)
      {
         if (! strncmp("hdfsbsize", params, 9))
         {
            long long minBlSize =  32 * 1024;
            long long maxBlSize = 128 * 1024 * 1024;
            if ( XrdOuca2x::a2sz(m_log, "Error getting file fragment size", cwg.GetWord(), &m_configuration.m_hdfsbsize, minBlSize, maxBlSize))
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
      tmpc.m_flushRaw = cwg.GetWord();
      if ( ! cwg.HasLast())
      {
         m_log.Emsg("Config", "Error: pfc.flush requires a parameter.");
         return false;
      }
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
   schedP = (XrdScheduler *) theEnv.GetPtr("XrdScheduler*");
}
