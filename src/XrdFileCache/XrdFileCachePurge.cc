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
      else if (nBytesAccum < nBytesReq || iTime < fmap.rbegin()->first)
      {
         fmap.insert(std::pair<const time_t, FS> (iTime, FS(iPath, iNBytes, iTime)));
         nBytesAccum += iNBytes;

         // remove newest files from map if necessary
         while (nBytesAccum > nBytesReq)
         {
            time_t nt = fmap.begin()->first;
            std::pair<map_i, map_i> ret = fmap.equal_range(nt);
            for (map_i it2 = ret.first; it2 != ret.second; ++it2)
               nBytesAccum -= it2->second.nBytes;
            fmap.erase(ret.first, ret.second);
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
            if (fh->Open(np.c_str(), O_RDONLY, 0600, env) == XrdOssOK && cinfo.Read(fh, np))
            {
               time_t accessTime;
               if (cinfo.GetLatestDetachTime(accessTime))
               {
                  TRACE(Dump, "FillFileMapRecurse() checking " << buff << " accessTime  " << accessTime);
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
               TRACE(Warning, "FillFileMapRecurse() can't open or read " << np << ", err " << strerror(errno)
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

void Cache::CacheDirCleanup()
{
   XrdOucEnv    env;
   XrdOss*      oss = Cache::GetInstance().GetOss();
   XrdOssVSInfo sP;
   long long    estimated_file_usage = m_configuration.m_fileUsageMax;

   int age_based_purge_countdown = 0; // enforce on first purge loop entry.

   while (true)
   {
      {
         XrdSysCondVarHelper lock(&m_active_cond);

         m_in_purge = true;
      }

      long long bytesToRemove_d = 0, bytesToRemove_f = 0;

      // get amount of space to potentially erase based on total disk usage
      if (oss->StatVS(&sP, m_configuration.m_data_space.c_str(), 1) < 0)
      {
         TRACE(Error, "Cache::CacheDirCleanup() can't get statvs for oss space " << m_configuration.m_data_space);
         exit(1);
      }
      else
      {
         long long ausage = sP.Total - sP.Free;
         TRACE(Info, "Cache::CacheDirCleanup() used disk space " << ausage << " bytes.");

         if (ausage > m_configuration.m_diskUsageHWM)
         {
            bytesToRemove_d = ausage - m_configuration.m_diskUsageLWM;
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

         bytesToRemove_f = std::max(estimated_file_usage - m_configuration.m_fileUsageNominal, 0ll);

         // Here we estimate fractional usages -- to decide if full scan is necessary before actual purge.
         double frac_du = 0, frac_fu = 0;
         m_configuration.calculate_fractional_usages(sP.Total - sP.Free, estimated_file_usage, frac_du, frac_fu);

         if (frac_fu > 1.0 - frac_du)
         {
            bytesToRemove_f = std::max(bytesToRemove_f, sP.Total - sP.Free - m_configuration.m_diskUsageLWM);
         }
      }

      TRACE(Debug, "Cache::CacheDirCleanup() bytes_to_remove_disk=" <<  bytesToRemove_d <<
                                           " bytes_to remove_files=" << bytesToRemove_f << ".");

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

      if (bytesToRemove > 0 || enforce_age_based_purge)
      {
         // Make a sorted map of file paths sorted by access time.
         FPurgeState purgeState(bytesToRemove * 5 / 4); // prepare 20% more volume than required

         if (m_configuration.is_age_based_purge_in_effect())
         {
            struct timeval t;
            gettimeofday(&t, 0);
            purgeState.setMinTime(t.tv_sec - m_configuration.m_purgeColdFilesAge);
         }

         XrdOssDF* dh = oss->newDir(m_configuration.m_username.c_str());
         if (dh->Opendir("", env) == XrdOssOK)
         {
            FillFileMapRecurse(dh, "", purgeState);
            dh->Close();
         }
         delete dh; dh = 0;

         estimated_file_usage = purgeState.getNBytesTotal();

         // Adjust bytesToRemove_f and then bytesToRemove based on actual file usage.
         if (m_configuration.are_file_usage_limits_set())
         {
            bytesToRemove_f = std::max(estimated_file_usage - m_configuration.m_fileUsageNominal, 0ll);

            double frac_du = 0, frac_fu = 0;
            m_configuration.calculate_fractional_usages(sP.Total - sP.Free, estimated_file_usage, frac_du, frac_fu);

            if (frac_fu > 1.0 - frac_du)
            {
               bytesToRemove_f = std::max(bytesToRemove_f, sP.Total - sP.Free - m_configuration.m_diskUsageLWM);
            }
         }
         bytesToRemove = std::max(bytesToRemove_d, bytesToRemove_f);

         purgeState.MoveListEntriesToMap();

         // LOOP over map and remove files with highest value of access time.
         struct stat fstat;
         for (FPurgeState::map_i it = purgeState.fmap.begin(); it != purgeState.fmap.end(); ++it)
         {
            std::string infoPath = it->second.path;
            std::string dataPath = infoPath.substr(0, infoPath.size() - strlen(XrdFileCache::Info::m_infoExtension));

            if (IsFileActiveOrPurgeProtected(dataPath))
               continue;

            // remove info file
            if (oss->Stat(infoPath.c_str(), &fstat) == XrdOssOK)
            {
               // cinfo file can be on another oss.space, do not subtract for now.
               // Could be relevant for very small block sizes.
               // bytesToRemove        -= fstat.st_size;
               // estimated_file_usage -= fstat.st_size;

               oss->Unlink(infoPath.c_str());
               TRACE(Info, "Cache::CacheDirCleanup() removed file:" <<  infoPath <<  " size: " << fstat.st_size);
            }

            // remove data file
            if (oss->Stat(dataPath.c_str(), &fstat) == XrdOssOK)
            {
               bytesToRemove        -= it->second.nBytes;
               estimated_file_usage -= it->second.nBytes;

               oss->Unlink(dataPath.c_str());
               TRACE(Info, "Cache::CacheDirCleanup() removed file: " << dataPath << " size " << it->second.nBytes);
            }

            // Continue purging cold files i needed.
            if (m_configuration.is_age_based_purge_in_effect() && it->second.time < purgeState.getMinTime())
               continue;

            // Finish when enough space has been freed.
            if (bytesToRemove <= 0)
               break;
         }
      }

      {
         XrdSysCondVarHelper lock(&m_active_cond);

         m_purge_delay_set.clear();
         m_in_purge = false;
      }

      sleep(m_configuration.m_purgeInterval);
   }
}
