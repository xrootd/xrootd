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

#include <sstream>
#include <string>
#include <fcntl.h>
#include <stdio.h>


#include <map>


#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOss/XrdOssCache.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOfs/XrdOfsConfigPI.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdVersion.hh"
#include "XrdPosix/XrdPosixXrootd.hh"
#include "XrdCl/XrdClConstants.hh"

#include "XrdFileCache.hh"
#include "XrdFileCacheFactory.hh"
#include "XrdFileCachePrefetch.hh"


using namespace XrdFileCache;
namespace {
static long long s_diskSpacePrecisionFactor = 10000000;
}

XrdVERSIONINFO(XrdOucGetCache, XrdFileCache);


Factory * Factory::m_factory = NULL;

void *CacheDirCleanupThread(void* cache_void)
{
   Factory::GetInstance().CacheDirCleanup();
   return NULL;
}


Factory::Factory()
   : m_log(0, "XrdFileCache_")
{}

extern "C"
{
XrdOucCache *XrdOucGetCache(XrdSysLogger *logger,
                            const char   *config_filename,
                            const char   *parameters)
{
   XrdSysError err(0, "");
   err.logger(logger);
   err.Emsg("Retrieve", "Retrieving a caching proxy factory.");
   Factory &factory = Factory::GetInstance();
   if (!factory.Config(logger, config_filename, parameters))
   {
      err.Emsg("Retrieve", "Error - unable to create a factory.");
      return NULL;
   }
   err.Emsg("Retrieve", "Success - returning a factory.");


   pthread_t tid;
   XrdSysThread::Run(&tid, CacheDirCleanupThread, NULL, 0, "XrdFileCache CacheDirCleanup");
   return &factory;
}
}

Factory &Factory::GetInstance()
{
   if (m_factory == NULL)
      m_factory = new Factory();
   return *m_factory;
}

XrdOucCache *Factory::Create(Parms & parms, XrdOucCacheIO::aprParms * prParms)
{
   clLog()->Info(XrdCl::AppMsg, "Factory::Create() new cache object");
   return new Cache(m_stats);
}


/* Function: xdlib

   Purpose:  To parse the directive: decisionlib <path> [<parms>]

             <path>  the path of the decision library to be used.
             <parms> optional parameters to be passed.


   Output: true upon success or false upon failure.
 */
bool Factory::xdlib(XrdOucStream &Config)
{
   const char*  val;

   std::string libp;
   if (!(val = Config.GetWord()) || !val[0])
   {
      clLog()->Info(XrdCl::AppMsg, " Factory:;Config() decisionlib not specified; always caching files");
      return true;
   }
   else
   {
      libp = val;
   }

   const char* params;
   params = (val[0]) ?  Config.GetWord() : 0;


   XrdOucPinLoader* myLib = new XrdOucPinLoader(&m_log, 0, "decisionlib",
                                                libp.c_str());

   Decision *(*ep)(XrdSysError&);
   ep = (Decision *(*)(XrdSysError&))myLib->Resolve("XrdFileCacheGetDecision");
   if (!ep) {myLib->Unload(true); return false;}

   Decision * d = ep(m_log);
   if (!d)
   {
      clLog()->Error(XrdCl::AppMsg, "Factory::Config() decisionlib was not able to create a decision object");
      return false;
   }
   if (params)
      d->ConfigDecision(params);

   m_decisionpoints.push_back(d);
   return true;
}
//______________________________________________________________________________


bool Factory::Decide(XrdOucCacheIO* io)
{
   //   if ( CheckFileForDiskSpace(io->Path(), io->FSize()) == false )
   //  return false;

   if(!m_decisionpoints.empty())
   {
      std::string filename = io->Path();
      std::vector<Decision*>::const_iterator it;
      for ( it = m_decisionpoints.begin(); it != m_decisionpoints.end(); ++it)
      {
         XrdFileCache::Decision *d = *it;
         if (!d) continue;
         if (!d->Decide(filename, *m_output_fs))
         {
            return false;
         }
      }
   }

   return true;
}



//______________________________________________________________________________


bool Factory::Config(XrdSysLogger *logger, const char *config_filename, const char *parameters)
{
   m_log.logger(logger);

   const char * cache_env;
   if (!(cache_env = getenv("XRDPOSIX_CACHE")) || !*cache_env)
      XrdOucEnv::Export("XRDPOSIX_CACHE", "mode=s&optwr=0");

   XrdOucEnv myEnv;
   XrdOucStream Config(&m_log, getenv("XRDINSTANCE"), &myEnv, "=====> ");

   if (!config_filename || !*config_filename)
   {
      clLog()->Warning(XrdCl::AppMsg, "Factory::Config() configuration file not specified.");
      return false;
   }

   int fd;
   if ( (fd = open(config_filename, O_RDONLY, 0)) < 0)
   {
      clLog()->Error(XrdCl::AppMsg, "Factory::Config() can't open configuration file %s", config_filename);
      return false;
   }

   Config.Attach(fd);

   // Obtain plugin configurator
   XrdOfsConfigPI *ofsCfg = XrdOfsConfigPI::New(config_filename,&Config,&m_log,
                                                &XrdVERSIONINFOVAR(XrdOucGetCache));
   if (!ofsCfg) return false;

   // Actual parsing of the config file.
   bool retval = true;
   char *var;
   while((var = Config.GetMyFirstWord()))
   {
      if (!strcmp(var,"pfc.osslib"))
      {
         ofsCfg->Parse(XrdOfsConfigPI::theOssLib);
      }
      else if (!strcmp(var,"pfc.decisionlib"))
      {
         xdlib(Config);
      }
      else if (!strncmp(var,"pfc.", 4))
      {
         retval = ConfigParameters(std::string(var+4), Config);
      }

      if (!retval)
      {
         retval = false;
         clLog()->Error(XrdCl::AppMsg, "Factory::Config() error in parsing");
         break;
      }

   }

   Config.Close();


   if (retval)
   {
      if (ofsCfg->Load(XrdOfsConfigPI::theOssLib))  {
          ofsCfg->Plugin(m_output_fs);
          XrdOssCache_FS* ocfs = XrdOssCache::Find("public");
          ocfs->Add(m_configuration.m_cache_dir.c_str());
      }
      else
      {
         clLog()->Error(XrdCl::AppMsg, "Factory::Config() Unable to create an OSS object");
         retval = false;
         m_output_fs = 0;
      }

  
      char buff[2048];
      snprintf(buff, sizeof(buff), "result\n"
               "\tpfc.cachedir %s\n"
               "\tpfc.blocksize %lld\n"
               "\tpfc.nramread %d\n\tpfc.nramprefetch %d\n",
               m_configuration.m_cache_dir.c_str() , 
               m_configuration.m_bufferSize, 
               m_configuration.m_NRamBuffersRead, m_configuration.m_NRamBuffersPrefetch );


      if (m_configuration.m_hdfsmode)
      {
         char buff2[512];
         snprintf(buff2, sizeof(buff2), "\tpfc.hdfsmode hdfsbsize %lld \n", m_configuration.m_hdfsbsize);
         m_log.Emsg("", buff, buff2);   
      }          
      else {

      m_log.Emsg("Config", buff);
      }
   }

   m_log.Emsg("Config", "Configuration =  ", retval ? "Success" : "Fail");

   if (ofsCfg) delete ofsCfg;
   return retval;
}

//______________________________________________________________________________


bool Factory::ConfigParameters(std::string part, XrdOucStream& config )
{   
   XrdSysError err(0, "");
   if ( part == "user" )
   {
      m_configuration.m_username = config.GetWord();
   }
   else if  ( part == "cachedir" )
   {
      m_configuration.m_cache_dir = config.GetWord();
   }
   else if  ( part == "diskusage" )
   {
      const char* minV = config.GetWord();
      if (minV) {
         m_configuration.m_lwm = ::atof(minV);
         const char* maxV = config.GetWord();
         if (maxV) {
            m_configuration.m_hwm = ::atof(maxV);
         }
      }
      else {
         clLog()->Error(XrdCl::AppMsg, "Factory::ConfigParameters() pss.diskUsage min max value not specified");
      }
   }
   else if  ( part == "blocksize" )
   {
      long long minBSize = 64 * 1024;
      long long maxBSize = 16 * 1024 * 1024;
      if ( XrdOuca2x::a2sz(m_log, "get block size", config.GetWord(), &m_configuration.m_bufferSize, minBSize, maxBSize))
      {
         return false;
      }
   }
   else if (part == "nramread")
   {
      m_configuration.m_NRamBuffersRead = ::atoi(config.GetWord());
   }
   else if (part == "nramprefetch")
   {
      m_configuration.m_NRamBuffersPrefetch = ::atoi(config.GetWord());
   }
   else if ( part == "hdfsmode" )
   {
      m_configuration.m_hdfsmode = true;

      const char* params =  config.GetWord();
      if (params) {
         if (!strncmp("hdfsbsize", params, 9)) {
            long long minBlSize = 128 * 1024;
            long long maxBlSize = 1024 * 1024 * 1024;
            params = config.GetWord();
            if ( XrdOuca2x::a2sz(m_log, "Error getting file fragment size", params, &m_configuration.m_hdfsbsize, minBlSize, maxBlSize))
            {
               return false;
            }
         }
         else {
            m_log.Emsg("Config", "Error setting the fragment size parameter name");
            return false;        
         }
      }         
   }
   else
   {
      m_log.Emsg("Factory::ConfigParameters() unmatched pfc parameter", part.c_str());
      return false;
   }

   assert ( config.GetWord() == 0 && "Factory::ConfigParameters() lost argument"); 

   return true;
}

//______________________________________________________________________________


void FillFileMapRecurse( XrdOssDF* iOssDF, const std::string& path, std::map<std::string, time_t>& fcmap)
{
   char buff[256];
   XrdOucEnv env;
   int rdr;
   const size_t InfoExtLen = strlen(XrdFileCache::Info::m_infoExtension);  // cached var
   XrdCl::Log *log = XrdCl::DefaultEnv::GetLog();

   Factory& factory = Factory::GetInstance();
   while ( (rdr = iOssDF->Readdir(&buff[0], 256)) >= 0)
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
         XrdOssDF* dh = factory.GetOss()->newDir(factory.RefConfiguration().m_username.c_str());
         XrdOssDF* fh = factory.GetOss()->newFile(factory.RefConfiguration().m_username.c_str());

         if (fname_len > InfoExtLen && strncmp(&buff[fname_len - InfoExtLen ], XrdFileCache::Info::m_infoExtension, InfoExtLen) == 0)
         {
            fh->Open((np).c_str(),O_RDONLY, 0600, env);
            Info cinfo;
            time_t accessTime;
            cinfo.Read(fh);
            if (cinfo.GetLatestDetachTime(accessTime, fh))
            {
               log->Debug(XrdCl::AppMsg, "FillFileMapRecurse() checking %s accessTime %d ", buff, (int)accessTime);
               fcmap[np] = accessTime;
            }
            else
            {
               log->Warning(XrdCl::AppMsg, "FillFileMapRecurse() could not get access time for %s \n", np.c_str());
            }
         }
         else if ( dh->Opendir(np.c_str(), env)  >= 0 )
         {
            FillFileMapRecurse(dh, np, fcmap);
         }

         delete dh; dh = 0;
         delete fh; fh = 0;
      }
   }
}


void Factory::CacheDirCleanup()
{
   // check state every sleep seconds
   const static int sleept = 300;
   struct stat fstat;
   XrdOucEnv env;

   XrdOss* oss =  Factory::GetInstance().GetOss();
   XrdOssDF* dh = oss->newDir(m_configuration.m_username.c_str());
   XrdOssVSInfo sP;

   while (1)
   {
      // get amount of space to erase
      long long bytesToRemove = 0;
      if( oss->StatVS(&sP, "public", 1) < 0 )
      {
         clLog()->Error(XrdCl::AppMsg, "Factory::CacheDirCleanup() can't get statvs for dir [%s] \n", m_configuration.m_cache_dir.c_str());
         exit(1);
      }
      else
      {
         float oc = 1 - float(sP.Free)/(sP.Total);
         clLog()->Debug(XrdCl::AppMsg, "Factory::CacheDirCleanup() occupates disk space == %f", oc);
         if (oc > m_configuration.m_hwm)
         {
            long long bytesToRemoveLong = static_cast<long long> ((oc - m_configuration.m_lwm) * static_cast<float>(s_diskSpacePrecisionFactor));
            bytesToRemove = (sP.Total * bytesToRemoveLong) / s_diskSpacePrecisionFactor;
            clLog()->Info(XrdCl::AppMsg, "Factory::CacheDirCleanup() need space for  %lld bytes", bytesToRemove);
         }
      }

      if (bytesToRemove > 0)
      {
         typedef std::map<std::string, time_t> fcmap_t;
         fcmap_t fcmap;
         // make a sorted map of file patch by access time
         if (dh->Opendir(m_configuration.m_cache_dir.c_str(), env) >= 0)
         {
            FillFileMapRecurse(dh, m_configuration.m_cache_dir, fcmap);

            // loop over map and remove files with highest value of access time
            for (fcmap_t::iterator i = fcmap.begin(); i != fcmap.end(); ++i)
            {
               std::string path = i->first;
               // remove info file
               if (oss->Stat(path.c_str(), &fstat) == XrdOssOK)
               {
                  bytesToRemove -= fstat.st_size;
                  oss->Unlink(path.c_str());
                  clLog()->Info(XrdCl::AppMsg, "Factory::CacheDirCleanup() removed %s size %lld ", path.c_str(), fstat.st_size);
               }

               // remove data file
               path = path.substr(0, path.size() - strlen(XrdFileCache::Info::m_infoExtension));
               if (oss->Stat(path.c_str(), &fstat) == XrdOssOK)
               {
                  bytesToRemove -= fstat.st_size;
                  oss->Unlink(path.c_str());
                  clLog()->Info(XrdCl::AppMsg, "Factory::CacheDirCleanup() removed %s size %lld ", path.c_str(), fstat.st_size);
               }
               if (bytesToRemove <= 0)
                  break;
            }
         }
      }
      sleep(sleept);
   }
   dh->Close();
   delete dh; dh =0;
}



bool Factory::CheckFileForDiskSpace(const char* path, long long fsize)
{
    long long inQueue = 0;
    for (std::map<std::string, long long>::iterator i = m_filesInQueue.begin(); i!= m_filesInQueue.end(); ++i)
        inQueue += i->second;


    XrdOssVSInfo sP;
    long long availableSpace = 0;

    if(m_output_fs->StatVS(&sP, "public", 1) < 0 ) {
        clLog()->Error(XrdCl::AppMsg, "Factory:::CheckFileForDiskSpace can't get statvfs for dir [%s] \n", m_configuration.m_cache_dir.c_str());
        exit(1);
    }
    float oc = 1 - float(sP.Free)/sP.Total;
    long long availableSpaceLong =  static_cast<long long>((m_configuration.m_hwm -oc)* static_cast<float>(s_diskSpacePrecisionFactor));
    if (oc < m_configuration.m_hwm) {
        availableSpace = (sP.Free * availableSpaceLong) / s_diskSpacePrecisionFactor;

        if (availableSpace > fsize) {
            m_filesInQueue[path] = fsize;
            return true;
        }

    }
    clLog()->Error(XrdCl::AppMsg, "Factory:::CheckFileForDiskSpace not enugh space , availableSpace = %lld \n", availableSpace);
    return false;
}


void Factory::UnCheckFileForDiskSpace(const char* path)
{
    m_filesInQueue.erase(path);
}
