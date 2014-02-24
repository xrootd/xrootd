#ifndef __XRDFILECACHE_FACTORY_HH__
#define __XRDFILECACHE_FACTORY_HH__
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

#include <string>
#include <vector>
#include <map>

#include "XrdSys/XrdSysPthread.hh"
#include "XrdOuc/XrdOucCache.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdVersion.hh"

#include "XrdFileCacheDecision.hh"

class XrdOucStream;
class XrdSysError;

namespace XrdCl
{
   class Log;
}

namespace XrdFileCache
{
   //----------------------------------------------------------------------------
   //! Contains parameters configurable from the xrootd config file.
   //----------------------------------------------------------------------------
   struct Configuration
   {
      Configuration() :
         m_prefetchFileBlocks(false),
         m_cache_dir("/var/tmp/xrootd-file-cache"),
         m_username("nobody"),
         m_lwm(0.95),
         m_hwm(0.9),
         m_bufferSize(1024*1024),
         m_blockSize(128*1024*1024) {}

      bool m_prefetchFileBlocks;      //!< flag for enabling block-level operation
      std::string m_cache_dir;        //!< path of disk cache
      std::string m_username;         //!< username passed to oss plugin
      std::string m_osslib_name;      //!< oss library name (optional)

      float m_lwm;                    //!< cache purge low water mark
      float m_hwm;                    //!< cache purge high water mark

      long long m_bufferSize;         //!< prefetch buffer size, default 1MB
      long long m_blockSize;          //!< used with m_prefetchFileBlocks, default 128MB
   };


   //----------------------------------------------------------------------------
   //! Instantiates Cache and Decision plugins. Parses configuration file.
   //----------------------------------------------------------------------------
   class Factory : public XrdOucCache
   {
      public:
         //--------------------------------------------------------------------------
         //! Constructor
         //--------------------------------------------------------------------------
         Factory();

         //---------------------------------------------------------------------
         //! \brief Unused abstract method. This method is implemented in the
         //! the Cache class.
         //---------------------------------------------------------------------
         virtual XrdOucCacheIO *Attach(XrdOucCacheIO *, int Options=0) { return NULL; }

         //---------------------------------------------------------------------
         //! \brief Unused abstract method. This information is available in
         //! the Cache class.
         //---------------------------------------------------------------------
         virtual int isAttached() { return false; }

         //---------------------------------------------------------------------
         //! Creates XrdFileCache::Cache object
         //---------------------------------------------------------------------
         virtual XrdOucCache* Create(Parms &, XrdOucCacheIO::aprParms *aprP);

         XrdOss* GetOss() const { return m_output_fs; }

         //---------------------------------------------------------------------
         //! Getter for xrootd logger
         //---------------------------------------------------------------------
          XrdSysError& GetSysError() { return m_log; }

         //--------------------------------------------------------------------
         //! \brief Makes decision if the original XrdOucCacheIO should be cached.
         //!
         //! @param & URL of file
         //!
         //! @return decision if IO object will be cached.
         //--------------------------------------------------------------------
         bool Decide(XrdOucCacheIO*);

         //------------------------------------------------------------------------
         //! Reference XrdFileCache configuration
         //------------------------------------------------------------------------
         const Configuration& RefConfiguration() const { return m_configuration; }

       
         //---------------------------------------------------------------------
         //! \brief Parse configuration file
         //!
         //! @param logger             xrootd logger
         //! @param config_filename    path to configuration file
         //! @param parameters         optional parameters to be passed
         //!
         //! @return parse status
         //---------------------------------------------------------------------
         bool Config(XrdSysLogger *logger, const char *config_filename, const char *parameters);

         //---------------------------------------------------------------------
         //! Singleton access.
         //---------------------------------------------------------------------
         static Factory &GetInstance();

         //---------------------------------------------------------------------
         //! Version check.
         //---------------------------------------------------------------------
         static bool VCheck(XrdVersionInfo &urVersion) { return true; }

         //---------------------------------------------------------------------
         //! Thread function running disk cache purge periodically.
         //---------------------------------------------------------------------
         void CacheDirCleanup();

      private:
         bool CheckFileForDiskSpace(const char* path, long long fsize);
         void UnCheckFileForDiskSpace(const char* path);

         bool ConfigParameters(const char *);
         bool ConfigXeq(char *, XrdOucStream &);
         bool xolib(XrdOucStream &);
         bool xdlib(XrdOucStream &);

         XrdCl::Log* clLog() const { return XrdCl::DefaultEnv::GetLog(); }

         static Factory   *m_factory;   //!< this object

         XrdSysError       m_log;       //!< XFC namespace logger
         XrdOucCacheStats  m_stats;     //!< passed to cache, currently not used
         XrdOss           *m_output_fs; //!< disk cache file system

         std::vector<XrdFileCache::Decision*> m_decisionpoints; //!< decision plugins

         std::map<std::string, long long> m_filesInQueue;

         Configuration     m_configuration; //!< configurable parameters
   };
}

#endif
