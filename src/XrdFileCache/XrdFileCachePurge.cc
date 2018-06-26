#include "XrdFileCache.hh"
#include "XrdFileCacheTrace.hh"

using namespace XrdFileCache;

#include <fcntl.h>
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
      long long nByte;
      FS(const char* p, long long n) : path(p), nByte(n) {}
   };

   typedef std::multimap<time_t, FS> map_t;
   typedef map_t::iterator map_i;
   map_t fmap;
   
   FPurgeState(long long iNByteReq) : nByteReq(iNByteReq), nByteAccum(0) {}

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

XrdSysTrace* GetTrace()
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
            // We could also check if it is currently opened with Cache::HaveActiveFileWihtLocalPath()
            // This is not relay necessary because we do that check before unlinking the file
            Info cinfo(Cache::GetInstance().GetTrace());
            if (fh->Open(np.c_str(), O_RDONLY, 0600, env) == XrdOssOK && cinfo.Read(fh, np))
            {
               time_t accessTime;
               if (cinfo.GetLatestDetachTime(accessTime))
               {
                  TRACE(Dump, "FillFileMapRecurse() checking " << buff << " accessTime  " << accessTime);
                  purgeState.checkFile(accessTime, np.c_str(), cinfo.GetNDownloadedBytes());
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
                     purgeState.checkFile(accessTime, np.c_str(), cinfo.GetNDownloadedBytes());
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
}
void Cache::CacheDirCleanup()
{
   XrdOucEnv env;
   XrdOss*      oss = Cache::GetInstance().GetOss();
   XrdOssVSInfo sP;

   while (1)
   {

      {
         XrdSysCondVarHelper lock(&m_active_cond);

         m_in_purge = true;
      }

      // get amount of space to erase
      long long bytesToRemove = 0;
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
            bytesToRemove = ausage - m_configuration.m_diskUsageLWM;
            TRACE(Info, "Cache::CacheDirCleanup() need to remove " <<  bytesToRemove << " bytes.");
         }
      }

      if (bytesToRemove > 0)
      {
         // make a sorted map of file patch by access time
         XrdOssDF* dh = oss->newDir(m_configuration.m_username.c_str());
         if (dh->Opendir("", env) == XrdOssOK)
         {
            FPurgeState purgeState(bytesToRemove * 5 / 4); // prepare 20% more volume than required

            FillFileMapRecurse(dh, "", purgeState);

            // loop over map and remove files with highest value of access time
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
                  // bytesToRemove -= fstat.st_size;
                  oss->Unlink(infoPath.c_str());
                  TRACE(Info, "Cache::CacheDirCleanup() removed file:" <<  infoPath <<  " size: " << fstat.st_size);
               }

               // remove data file
               if (oss->Stat(dataPath.c_str(), &fstat) == XrdOssOK)
               {
                  bytesToRemove -= it->second.nByte;

                  oss->Unlink(dataPath.c_str());
                  TRACE(Info, "Cache::CacheDirCleanup() removed file: %s " << dataPath << " size " << it->second.nByte);
               }

               if (bytesToRemove <= 0)
                  break;
            }
         }
         dh->Close();
         delete dh; dh = 0;
      }

      {
         XrdSysCondVarHelper lock(&m_active_cond);

         m_purge_delay_set.clear();
         m_in_purge = false;
      }

      sleep(m_configuration.m_purgeInterval);
   }
}
