
#include <fcntl.h>

#include "XrdSys/XrdSysPlugin.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"

#include "XrdThrottle/XrdThrottle.hh"
#include "XrdThrottle/XrdThrottleConfig.hh"
#include "XrdThrottle/XrdThrottleTrace.hh"

using namespace XrdThrottle;

#define OFS_NAME "libXrdOfs.so"

/*
 * Note nothing in this file is thread-safe.
 */

static XrdSfsFileSystem *
LoadFS(const std::string &fslib, XrdSysError &eDest, const std::string &config_file){
   // Load the library
   XrdSysPlugin ofsLib(&eDest, fslib.c_str(), "fslib", NULL);
   XrdSfsFileSystem *fs;
   if (fslib == OFS_NAME)
   {
      extern XrdSfsFileSystem *XrdSfsGetDefaultFileSystem(XrdSfsFileSystem *native_fs,
                                             XrdSysLogger     *lp,
                                             const char       *configfn,
                                             XrdOucEnv        *EnvInfo);

      if (!(fs = XrdSfsGetDefaultFileSystem(0, eDest.logger(), config_file.c_str(), 0)))
      {
         eDest.Emsg("Config", "Unable to load OFS filesystem.");
      }
   }
   else
   {
      XrdSfsFileSystem *(*ep)(XrdSfsFileSystem *, XrdSysLogger *, const char *);
      if (!(ep = (XrdSfsFileSystem *(*)(XrdSfsFileSystem *,XrdSysLogger *,const char *))
                                       ofsLib.getPlugin("XrdSfsGetFileSystem")))
         return NULL;
      if (!(fs = (*ep)(0, eDest.logger(), config_file.c_str())))
      {
         eDest.Emsg("Config", "Unable to create file system object via", fslib.c_str());
         return NULL;
      }
   }
   ofsLib.Persist();

   return fs;
}

namespace XrdThrottle {
XrdSfsFileSystem *
XrdSfsGetFileSystem_Internal(XrdSfsFileSystem *native_fs,
                            XrdSysLogger     *lp,
                            const char       *configfn,
                            XrdOucEnv        *envP)
{
   FileSystem* fs = NULL;
   if (envP && envP->GetInt("XrdOssThrottle") == 1) {
      XrdSysError eDest(lp, "XrdOssThrottle");
      eDest.Emsg("Config", "XrdOssThrottle is loaded; not stacking XrdThrottle on OFS.  "
         "This is a warning for backward compatability; this configuration may generate an "
         "error in the future.");
      return native_fs;
   }
   FileSystem::Initialize(fs, native_fs, lp, configfn, envP);
   return fs;
}
}

// Export the symbol necessary for this to be dynamically loaded.
extern "C" {
XrdSfsFileSystem *
XrdSfsGetFileSystem(XrdSfsFileSystem *native_fs,
                    XrdSysLogger     *lp,
                    const char       *configfn)
{
   return XrdSfsGetFileSystem_Internal(native_fs, lp, configfn, nullptr);
}

XrdSfsFileSystem *
XrdSfsGetFileSystem2(XrdSfsFileSystem *native_fs,
                    XrdSysLogger     *lp,
                    const char       *configfn,
                    XrdOucEnv        *envP)
{
   return XrdSfsGetFileSystem_Internal(native_fs, lp, configfn, envP);
}
}

XrdVERSIONINFO(XrdSfsGetFileSystem, FileSystem);
XrdVERSIONINFO(XrdSfsGetFileSystem2, FileSystem);

FileSystem* FileSystem::m_instance = 0;

FileSystem::FileSystem()
   : m_eroute(0), m_trace(&m_eroute), m_sfs_ptr(0), m_initialized(false), m_throttle(&m_eroute, &m_trace)
{
   myVersion = &XrdVERSIONINFOVAR(XrdSfsGetFileSystem);
}

FileSystem::~FileSystem() {}

void
FileSystem::Initialize(FileSystem      *&fs,
                       XrdSfsFileSystem *native_fs, 
                       XrdSysLogger     *lp,
                       const char       *configfn,
                       XrdOucEnv        *envP)
{
   fs = NULL;
   if (m_instance == NULL && !(m_instance = new FileSystem()))
   {
      return;
   }
   fs = m_instance;
   if (!fs->m_initialized)
   {
      fs->m_config_file = configfn;
      fs->m_eroute.logger(lp);
      fs->m_eroute.Say("Initializing a Throttled file system.");
      if (fs->Configure(fs->m_eroute, native_fs, envP))
      {
         fs->m_eroute.Say("Initialization of throttled file system failed.");
         fs = NULL;
         return;
      }
      fs->m_throttle.Init();
      fs->m_initialized = true;
   }
}

#define TS_Xeq(key, func) NoGo = (strcmp(key, var) == 0) ? func(Config) : 0
int
FileSystem::Configure(XrdSysError & log, XrdSfsFileSystem *native_fs, XrdOucEnv *envP)
{
   XrdThrottle::Configuration Config(log, envP);
   if (Config.Configure(m_config_file))
   {
      log.Emsg("Config", "Unable to load configuration file", m_config_file.c_str());
      return 1;
   }

   m_throttle.FromConfig(Config);
   m_trace.What = Config.GetTraceLevels();

   // Load the filesystem object.
   m_sfs_ptr = native_fs ? native_fs : LoadFS(Config.GetFileSystemLibrary(), m_eroute, m_config_file);
   if (!m_sfs_ptr) return 1;

   // Overwrite the environment variable saying that throttling is the fslib.
   XrdOucEnv::Export("XRDOFSLIB", Config.GetFileSystemLibrary().c_str());

   if (envP)
   {
       auto gstream = reinterpret_cast<XrdXrootdGStream*>(envP->GetPtr("Throttle.gStream*"));
       log.Say("Config", "Throttle g-stream has", gstream ? "" : " NOT", " been configured via xrootd.mongstream directive");
       m_throttle.SetMonitor(gstream);
   }

   // The Feature function is not a virtual but implemented by the base class to
   // look at a protected member.  Thus, to forward the call, we need to copy
   // from the underlying filesystem
   FeatureSet = m_sfs_ptr->Features();
   return 0;
}
