#include "XrdFileCache.hh"
#include "XrdFileCacheTrace.hh"

using namespace XrdFileCache;

#include <fcntl.h>
#include <sys/time.h>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysTrace.hh"

namespace
{

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
   map_t  fmap; // map of files that are purge candidates

   typedef std::list<FS>    list_t;
   typedef list_t::iterator list_i;
   list_t flist; // list of files to be removed unconditionally

   FPurgeState(long long iNBytesReq) : nBytesReq(iNBytesReq), nBytesAccum(0), nBytesTotal(0), tMinTimeStamp(0) {}

   void      setMinTime(time_t min_time) { tMinTimeStamp = min_time; }
   time_t    getMinTime()          const { return tMinTimeStamp; }

   long long getNBytesTotal()      const { return nBytesTotal; }

   void checkFile(const std::string& iPath, long long iNBytes, time_t iTime)
   {
      nBytesTotal += iNBytes;

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
   char buff[256];
   XrdOucEnv env;
   int rdr;
   const size_t InfoExtLen = strlen(XrdFileCache::Info::m_infoExtension);  // cached var

   static const char* m_traceID = "Purge";
   Cache& factory = Cache::GetInstance();
   while ((rdr = iOssDF->Readdir(&buff[0], 256)) >= 0)
   {
      // printf("readdir [%s]\n", buff);
      std::string np = path + "/" + std::string(buff);
      size_t fname_len = strlen(&buff[0]);
      if (fname_len == 0)
      {
         // std::cout << "Finish read dir.[" << np <<"] Break loop \n";
         break;
      }

      if (strncmp("..", &buff[0], 2) && strncmp(".", &buff[0], 1))
      {
         XrdOssDF* dh = factory.GetOss()->newDir(factory.RefConfiguration().m_username.c_str());
         XrdOssDF* fh = factory.GetOss()->newFile(factory.RefConfiguration().m_username.c_str());

         if (fname_len > InfoExtLen && strncmp(&buff[fname_len - InfoExtLen], XrdFileCache::Info::m_infoExtension, InfoExtLen) == 0)
         {
            // We could also check if it is currently opened with Cache::HaveActiveFileWihtLocalPath()
            // This is not really necessary because we do that check before unlinking the file
            Info cinfo(Cache::GetInstance().GetTrace());
            int open_rs;
            if ((open_rs = fh->Open(np.c_str(), O_RDONLY, 0600, env)) == XrdOssOK && cinfo.Read(fh, np))
            {
               time_t accessTime;
               if (cinfo.GetLatestDetachTime(accessTime))
               {
                  // TRACE(Dump, "FillFileMapRecurse() checking " << buff << " accessTime  " << accessTime);
                  purgeState.checkFile(np, cinfo.GetNDownloadedBytes(), accessTime);
               }
               else
               {
                  // cinfo file does not contain any known accesses, use stat.mtime instead.

                  TRACE(Debug, "FillFileMapRecurse() could not get access time for " << np << ", trying stat");

                  XrdOss* oss = Cache::GetInstance().GetOss();
                  struct stat fstat;

                  if (oss->Stat(np.c_str(), &fstat) == XrdOssOK)
                  {
                     accessTime = fstat.st_mtime;
                     TRACE(Dump, "FillFileMapRecurse() have access time for " << np << " via stat: " << accessTime);
                     purgeState.checkFile(np, cinfo.GetNDownloadedBytes(), accessTime);
                  }
                  else
                  {
                     // This really shouldn't happen ... but if it does remove cinfo and the data file right away.

                     TRACE(Warning, "FillFileMapRecurse() could not get access time for " << np
                                                                                          << "; purging.");
                     oss->Unlink(np.c_str());
                     np = np.substr(0, np.size() - strlen(XrdFileCache::Info::m_infoExtension));
                     oss->Unlink(np.c_str());
                  }
               }
            }
            else
            {
               TRACE(Warning, "FillFileMapRecurse() can't open or read " << np << ", open exit status " << strerror(-open_rs)
                                                                         << "; purging.");
               XrdOss* oss = Cache::GetInstance().GetOss();
               oss->Unlink(np.c_str());
               np = np.substr(0, np.size() - strlen(XrdFileCache::Info::m_infoExtension));
               oss->Unlink(np.c_str());
            }
         }
         else if (dh->Opendir(np.c_str(), env) == XrdOssOK)
         {
            FillFileMapRecurse(dh, np, purgeState);
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
         TRACE(Error, trc_pfx << "can't get statvs for oss space " << m_configuration.m_data_space);
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
            std::string dataPath = infoPath.substr(0, infoPath.size() - strlen(XrdFileCache::Info::m_infoExtension));

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
