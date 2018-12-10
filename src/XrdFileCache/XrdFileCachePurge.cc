#include "XrdFileCache.hh"
#include "XrdFileCacheTrace.hh"

#include <fcntl.h>
#include <sys/time.h>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysTrace.hh"

using namespace XrdFileCache;

namespace
{

class DStatsState
{
   DStatsState *m_parent;
   Stats        m_stats;
   long long    m_usage; // will be in stats???
   long long    m_sum_usage; // collected upwards and reset after push
   bool         m_rrd_dump;  // storing of stats required

   typedef std::map<std::string, DStatsState> DssMap_t;
   typedef DssMap_t::iterator                 DssMap_i;

   DssMap_t m_subdirs;

   DStatsState& find_path_tok(PathTokenizer &pt, int pos)
   {
      // is this ok??
      if (m_subdirs.empty()) return *this;

      // ??? check if at the end of pt tokens !!!

      DssMap_i i = m_subdirs.find(pt.m_dirs[pos]);

      if (i != m_subdirs.end())
      {
      }
      else
      {
         // ??? create if not found ??? to what level, etc ...
      }
   }

public:
   DStatsState() : m_parent(0) {}

   DStatsState(DStatsState *parent) : m_parent(parent) {}

   DStatsState& find_path(const std::string &path, int max_depth=-1)
   {
      if (max_depth < 0)
      {
         max_depth = Cache::GetInstance().RefConfiguration().m_dirStatsStoreDepth;
      }

      PathTokenizer pt(path, max_depth);

      return find_path_tok(pt, 0);
   }
};

class FPurgeState
{
public:
   struct FS
   {
      std::string path;
      long long   nBytes;
      time_t      time;

      FS(const std::string& p, long long n, time_t t) : path(p), nBytes(n), time(t) {}
   };

   typedef std::multimap<time_t, FS> map_t;
   typedef map_t::iterator           map_i;

   map_t   fmap; // map of files that are purge candidates

   typedef std::list<FS>    list_t;
   typedef list_t::iterator list_i;

   list_t  flist; // list of files to be removed unconditionally


   // ------------------------------------
   // Directory handling & stat collection
   // ------------------------------------

   int          m_dir_level;
   int          m_max_dir_level_for_stat_collection; // until I honor globs from pfc.dirstats
   std::string  m_current_dir;
   std::string  m_current_path; // Note: without leading '/'!

   std::vector<std::string> m_dir_names_stack;
   std::vector<long long>   m_dir_usage_stack;

   void cd_down(const std::string& dir_name, const std::string& full_path)
   {
      ++m_dir_level;
      if (m_dir_level <= m_max_dir_level_for_stat_collection)
      {
         m_dir_usage_stack.push_back(0);
      }
      m_dir_names_stack.push_back(dir_name);
      m_current_dir  = dir_name;
      m_current_path = full_path;
   }

   void cd_up(const std::string& full_path)
   {
      m_current_path = full_path;
      m_current_dir  = m_dir_names_stack.back();
      m_dir_names_stack.pop_back();
      if (m_dir_level <= m_max_dir_level_for_stat_collection)
      {
         // what else to do?

         long long tail = m_dir_usage_stack.back();
         m_dir_usage_stack.pop_back();

         // or here

         m_dir_usage_stack.back() += tail;

         // or here
      }
      --m_dir_level;
   }

   // --------------------------------


   // ------------------------------------------------------------------------

   FPurgeState(long long iNBytesReq) :
      m_dir_level(0), m_max_dir_level_for_stat_collection(2),
      nBytesReq(iNBytesReq), nBytesAccum(0), nBytesTotal(0), tMinTimeStamp(0)
   {
      m_dir_names_stack.reserve(32);
      m_dir_usage_stack.reserve(m_max_dir_level_for_stat_collection + 1);
      m_dir_usage_stack.push_back(0);
   }

   // ------------------------------------------------------------------------

   void      setMinTime(time_t min_time) { tMinTimeStamp = min_time; }
   time_t    getMinTime()          const { return tMinTimeStamp; }

   long long getNBytesTotal()      const { return nBytesTotal; }

   void checkFile(const std::string& iPath, long long iNBytes, time_t iTime)
   {
      nBytesTotal += iNBytes;

      m_dir_usage_stack.back() += iNBytes;

      if (tMinTimeStamp > 0 && iTime < tMinTimeStamp)
      {
         flist.push_back(FS(iPath, iNBytes, iTime));
         nBytesAccum += iNBytes;
      }
      else if (nBytesAccum < nBytesReq || ( ! fmap.empty() && iTime < fmap.rbegin()->first))
      {
         fmap.insert(std::make_pair(iTime, FS(iPath, iNBytes, iTime)));
         nBytesAccum += iNBytes;

         // remove newest files from map if necessary
         while ( ! fmap.empty() && nBytesAccum - fmap.rbegin()->second.nBytes >= nBytesReq)
         {
            nBytesAccum -= fmap.rbegin()->second.nBytes;
            fmap.erase(--(fmap.rbegin().base()));
         }
      }
   }

   void MoveListEntriesToMap()
   {
      for (list_i i = flist.begin(); i != flist.end(); ++i)
      {
         fmap.insert(std::make_pair(i->time, *i));
      }
      flist.clear();
   }

private:
   long long nBytesReq;
   long long nBytesAccum;
   long long nBytesTotal;
   time_t    tMinTimeStamp;
};

XrdSysTrace* GetTrace()
{
   // needed for logging macros
   return Cache::GetInstance().GetTrace();
}

void FillFileMapRecurse(XrdOssDF* iOssDF, const std::string& path, FPurgeState& purgeState)
{
   static const char* m_traceID = "Purge";

   const char   *InfoExt    = XrdFileCache::Info::s_infoExtension;
   const size_t  InfoExtLen = strlen(InfoExt);

   Cache         &cache = Cache::GetInstance();
   XrdOss        *oss   = cache.GetOss();
   const Configuration &conf = cache.RefConfiguration();

   char      fname[256];
   XrdOucEnv env;

   while (iOssDF->Readdir(&fname[0], 256) >= 0)
   {
      // printf("readdir [%s]\n", fname);

      std::string new_path  = path + "/"; new_path += fname;
      size_t      fname_len = strlen(&fname[0]);

      if (fname_len == 0)
      {
         // std::cout << "Finish read dir.[" << new_path << "] Break loop.\n";
         break;
      }

      if (strncmp("..", &fname[0], 2) && strncmp(".", &fname[0], 1))
      {
         XrdOssDF* dh = oss->newDir (conf.m_username.c_str());
         XrdOssDF* fh = oss->newFile(conf.m_username.c_str());

         if (fname_len > InfoExtLen && strncmp(&fname[fname_len - InfoExtLen], InfoExt, InfoExtLen) == 0)
         {
            // Check if the file is currently opened / purge-protected is done before unlinking of the file.

            Info cinfo(Cache::GetInstance().GetTrace());

            if (fh->Open(new_path.c_str(), O_RDONLY, 0600, env) == XrdOssOK && cinfo.Read(fh, new_path))
            {
               bool   all_gauda = true;
               time_t accessTime;
               if ( ! cinfo.GetLatestDetachTime(accessTime))
               {
                  // cinfo file does not contain any known accesses, use stat.mtime instead.
                  TRACE(Debug, "FillFileMapRecurse() could not get access time for " << new_path << ", trying stat");

                  struct stat fstat;
                  if (oss->Stat(new_path.c_str(), &fstat) == XrdOssOK)
                  {
                     accessTime = fstat.st_mtime;
                     TRACE(Dump, "FillFileMapRecurse() have access time for " << new_path << " via stat: " << accessTime);
                  }
                  else
                  {
                     // This really shouldn't happen ... but if it does remove cinfo and the data file right away.
                     TRACE(Warning, "FillFileMapRecurse() could not get access time for " << new_path << "; purging.");
                     oss->Unlink(new_path.c_str());
                     new_path = new_path.substr(0, new_path.size() - strlen(InfoExt));
                     oss->Unlink(new_path.c_str());
                     all_gauda = false;
                  }
               }

               if (all_gauda)
               {
                  // TRACE(Dump, "FillFileMapRecurse() checking " << fname << " accessTime  " << accessTime);
                  purgeState.checkFile(new_path, cinfo.GetNDownloadedBytes(), accessTime);
               }
            }
            else
            {
               TRACE(Warning, "FillFileMapRecurse() can't open or read " << new_path << ", err " << strerror(errno)
                                                                         << "; purging.");
               XrdOss* oss = Cache::GetInstance().GetOss();
               oss->Unlink(new_path.c_str());
               new_path = new_path.substr(0, new_path.size() - InfoExtLen);
               oss->Unlink(new_path.c_str());
            }
         }
         else if (dh->Opendir(new_path.c_str(), env) == XrdOssOK)
         {
            purgeState.cd_down(fname, new_path);

            FillFileMapRecurse(dh, new_path, purgeState);

            purgeState.cd_up(path);

            // here dump stats, if required, into '' new_path + ".crrd" ''
            // nope, also need access_stats, collected elsewhere ... before or after.
            // in cdUp, stats are added to the relevant directory.
            // max-depth controls how far down these will be kept.
            // Alternatively, I have to continuously compare to the glob list.
            // Or not, if those are tokenized and I compare them level by level.
         }

         delete dh; dh = 0;
         delete fh; fh = 0;
      }
   }
}

} // end anon namespace

//------------------------------------------------------------------------------

void Cache::Purge()
{
   static const char *trc_pfx = "Cache::Purge() ";

   XrdOucEnv    env;
   XrdOss*      oss = Cache::GetInstance().GetOss();
   XrdOssVSInfo sP;
   long long    disk_usage;
   long long    estimated_file_usage = m_configuration.m_diskUsageHWM;

   // Pause before initial run
   sleep(1);

   int  age_based_purge_countdown = 0; // enforce on first purge loop entry.
   bool is_first = true;

   while (true)
   {
      {
         XrdSysCondVarHelper lock(&m_active_cond);

         m_in_purge = true;
      }

      TRACE(Info, trc_pfx << "Started.");

      long long bytesToRemove_d = 0, bytesToRemove_f = 0;

      // get amount of space to potentially erase based on total disk usage
      if (oss->StatVS(&sP, m_configuration.m_data_space.c_str(), 1) < 0)
      {
         TRACE(Error, trc_pfx << "can't get StatVS for oss space " << m_configuration.m_data_space);
         continue;
      }
      else
      {
         disk_usage = sP.Total - sP.Free;
         TRACE(Debug, trc_pfx << "used disk space " << disk_usage << " bytes.");

         if (disk_usage > m_configuration.m_diskUsageHWM)
         {
            bytesToRemove_d = disk_usage - m_configuration.m_diskUsageLWM;
         }
      }

      // estimate amount of space to erase based on file usage
      if (m_configuration.are_file_usage_limits_set())
      {
         long long estimated_writes_since_last_purge;
         {
            XrdSysCondVarHelper lock(&m_writeQ.condVar);

            estimated_writes_since_last_purge = m_writeQ.writes_between_purges;
            m_writeQ.writes_between_purges = 0;
         }
         estimated_file_usage += estimated_writes_since_last_purge;

         TRACE(Debug, trc_pfx << "estimated usage by files " << estimated_file_usage << " bytes.");

         bytesToRemove_f = std::max(estimated_file_usage - m_configuration.m_fileUsageNominal, 0ll);

         // Here we estimate fractional usages -- to decide if full scan is necessary before actual purge.
         double frac_du = 0, frac_fu = 0;
         m_configuration.calculate_fractional_usages(disk_usage, estimated_file_usage, frac_du, frac_fu);

         if (frac_fu > 1.0 - frac_du)
         {
            bytesToRemove_f = std::max(bytesToRemove_f, disk_usage - m_configuration.m_diskUsageLWM);
         }
      }

      long long bytesToRemove = std::max(bytesToRemove_d, bytesToRemove_f);

      bool enforce_age_based_purge = false;
      if (m_configuration.is_age_based_purge_in_effect())
      {
         if (--age_based_purge_countdown <= 0)
         {
            enforce_age_based_purge   = true;
            age_based_purge_countdown = m_configuration.m_purgeColdFilesPeriod;
         }
      }

      // XXXX When to do all the access stat magicck, getting, consolidation, etc.
      // bool enforce_;


      TRACE(Debug, trc_pfx << "Precheck:");
      TRACE(Debug, "\tbytes_to_remove_disk    = " << bytesToRemove_d << " B");
      TRACE(Debug, "\tbytes_to remove_files   = " << bytesToRemove_f << " B (" << (is_first ? "max possible for initial run" : "estimated") << ")");
      TRACE(Debug, "\tbytes_to_remove         = " << bytesToRemove   << " B");
      TRACE(Debug, "\tenforce_age_based_purge = " << enforce_age_based_purge);
      is_first = false;

      long long bytesToRemove_at_start = 0; // set after file scan
      int       deleted_file_count     = 0;

      if (bytesToRemove > 0 || enforce_age_based_purge)
      {
         // Make a sorted map of file paths sorted by access time.
         FPurgeState purgeState(2 * bytesToRemove); // prepare twice more volume than required

         if (m_configuration.is_age_based_purge_in_effect())
         {
            purgeState.setMinTime(time(0) - m_configuration.m_purgeColdFilesAge);
         }

         XrdOssDF* dh = oss->newDir(m_configuration.m_username.c_str());
         if (dh->Opendir("", env) == XrdOssOK)
         {
            FillFileMapRecurse(dh, "", purgeState);
            dh->Close();
         }
         delete dh; dh = 0;

         estimated_file_usage = purgeState.getNBytesTotal();

         TRACE(Debug, trc_pfx << "actual usage by files " << estimated_file_usage << " bytes.");

         // Adjust bytesToRemove_f and then bytesToRemove based on actual file usage,
         // possibly retreating below nominal file usage (but not below baseline file usage).
         if (m_configuration.are_file_usage_limits_set())
         {
            bytesToRemove_f = std::max(estimated_file_usage - m_configuration.m_fileUsageNominal, 0ll);

            double frac_du = 0, frac_fu = 0;
            m_configuration.calculate_fractional_usages(disk_usage, estimated_file_usage, frac_du, frac_fu);

            if (frac_fu > 1.0 - frac_du)
            {
               bytesToRemove = std::max(bytesToRemove_f, disk_usage - m_configuration.m_diskUsageLWM);
               bytesToRemove = std::min(bytesToRemove,   estimated_file_usage - m_configuration.m_fileUsageBaseline);
            }
            else
            {
               bytesToRemove = std::max(bytesToRemove_d, bytesToRemove_f);
            }
         }
         else
         {
            bytesToRemove = std::max(bytesToRemove_d, bytesToRemove_f);
         }
         bytesToRemove_at_start = bytesToRemove;

         TRACE(Debug, trc_pfx << "After scan:");
         TRACE(Debug, "\tbytes_to_remove_disk    = " << bytesToRemove_d << " B");
         TRACE(Debug, "\tbytes_to remove_files   = " << bytesToRemove_f << " B (measured)");
         TRACE(Debug, "\tbytes_to_remove         = " << bytesToRemove   << " B");
         TRACE(Debug, "\tenforce_age_based_purge = " << enforce_age_based_purge);
         TRACE(Debug, "\tmin_time                = " << purgeState.getMinTime());

         if (enforce_age_based_purge)
         {
            purgeState.MoveListEntriesToMap();
         }

         // Loop over map and remove files with oldest values of access time.
         struct stat fstat;
         int         protected_cnt = 0;
         long long   protected_sum = 0;
         for (FPurgeState::map_i it = purgeState.fmap.begin(); it != purgeState.fmap.end(); ++it)
         {
            // Finish when enough space has been freed but not while purging of cold files is in progress.
            if (bytesToRemove <= 0 && ! (m_configuration.is_age_based_purge_in_effect() && it->first < purgeState.getMinTime()))
            {
               break;
            }

            std::string infoPath = it->second.path;
            std::string dataPath = infoPath.substr(0, infoPath.size() - strlen(XrdFileCache::Info::s_infoExtension));

            if (IsFileActiveOrPurgeProtected(dataPath))
            {
               ++protected_cnt;
               protected_sum += it->second.nBytes;
               TRACE(Debug, trc_pfx << "File is active or purge-protected: " << dataPath << " size: " << it->second.nBytes);
               continue;
            }

            // remove info file
            if (oss->Stat(infoPath.c_str(), &fstat) == XrdOssOK)
            {
               // cinfo file can be on another oss.space, do not subtract for now.
               // Could be relevant for very small block sizes.
               // bytesToRemove        -= fstat.st_size;
               // estimated_file_usage -= fstat.st_size;
               // ++deleted_file_count;

               oss->Unlink(infoPath.c_str());
               TRACE(Dump, trc_pfx << "Removed file: '" << infoPath << "' size: " << fstat.st_size);
            }

            // remove data file
            if (oss->Stat(dataPath.c_str(), &fstat) == XrdOssOK)
            {
               bytesToRemove        -= it->second.nBytes;
               estimated_file_usage -= it->second.nBytes;
               ++deleted_file_count;

               oss->Unlink(dataPath.c_str());
               TRACE(Dump, trc_pfx << "Removed file: '" << dataPath << "' size: " << it->second.nBytes << ", time: " << it->first);
            }
         }
         if (protected_cnt > 0)
         {
            TRACE(Info, trc_pfx << "Encountered " << protected_cnt << " protected files, sum of their size: " << protected_sum);
         }
      }

      {
         XrdSysCondVarHelper lock(&m_active_cond);

         m_purge_delay_set.clear();
         m_in_purge = false;
      }

      TRACE(Info, trc_pfx << "Finished, removed " << deleted_file_count << " data files, total size " <<
            bytesToRemove_at_start - bytesToRemove << ", bytes to remove at end: " << bytesToRemove);

      sleep(m_configuration.m_purgeInterval);
   }
}


//==============================================================================
// DirStats specific stuff
//==============================================================================

/*

  RRDtool DB sketch

  # --start is not needed, default is "now - 10s"

  rrdtool create <dirname>.crrd --step <purge_interval> \
     DS:open_events:ABSOLUTE:<2*purge_interval>:0:1000 \
     DS:access_duration:ABSOLUTE:<2*purge_interval>:0:1000000 \
     DS:bytes_disk:ABSOLUTE:<2*purge_interval>:0:100000000000 \
     DS:bytes_fetch:ABSOLUTE:<2*purge_interval>:0:100000000000 \
     DS:bytes_bypass:ABSOLUTE:<2*purge_interval>:0:100000000000 \
     DS:bytes_served:COMPUTE:bytes_disk,bytes_fetch,bytes_bypass,+,+ \
     RRA:AVERAGE:0.5:<purge_interval>s:1w \
     RRA:AVERAGE:0.5:1h:1M \
     RRA:AVERAGE:0.5:1d:1y \

  Questions / Issues:
  1. DS min / max -- are they for values after division or before?
     I'm assuming after in the above.
  2. What is xxf (argument to RRA)? The thing that is usually 0.5.
     Ah, xfiles factor, how much can be unknown for consolidated value to eb known.
  3. Use COMPUTE for bytes_served (sum of the others).
     ARGH, can not change heartbeat for COMPUTE, will pass it in manually.
  4. Use rddtool tune -h to change heartbeat on start (if different, maybe).

  5. ADD disk_usage !!!!
     DS:disk_usage:GAUGE:...
 */
