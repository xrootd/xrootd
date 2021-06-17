#include "XrdPfc.hh"
#include "XrdPfcTrace.hh"
#include "XrdPfcInfo.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOuca2x.hh"

#include "XrdOfs/XrdOfsConfigPI.hh"
#include "XrdVersion.hh"

#include <fcntl.h>

using namespace XrdPfc;

XrdVERSIONINFO(XrdOucGetCache, XrdPfc);

Configuration::Configuration() :
   m_hdfsmode(false),
   m_allow_xrdpfc_command(false),
   m_data_space("public"),
   m_meta_space("public"),
   m_diskTotalSpace(-1),
   m_diskUsageLWM(-1),
   m_diskUsageHWM(-1),
   m_fileUsageBaseline(-1),
   m_fileUsageNominal(-1),
   m_fileUsageMax(-1),
   m_purgeInterval(300),
   m_purgeColdFilesAge(-1),
   m_purgeAgeBasedPeriod(10),
   m_accHistorySize(20),
   m_dirStatsMaxDepth(-1),
   m_dirStatsStoreDepth(0),
   m_bufferSize(256*1024),
   m_RamAbsAvailable(0),
   m_RamKeepStdBlocks(0),
   m_wqueue_blocks(16),
   m_wqueue_threads(4),
   m_prefetch_max_blocks(10),
   m_hdfsbsize(128*1024*1024),
   m_flushCnt(2000),
   m_cs_UVKeep(-1),
   m_cs_Chk(CSChk_Net),
   m_cs_ChkTLS(false)
{}


bool Cache::cfg2bytes(const std::string &str, long long &store, long long totalSpace, const char *name)
{
   char errStr[1024];
   snprintf(errStr, 1024, "ConfigParameters() Error parsing parameter %s", name);

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
     snprintf(errStr, 1024, "ConfigParameters() Error: parameter %s should be between 0 and total available disk space (%lld) - it is %lld (given as %s)",
              name, totalSpace, store, str.c_str());
     m_log.Emsg(errStr, "");
     return false;
   }

   return true;
}

/* Function: xcschk

   Purpose:  To parse the directive: cschk <parms>

             parms:  [[no]net] [[no]tls] [[no]cache] [uvkeep <arg>]

             all     Checksum check on cache & net transfers.
             cache   Checksum check on cache only, 'no' turns it off.
             net     Checksum check on net transfers 'no' turns it off.
             tls     use TLS if server doesn't support checksums 'no' turns it off.
             uvkeep  Maximum amount of time a cached file make be kept if it
                     contains unverified checksums as n[d|h|m|s], where 'n'
                     is a non-negative integer. A value of 0 prohibits disk
                     caching unless the checksum can be verified. You can
                     also specify "lru" which means the standard purge policy
                     is to be used.

   Output: true upon success or false upon failure.
 */
bool Cache::xcschk(XrdOucStream &Config)
{
   const char *val, *val2;
   struct cschkopts {const char *opname; int opval;} csopts[] =
   {
      {"off",     CSChk_None},
      {"cache",   CSChk_Cache},
      {"net",     CSChk_Net},
      {"tls",     CSChk_TLS}
   };
   int i, numopts = sizeof(csopts)/sizeof(struct cschkopts);
   bool isNo;

   if (! (val = Config.GetWord()))
   {m_log.Emsg("Config", "cschk parameter not specified"); return false; }

   while(val)
   {
      if ((isNo = strncmp(val, "no", 2) == 0))
         val2 = val + 2;
      else
         val2 = val;
      for (i = 0; i < numopts; i++)
      {
         if (!strcmp(val2, csopts[i].opname))
         {
            if (isNo)
               m_configuration.m_cs_Chk &= ~csopts[i].opval;
            else if (csopts[i].opval)
               m_configuration.m_cs_Chk |= csopts[i].opval;
            else
               m_configuration.m_cs_Chk  = csopts[i].opval;
            break;
         }
      }
      if (i >= numopts)
      {
         if (strcmp(val, "uvkeep"))
         {
            m_log.Emsg("Config", "invalid cschk option -", val);
            return false;
         }
         if (!(val = Config.GetWord()))
         {
            m_log.Emsg("Config", "cschk uvkeep value not specified");
            return false;
         }
         if (!strcmp(val, "lru"))
            m_configuration.m_cs_UVKeep = -1;
         else
         {
            int uvkeep;
            if (XrdOuca2x::a2tm(m_log, "uvkeep time", val, &uvkeep, 0))
               return false;
            m_configuration.m_cs_UVKeep = uvkeep;
         }
      }
      val = Config.GetWord();
   }
   // Decompose into separate TLS state, it is only passed on to psx
   m_configuration.m_cs_ChkTLS =  m_configuration.m_cs_Chk & CSChk_TLS;
   m_configuration.m_cs_Chk   &= ~CSChk_TLS;

   m_env->Put("psx.CSNet", m_configuration.is_cschk_net() ? (m_configuration.m_cs_ChkTLS ? "2" : "1") : "0");

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
   ep = (Decision *(*)(XrdSysError&))myLib->Resolve("XrdPfcGetDecision");
   if (! ep) {myLib->Unload(true); return false; }

   Decision * d = ep(m_log);
   if (! d)
   {
      TRACE(Error, "Config() decisionlib was not able to create a decision object");
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
   m_log.Emsg("Config", "invalid trace option -", val);
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
      TRACE(Error, "Config() configuration file not specified.");
      return false;
   }

   int fd;
   if ( (fd = open(config_filename, O_RDONLY, 0)) < 0)
   {
      TRACE( Error, "Config() can't open configuration file " << config_filename);
      return false;
   }

   Config.Attach(fd);
   static const char *cvec[] = { "*** pfc plugin config:", 0 };
   Config.Capture(cvec);

   // Obtain OFS configurator for OSS plugin.
   XrdOfsConfigPI *ofsCfg = XrdOfsConfigPI::New(config_filename,&Config,&m_log,
                                                &XrdVERSIONINFOVAR(XrdOucGetCache));
   if (! ofsCfg) return false;

   TmpConfiguration tmpc;

   // Adjust default parameters for client/serverless caching
   if (m_isClient)
   {
      m_configuration.m_bufferSize     = 256 * 1024;
      m_configuration.m_wqueue_blocks  = 8;
      m_configuration.m_wqueue_threads = 1;
   }

   // If network checksum processing is the default, indicate so.
   if (m_configuration.is_cschk_net()) m_env->Put("psx.CSNet", m_configuration.m_cs_ChkTLS ? "2" : "1");

   // Actual parsing of the config file.
   bool retval = true, aOK = true;
   char *var;
   while ((var = Config.GetMyFirstWord()))
   {
      if (! strcmp(var,"pfc.osslib"))
      {
         retval = ofsCfg->Parse(XrdOfsConfigPI::theOssLib);
      }
      else if (! strcmp(var,"pfc.cschk"))
      {
         retval = xcschk(Config);
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
         TRACE(Error, "Config() error in parsing");
         aOK = false;
      }
   }

   Config.Close();

   // Load OSS plugin.
   myEnv.Put("oss.runmode", "pfc");
   if (m_configuration.is_cschk_cache())
   {
      char csi_conf[128];
      if (snprintf(csi_conf, 128, "space=%s nofill", m_configuration.m_meta_space.c_str()) < 128)
      {
         ofsCfg->Push(XrdOfsConfigPI::theOssLib, "libXrdOssCsi.so", csi_conf);
      } else {
         TRACE(Error, "Config() buffer too small for libXrdOssCsi params.");
         return false;
      }
   }
   if (ofsCfg->Load(XrdOfsConfigPI::theOssLib, &myEnv))
   {
      ofsCfg->Plugin(m_oss);
   }
   else
   {
      TRACE(Error, "Config() Unable to create an OSS object");
      return false;
   }

   // sets default value for disk usage
   XrdOssVSInfo sP;
   {
      if (m_oss->StatVS(&sP, m_configuration.m_data_space.c_str(), 1) < 0)
      {
         m_log.Emsg("ConfigParameters()", "error obtaining stat info for data space ", m_configuration.m_data_space.c_str());
         return false;
      }
      if (sP.Total < 10ll << 20)
      {
         m_log.Emsg("ConfigParameters()", "available data space is less than 10 MB (can be due to a mistake in oss.localroot directive) for space ",
                    m_configuration.m_data_space.c_str());
                    return false;
      }

      m_configuration.m_diskTotalSpace = sP.Total;

      if (cfg2bytes(tmpc.m_diskUsageLWM, m_configuration.m_diskUsageLWM, sP.Total, "lowWatermark") &&
          cfg2bytes(tmpc.m_diskUsageHWM, m_configuration.m_diskUsageHWM, sP.Total, "highWatermark"))
      {
         if (m_configuration.m_diskUsageLWM >= m_configuration.m_diskUsageHWM) {
            m_log.Emsg("ConfigParameters()", "pfc.diskusage should have lowWatermark < highWatermark.");
            aOK = false;
         }
      }
      else aOK = false;

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
            m_log.Emsg("ConfigParameters()", "pfc.diskusage files should have baseline < nominal < max.");
            aOK = false;
          }
        }
        else aOK = false;
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
   // Setup number of standard-size blocks not released back to the system to 5% of total RAM.
   m_configuration.m_RamKeepStdBlocks = (m_configuration.m_RamAbsAvailable / m_configuration.m_bufferSize + 1) * 5 / 100;
   

   // Set tracing to debug if this is set in environment
   char* cenv = getenv("XRDDEBUG");
   if (cenv && ! strcmp(cenv,"1") && m_trace->What < 4) m_trace->What = 4;

   if (aOK)
   {
      int  loff = 0;
//                         000    001            010
      const char *csc[] = {"off", "cache nonet", "nocache net notls",
//                         011
                           "cache net notls",
//                         100    101            110
                           "off", "cache nonet", "nocache net tls",
//                         111
                           "cache net tls"};
      char buff[8192], uvk[32];
      if (m_configuration.m_cs_UVKeep < 0)
         strcpy(uvk, "lru");
      else
         sprintf(uvk, "%ld", m_configuration.m_cs_UVKeep);
      float rg = (m_configuration.m_RamAbsAvailable) / float(1024*1024*1024);
      loff = snprintf(buff, sizeof(buff), "Config effective %s pfc configuration:\n"
                      "       pfc.cschk %s uvkeep %s\n"
                      "       pfc.blocksize %lld\n"
                      "       pfc.prefetch %d\n"
                      "       pfc.ram %.fg\n"
                      "       pfc.writequeue %d %d\n"
                      "       # Total available disk: %lld\n"
                      "       pfc.diskusage %lld %lld files %lld %lld %lld purgeinterval %d purgecoldfiles %d\n"
                      "       pfc.spaces %s %s\n"
                      "       pfc.trace %d\n"
                      "       pfc.flush %lld\n"
                      "       pfc.acchistorysize %d\n",
                      config_filename,
                      csc[int(m_configuration.m_cs_Chk)], uvk,
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
                      m_configuration.m_flushCnt,
                      m_configuration.m_accHistorySize);

      if (m_configuration.is_dir_stat_reporting_on())
      {
         loff += snprintf(buff + loff, sizeof(buff) - loff,
                          "       pfc.dirstats maxdepth %d ((internal: store_depth %d, size_of_dirlist %d, size_of_globlist %d))\n",
                          m_configuration.m_dirStatsMaxDepth, m_configuration.m_dirStatsStoreDepth,
                          (int) m_configuration.m_dirStatsDirs.size(), (int) m_configuration.m_dirStatsDirGlobs.size());
         loff += snprintf(buff + loff, sizeof(buff) - loff, "           dirlist:\n");
         for (std::set<std::string>::iterator i = m_configuration.m_dirStatsDirs.begin(); i != m_configuration.m_dirStatsDirs.end(); ++i)
            loff += snprintf(buff + loff, sizeof(buff) - loff, "               %s\n", i->c_str());
         loff += snprintf(buff + loff, sizeof(buff) - loff, "           globlist:\n");
         for (std::set<std::string>::iterator i = m_configuration.m_dirStatsDirGlobs.begin(); i != m_configuration.m_dirStatsDirGlobs.end(); ++i)
            loff += snprintf(buff + loff, sizeof(buff) - loff, "               %s/*\n", i->c_str());
      }

      if (m_configuration.m_hdfsmode)
      {
         loff += snprintf(buff + loff, sizeof(buff) - loff, "       pfc.hdfsmode hdfsbsize %lld\n", m_configuration.m_hdfsbsize);
      }

      if (m_configuration.m_username.empty())
      {
         char unameBuff[256];
         XrdOucUtils::UserName(getuid(), unameBuff, sizeof(unameBuff));
         m_configuration.m_username = unameBuff;
      }
      else
      {
         loff += snprintf(buff + loff, sizeof(buff) - loff, "       pfc.user %s\n", m_configuration.m_username.c_str());
      }

      m_log.Say(buff);
   }

   // Derived settings
   m_prefetch_enabled   = m_configuration.m_prefetch_max_blocks > 0;
   Info::s_maxNumAccess = m_configuration.m_accHistorySize;

   m_gstream = (XrdXrootdGStream*) m_env->GetPtr("pfc.gStream*");

   m_log.Say("Config Proxy File Cache g-stream has", m_gstream ? "" : " NOT", " been configured via xrootd.monitor directive");

   m_log.Say("------ Proxy File Cache configuration parsing ", aOK ? "completed" : "failed");

   if (ofsCfg) delete ofsCfg;

   // XXXX-CKSUM Testing. To be removed after OssPgi is also merged and valildated.
   // Building of xrdpfc_print fails when this is enabled.
#ifdef XRDPFC_CKSUM_TEST
   {
      int xxx = m_configuration.m_cs_Chk;

      for (m_configuration.m_cs_Chk = CSChk_None; m_configuration.m_cs_Chk <= CSChk_Both; ++m_configuration.m_cs_Chk)
      {
         Info::TestCksumStuff();
      }

      m_configuration.m_cs_Chk = xxx;
   }
#endif

   return aOK;
}

//------------------------------------------------------------------------------

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
            if (XrdOuca2x::a2tm(m_log, "Error getting purgecoldfiles age", cwg.GetWord(), &m_configuration.m_purgeColdFilesAge, 3600, 3600*24*360))
            {
               return false;
            }
            if (XrdOuca2x::a2i(m_log, "Error getting purgecoldfiles period", cwg.GetWord(), &m_configuration.m_purgeAgeBasedPeriod, 1, 1000))
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
   else if ( part == "acchistorysize" )
   {
      if ( XrdOuca2x::a2i(m_log, "Error getting access-history-size", cwg.GetWord(), &m_configuration.m_accHistorySize, 20, 200))
      {
         return false;
      }
   }
   else if ( part == "dirstats" )
   {
      const char *p = 0;
      while ((p = cwg.GetWord()) && cwg.HasLast())
      {
         if (strcmp(p, "maxdepth") == 0)
         {
            if (XrdOuca2x::a2i(m_log, "Error getting maxdepth value", cwg.GetWord(), &m_configuration.m_dirStatsMaxDepth, 0, 16))
            {
               return false;
            }
            m_configuration.m_dirStatsStoreDepth = std::max(m_configuration.m_dirStatsStoreDepth, m_configuration.m_dirStatsMaxDepth);
         }
         else if (strcmp(p, "dir") == 0)
         {
            p = cwg.GetWord();
            if (p && p[0] == '/')
            {
               // XXX -- should we just store them as sets of PathTokenizer objects, not strings?

               char d[1024]; d[0] = 0;
               int  depth = 0;
               {  // Compress multiple slashes and "measure" depth
                  const char *pp = p;
                  char *pd = d;
                  *(pd++) = *(pp++);
                  while (*pp != 0)
                  {
                     if (*(pd - 1) == '/')
                     {
                        if (*pp == '/')
                        {
                           ++pp; continue;
                        }
                        ++depth;
                     }
                     *(pd++) = *(pp++);
                  }
                  *(pd--) = 0;
                  // remove trailing but but not leading /
                  if (*pd == '/' && pd != d) *pd = 0;
               }
               int ld = strlen(d);
               if (ld >= 2 && d[ld-1] == '*' && d[ld-2] == '/')
               {
                  d[ld-2] = 0;
                  ld     -= 2;
                  m_configuration.m_dirStatsDirGlobs.insert(d);
                  printf("Glob %s -> %s -- depth = %d\n", p, d, depth);
               }
               else
               {
                  m_configuration.m_dirStatsDirs.insert(d);
                  printf("Dir  %s -> %s -- depth = %d\n", p, d, depth);
               }

               m_configuration.m_dirStatsStoreDepth = std::max(m_configuration.m_dirStatsStoreDepth, depth);
            }
            else
            {
               m_log.Emsg("Config", "Error: dirstats dir parameter requires a directory argument starting with a '/'.");
               return false;
            }
         }
         else
         {
            m_log.Emsg("Config", "Error: dirstats stanza contains unknown directive '", p, "'");
            return false;
         }
      }
   }
   else if ( part == "blocksize" )
   {
      long long minBSize =   4 * 1024;
      long long maxBSize = 512 * 1024 * 1024;
      if (XrdOuca2x::a2sz(m_log, "Error reading block-size", cwg.GetWord(), &m_configuration.m_bufferSize, minBSize, maxBSize))
      {
         return false;
      }
      if (m_configuration.m_bufferSize & 0xFFF)
      {
         m_configuration.m_bufferSize &= ~0x0FFF;
         m_configuration.m_bufferSize +=  0x1000;
         m_log.Emsg("Config", "pfc.blocksize must be a multiple of 4 kB. Rounded up.");
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
   else if ( part == "hdfsmode" )
   {
      m_log.Emsg("Config", "pfc.hdfsmode is currently unsupported.");
      return false;

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
      m_log.Emsg("ConfigParameters() unmatched pfc parameter", part.c_str());
      return false;
   }

   return true;
}
