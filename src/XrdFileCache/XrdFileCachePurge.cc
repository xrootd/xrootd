#include "XrdFileCache.hh"

using namespace XrdFileCache;

#include <fcntl.h>
#include "XrdOuc/XrdOucEnv.hh"

namespace
{
   class FPurgeState
   {
   public:
      struct FS
      {
         std::string path;
         long long   nByte;

         FS(const char* p, long long n) : path(p), nByte(n) {}
      };

      typedef std::multimap<time_t, FS> map_t;
      typedef map_t::iterator map_i;

      FPurgeState(long long iNByteReq) : nByteReq(iNByteReq), nByteAccum(0) {}

      map_t fmap;

      void checkFile (time_t iTime, const char* iPath,  long long iNByte)
      {
         if (nByteAccum < nByteReq || iTime < fmap.rbegin()->first)
         {
            fmap.insert(std::pair<const time_t, FS> (iTime, FS(iPath, iNByte)));
            nByteAccum += iNByte;

            // remove newest files from map if necessary
            while (nByteAccum > nByteReq)
            {
               time_t nt = fmap.begin()->first;
               std::pair<map_i, map_i> ret = fmap.equal_range(nt); 
               for (map_i it2 = ret.first; it2 != ret.second; ++it2)
                  nByteAccum -= it2->second.nByte;
               fmap.erase(ret.first, ret.second);
            }
         }
      }

   private:
      long long nByteReq;
      long long nByteAccum;
   };
}

void FillFileMapRecurse( XrdOssDF* iOssDF, const std::string& path, FPurgeState& purgeState)
{
   char buff[256];
   XrdOucEnv env;
   int rdr;
   const size_t InfoExtLen = strlen(XrdFileCache::Info::m_infoExtension);  // cached var
   XrdCl::Log *log = XrdCl::DefaultEnv::GetLog();

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

         if (fname_len > InfoExtLen && strncmp(&buff[fname_len - InfoExtLen ], XrdFileCache::Info::m_infoExtension, InfoExtLen) == 0)
         {
            // XXXX MT - shouldn't we also check if it is currently opened?

            fh->Open(np.c_str(), O_RDONLY, 0600, env);
            Info cinfo(factory.RefConfiguration().m_bufferSize);
            time_t accessTime;
            cinfo.Read(fh);
            if (cinfo.GetLatestDetachTime(accessTime, fh))
            {
               log->Debug(XrdCl::AppMsg, "FillFileMapRecurse() checking %s accessTime %d ", buff, (int)accessTime);
               purgeState.checkFile(accessTime, np.c_str(), cinfo.GetNDownloadedBytes());
            }
            else
            {
               // cinfo file does not contain any known accesses, use stat.mtime instead.

               log->Info(XrdCl::AppMsg, "FillFileMapRecurse() could not get access time for %s, trying stat.\n", np.c_str());

               XrdOss* oss = Cache::GetInstance().GetOss();
               struct stat fstat;

               if (oss->Stat(np.c_str(), &fstat) == XrdOssOK)
               {
                  accessTime = fstat.st_mtime;
                  log->Info(XrdCl::AppMsg, "FillFileMapRecurse() determined access time for %s via stat: %lld\n",
                                                np.c_str(), accessTime);

                  purgeState.checkFile(accessTime, np.c_str(), cinfo.GetNDownloadedBytes());
               }
               else
               {
                  // This really shouldn't happen ... but if it does remove cinfo and the data file right away.

                  log->Warning(XrdCl::AppMsg, "FillFileMapRecurse() could not get access time for %s. Purging directly.\n",
                               np.c_str());

                  oss->Unlink(np.c_str());
                  np = np.substr(0, np.size() - strlen(XrdFileCache::Info::m_infoExtension));
                  oss->Unlink(np.c_str());
               }
            }
         }
         else if (dh->Opendir(np.c_str(), env) >= 0)
         {
            FillFileMapRecurse(dh, np, purgeState);
         }

         delete dh; dh = 0;
         delete fh; fh = 0;
      }
   }
}

void Cache::CacheDirCleanup()
{
   // check state every sleep seconds
   const static int sleept = 300;
   struct stat fstat;
   XrdOucEnv env;

   XrdOss* oss = Cache::GetInstance().GetOss();
   XrdOssVSInfo sP;

   while (1)
   {
      // get amount of space to erase
      long long bytesToRemove = 0;
      if (oss->StatVS(&sP, "public", 1) < 0)
      {
         clLog()->Error(XrdCl::AppMsg, "Cache::CacheDirCleanup() can't get statvs for dir [%s] \n", m_configuration.m_cache_dir.c_str());
         exit(1);
      }
      else
      {
         long long ausage = sP.Total - sP.Free;
         clLog()->Info(XrdCl::AppMsg, "Cache::CacheDirCleanup() occupates disk space == %lld", ausage);
         if (ausage > m_configuration.m_diskUsageHWM)
         {
            bytesToRemove = ausage - m_configuration.m_diskUsageLWM;
            clLog()->Info(XrdCl::AppMsg, "Cache::CacheDirCleanup() need space for  %lld bytes", bytesToRemove);
         }
      }

      if (bytesToRemove > 0)
      {
         // make a sorted map of file patch by access time
         XrdOssDF* dh = oss->newDir(m_configuration.m_username.c_str());
         if (dh->Opendir(m_configuration.m_cache_dir.c_str(), env) >= 0)
         {
            FPurgeState purgeState(bytesToRemove * 5 / 4); // prepare 20% more volume than required

            FillFileMapRecurse(dh, m_configuration.m_cache_dir, purgeState);

            // loop over map and remove files with highest value of access time
            for (FPurgeState::map_i it = purgeState.fmap.begin(); it != purgeState.fmap.end(); ++it)
            {
               // XXXX MT - shouldn't we re-check if the file is currently opened?

               std::string path = it->second.path;
               // remove info file
               if (oss->Stat(path.c_str(), &fstat) == XrdOssOK)
               {
                  bytesToRemove -= fstat.st_size;
                  oss->Unlink(path.c_str());
                  clLog()->Info(XrdCl::AppMsg, "Cache::CacheDirCleanup() removed %s size %lld",
                                                path.c_str(), fstat.st_size);
               }

               // remove data file
               path = path.substr(0, path.size() - strlen(XrdFileCache::Info::m_infoExtension));
               if (oss->Stat(path.c_str(), &fstat) == XrdOssOK)
               {
                  bytesToRemove -= it->second.nByte;
                  oss->Unlink(path.c_str());
                  clLog()->Info(XrdCl::AppMsg, "Cache::CacheDirCleanup() removed %s bytes %lld, stat_size %lld",
                                                path.c_str(), it->second.nByte, fstat.st_size);
               }

               if (bytesToRemove <= 0)
                  break;
            }
         }
	 dh->Close();
	 delete dh; dh =0;
      }

      sleep(sleept);
   }
}
