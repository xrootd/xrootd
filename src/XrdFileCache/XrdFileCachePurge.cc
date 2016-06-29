#include "XrdFileCache.hh"
#include "XrdFileCacheTrace.hh"

using namespace XrdFileCache;

#include <fcntl.h>
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucTrace.hh"

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

XrdOucTrace* GetTrace()
{
   // needed for logging macros
   return Cache::GetInstance().GetTrace();
}

void FillFileMapRecurse( XrdOssDF* iOssDF, const std::string& path, FPurgeState& purgeState)
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
            // XXXX MT - shouldn't we also check if it is currently opened?

            Info cinfo(Cache::GetInstance().GetTrace());
            if (fh->Open(np.c_str(), O_RDONLY, 0600, env) == XrdOssOK && cinfo.Read(fh, np))
            {
               time_t accessTime;
               if (cinfo.GetLatestDetachTime(accessTime, fh))
               {
                  TRACE(Dump, "FillFileMapRecurse() checking " << buff << " accessTime  " << accessTime);
                  purgeState.checkFile(accessTime, np.c_str(), cinfo.GetNDownloadedBytes());
               }
               else
               {
                  // cinfo file does not contain any known accesses, use stat.mtime instead.

                  TRACE(Warning, "FillFileMapRecurse() could not get access time for " << np << ", trying stat");

                  XrdOss* oss = Cache::GetInstance().GetOss();
                  struct stat fstat;

                  if (oss->Stat(np.c_str(), &fstat) == XrdOssOK)
                  {
                     accessTime = fstat.st_mtime;
                     TRACE(Dump, "FillFileMapRecurse() have access time for " << np << " via stat: " << accessTime);
                     purgeState.checkFile(accessTime, np.c_str(), cinfo.GetNDownloadedBytes());
                  }
                  else
                  {
                     // This really shouldn't happen ... but if it does remove cinfo and the data file right away.

                     TRACE(Warning, "FillFileMapRecurse() could not get access time for " << np);
                     oss->Unlink(np.c_str());
                     np = np.substr(0, np.size() - strlen(XrdFileCache::Info::m_infoExtension));
                     oss->Unlink(np.c_str());
                  }
               }
            }
            else
            {
               TRACE(Warning, "FillFileMapRecurse() can't open or read " << np << " " << strerror(errno));
               // XXXX Purge it!
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
         TRACE(Error, "Cache::CacheDirCleanup() can't get statvs for dir " <<  m_configuration.m_cache_dir.c_str());
         exit(1);
      }
      else
      {
         long long ausage = sP.Total - sP.Free;
         TRACE(Debug, "Cache::CacheDirCleanup() occupates disk space == " <<  ausage);
         if (ausage > m_configuration.m_diskUsageHWM)
         {
            bytesToRemove = ausage - m_configuration.m_diskUsageLWM;
            TRACE(Debug, "Cache::CacheDirCleanup() need space for " <<  bytesToRemove << " bytes");
         }
      }

      if (bytesToRemove > 0)
      {
         // make a sorted map of file patch by access time
         XrdOssDF* dh = oss->newDir(m_configuration.m_username.c_str());
         if (dh->Opendir(m_configuration.m_cache_dir.c_str(), env) == XrdOssOK)
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
                  TRACE(Info, "Cache::CacheDirCleanup()  removed  file:" <<  path <<  " size: " << fstat.st_size);
               }

               // remove data file
               path = path.substr(0, path.size() - strlen(XrdFileCache::Info::m_infoExtension));
               if (oss->Stat(path.c_str(), &fstat) == XrdOssOK)
               {
                  bytesToRemove -= it->second.nByte;
                  oss->Unlink(path.c_str());
                  TRACE(Info, "Cache::CacheDirCleanup() removed file: %s " << path << " size " << it->second.nByte);
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
