//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University  
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman           
//----------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------------------

#include <fcntl.h>
#include <sstream>
#include <tr1/memory>
#include <sys/statvfs.h> 
#include "XrdSys/XrdSysPthread.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucEnv.hh"

#include "XrdFileCache.hh"
#include "XrdFileCacheIOEntire.hh"
#include "XrdFileCacheIOBlock.hh"
#include "XrdFileCacheFactory.hh"
#include "XrdFileCachePrefetch.hh"
#include "XrdFileCacheLog.hh"


XrdFileCache::Cache *XrdFileCache::Cache::m_cache = NULL;
using namespace XrdFileCache;

void*
TempDirCleanupThread(void*)
{
    XrdFileCache::Cache::m_cache->TempDirCleanup();
    return NULL;
}

Cache::Cache(XrdOucCacheStats & stats)
    :m_attached(0),
      m_stats(stats),
      m_disablePrefetch(false)
{
    m_cache = this;

    pthread_t tid;
    XrdSysThread::Run(&tid, TempDirCleanupThread, NULL, 0, "XrdFileCache TempDirCleanup");
}

XrdOucCacheIO *
Cache::Attach(XrdOucCacheIO *io, int Options)
{
    if (!m_disablePrefetch)
    {
        XrdSysMutexHelper lock(&m_io_mutex);
        m_attached++;

        aMsgIO(kInfo, io, "Cache::Attach()");
        if (io)
        {
            if (Factory::GetInstance().RefConfiguration().m_prefetchFileBlocks)
                return new IOBlocks(*io, m_stats, *this);
            else 
                return new IOEntire(*io, m_stats, *this);
        }
        else
        {
            aMsgIO(kDebug, io, "Cache::Attache(), XrdOucCacheIO == NULL");
        }
    
        m_attached--;
    }
    return io;
}

int
Cache::isAttached()
{
    XrdSysMutexHelper lock(&m_io_mutex);
    return m_attached;
}

void
Cache::Detach(XrdOucCacheIO* io)
{
    aMsgIO(kInfo, io, "Cache::Detach()");
    XrdSysMutexHelper lock(&m_io_mutex);
    m_attached--;

    aMsgIO(kDebug, io, "Cache::Detach(), deleting IO object. Attach count = %d", m_attached);


    delete io;
}


//______________________________________________________________________________


void
FillFileMapRecurse( XrdOssDF* df, const std::string& path, std::map<std::string, time_t>& fcmap)
{
   char buff[256];
   XrdOucEnv env;
   int rdr;
   const size_t InfoExtLen = sizeof(XrdFileCache::Info::m_infoExtension);// cached var

   Factory& factory = Factory::GetInstance();
   while ( (rdr = df->Readdir(&buff[0], 256)) >= 0)
   {
      // printf("readdir [%s]\n", buff);
      std::string np = path + "/" + std::string(buff);
      size_t fname_len = strlen(&buff[0]);
      if (fname_len == 0  )
      {
         // std::cout << "Finish read dir.[" << np <<"] Break loop \n";
         break;
      }

      if (strncmp("..", &buff[0], 2) && strncmp(".", &buff[0], 1))
      {
          // AMT get rid of smart pointer
         std::auto_ptr<XrdOssDF> dh(factory.GetOss()->newDir(factory.RefConfiguration().m_username.c_str()));   
         std::auto_ptr<XrdOssDF> fh(factory.GetOss()->newFile(factory.RefConfiguration().m_username.c_str()));   

         if (fname_len > InfoExtLen && strncmp(&buff[fname_len - InfoExtLen ], XrdFileCache::Info::m_infoExtension , InfoExtLen) == 0)
         {
            fh->Open((np).c_str(),O_RDONLY, 0600, env);
            Info cinfo;
            time_t accessTime;
            cinfo.Read(fh.get());
            if (cinfo.getLatestAttachTime(accessTime, fh.get()))
            {
               aMsg(kDebug, "FillFileMapRecurse() checking %s accessTime %d ", buff, (int)accessTime);
               fcmap[np] = accessTime;
            }
            else
            {
               aMsg(kWarning, "FillFileMapRecurse() could not get access time for %s \n", np.c_str());
            }
         }
         else if ( dh->Opendir(np.c_str(), env)  >= 0 )
         {
            FillFileMapRecurse(dh.get(), np, fcmap);
         }
      }
   }
}


void
Cache::TempDirCleanup()
{
    // check state every sleepts seconds
    const static int sleept = 180;

    struct stat fstat;
    XrdOucEnv env;

    const Configuration& c = Factory::GetInstance().RefConfiguration();
    XrdOss* oss =  Factory::GetInstance().GetOss();
    std::auto_ptr<XrdOssDF> dh(oss->newDir(c.m_username.c_str()));
    while (1)
    {     
        // get amout of space to erase
        long long bytesToRemove = 0;
        struct statvfs fsstat;
        if(statvfs(c.m_temp_directory.c_str(), &fsstat) < 0 ) {
            aMsg(kError, "Factory::TempDirCleanup() can't get statvfs for dir [%s] \n", c.m_temp_directory.c_str());
            exit(1);
        }
        else
        {
            float oc = 1 - float(fsstat.f_bfree)/fsstat.f_blocks;
            aMsg(kInfo, "Factory::TempDirCleanup() occupade disk space == %f", oc);
            if (oc > c.m_hwm) {
                bytesToRemove = fsstat.f_bsize*fsstat.f_blocks*(oc - c.m_lwm);
                aMsg(kInfo, "Factory::TempDirCleanup() need space for  %lld bytes", bytesToRemove);
            }
        }

        if (bytesToRemove > 0)
        {
            typedef std::map<std::string, time_t> fcmap_t;
            fcmap_t fcmap;
            // make a sorted map of file patch by access time
            if (dh->Opendir(c.m_temp_directory.c_str(), env) >= 0) {
                FillFileMapRecurse(dh.get(), c.m_temp_directory, fcmap);

                // loop over map and remove files with highest value of access time
                for (fcmap_t::iterator i = fcmap.begin(); i != fcmap.end(); ++i)
                {  
                    std::string path = i->first;
                    // remove info file
                    if (oss->Stat(path.c_str(), &fstat) == XrdOssOK)
                    {
                        bytesToRemove -= fstat.st_size;
                        oss->Unlink(path.c_str());
                        aMsg(kInfo, "Factory::TempDirCleanup() removed %s size %lld ", path.c_str(), fstat.st_size);
                    }

                    // remove data file
                    path = path.substr(0, path.size() - strlen(XrdFileCache::Info::m_infoExtension));
                    if (oss->Stat(path.c_str(), &fstat) == XrdOssOK)
                    {
                        bytesToRemove -= fstat.st_size;
                        oss->Unlink(path.c_str());
                        aMsg(kInfo, "Factory::TempDirCleanup() removed %s size %lld ", path.c_str(), fstat.st_size);
                    }
                    if (bytesToRemove <= 0) 
                        break;
                }
            }
        }
        sleep(sleept);
    }
    dh->Close();
}
