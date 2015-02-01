
#include <fcntl.h>

#include "XrdSys/XrdSysPlugin.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"

#include "XrdThrottle/XrdThrottle.hh"
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
                            const char       *configfn)
{
   FileSystem* fs = NULL;
   FileSystem::Initialize(fs, native_fs, lp, configfn);
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
   return XrdSfsGetFileSystem_Internal(native_fs, lp, configfn);
}
}

XrdVERSIONINFO(XrdSfsGetFileSystem, FileSystem);

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
                       const char       *configfn)
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
      if (fs->Configure(fs->m_eroute, native_fs))
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
FileSystem::Configure(XrdSysError & log, XrdSfsFileSystem *native_fs)
{
   XrdOucEnv myEnv;
   XrdOucStream Config(&m_eroute, getenv("XRDINSTANCE"), &myEnv, "(Throttle Config)> ");
   int cfgFD;
   if (m_config_file.length() == 0)
   {
      log.Say("No filename specified.");
      return 1;
   }
   if ((cfgFD = open(m_config_file.c_str(), O_RDONLY)) < 0)
   {
      log.Emsg("Config", errno, "Unable to open configuration file", m_config_file.c_str());
      return 1;
   }
   Config.Attach(cfgFD);

   std::string fslib = OFS_NAME;

   char *var, *val;
   int NoGo = 0;
   while( (var = Config.GetMyFirstWord()) )
   {
      if (strcmp("throttle.fslib", var) == 0)
      {
         val = Config.GetWord();
         if (!val || !val[0]) {log.Emsg("Config", "fslib not specified."); continue;}
         fslib = val;
      }
      TS_Xeq("throttle.throttle", xthrottle);
      TS_Xeq("throttle.loadshed", xloadshed);
      TS_Xeq("throttle.trace", xtrace);
      if (NoGo)
      {
         log.Emsg("Config", "Throttle configuration failed.");
      }
   }

   // Load the filesystem object.
   m_sfs_ptr = native_fs ? native_fs : LoadFS(fslib, m_eroute, m_config_file);
   if (!m_sfs_ptr) return 1;

   // Overwrite the environment variable saying that throttling is the fslib.
   XrdOucEnv::Export("XRDOFSLIB", fslib.c_str());

   return 0;
}

/******************************************************************************/
/*                            x t h r o t t l e                               */
/******************************************************************************/

/* Function: xthrottle

   Purpose:  To parse the directive: throttle [data <drate>] [iops <irate>] [concurrency <climit>] [interval <rint>]

             <drate>    maximum bytes per second through the server.
             <irate>    maximum IOPS per second through the server.
             <climit>   maximum number of concurrent IO connections.
             <rint>     minimum interval in milliseconds between throttle re-computing.

   Output: 0 upon success or !0 upon failure.
*/
int
FileSystem::xthrottle(XrdOucStream &Config)
{
    long long drate = -1, irate = -1, rint = 1000, climit = -1;
    char *val;

    while ((val = Config.GetWord()))
    {
       if (strcmp("data", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_eroute.Emsg("Config", "data throttle limit not specified."); return 1;}
          if (XrdOuca2x::a2sz(m_eroute,"data throttle value",val,&drate,1)) return 1;
       }
       else if (strcmp("iops", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_eroute.Emsg("Config", "IOPS throttle limit not specified."); return 1;}
          if (XrdOuca2x::a2sz(m_eroute,"IOPS throttle value",val,&irate,1)) return 1;
       }
       else if (strcmp("rint", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_eroute.Emsg("Config", "recompute interval not specified."); return 1;}
          if (XrdOuca2x::a2sp(m_eroute,"recompute interval value",val,&rint,10)) return 1;
       }
       else if (strcmp("concurrency", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_eroute.Emsg("Config", "Concurrency limit not specified."); return 1;}
          if (XrdOuca2x::a2sz(m_eroute,"Concurrency limit value",val,&climit,1)) return 1;
       }
       else
       {
          m_eroute.Emsg("Config", "Warning - unknown throttle option specified", val, ".");
       }
    }

    m_throttle.SetThrottles(drate, irate, climit, static_cast<float>(rint)/1000.0);
    return 0;
}

/******************************************************************************/
/*                            x l o a d s h e d                               */
/******************************************************************************/

/* Function: xloadshed

   Purpose:  To parse the directive: loadshed host <hostname> [port <port>] [frequency <freq>]

             <hostname> hostname of server to shed load to.  Required
             <port>     port of server to shed load to.  Defaults to 1094
             <freq>     A value from 1 to 100 specifying how often to shed load
                        (1 = 1% chance; 100 = 100% chance; defaults to 10).

   Output: 0 upon success or !0 upon failure.
*/
int FileSystem::xloadshed(XrdOucStream &Config)
{
    long long port, freq;
    char *val;
    std::string hostname;

    while ((val = Config.GetWord()))
    {
       if (strcmp("host", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_eroute.Emsg("Config", "loadshed hostname not specified."); return 1;}
          hostname = val;
       }
       else if (strcmp("port", val) == 0)
       {
          if (!(val = Config.GetWord()))
             {m_eroute.Emsg("Config", "Port number not specified."); return 1;}
          if (XrdOuca2x::a2sz(m_eroute,"Port number",val,&port,1, 65536)) return 1;
       }
       else if (strcmp("frequency", val) == 0)
       {
           if (!(val = Config.GetWord()))
              {m_eroute.Emsg("Config", "Loadshed frequency not specified."); return 1;}
           if (XrdOuca2x::a2sz(m_eroute,"Loadshed frequency",val,&freq,1,100)) return 1;
       }
       else
       {
           m_eroute.Emsg("Config", "Warning - unknown loadshed option specified", val, ".");
       }
    }

    if (hostname.empty())
    {
        m_eroute.Emsg("Config", "must specify hostname for loadshed parameter.");
        return 1;
    }

    m_throttle.SetLoadShed(hostname, port, freq);
    return 0;
}

/******************************************************************************/
/*                                x t r a c e                                 */
/******************************************************************************/

/* Function: xtrace

   Purpose:  To parse the directive: trace <events>

             <events> the blank separated list of events to trace. Trace
                      directives are cummalative.

   Output: 0 upon success or 1 upon failure.
*/

int FileSystem::xtrace(XrdOucStream &Config)
{
   char *val;
   static const struct traceopts {const char *opname; int opval;} tropts[] =
   {
      {"all",       TRACE_ALL},
      {"off",       TRACE_NONE},
      {"none",      TRACE_NONE},
      {"debug",     TRACE_DEBUG},
      {"iops",      TRACE_IOPS},
      {"bandwidth", TRACE_BANDWIDTH},
      {"ioload",    TRACE_IOLOAD},
   };
   int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

   if (!(val = Config.GetWord()))
   {
      m_eroute.Emsg("Config", "trace option not specified");
      return 1;
   }
   while (val)
   {
      if (!strcmp(val, "off"))
      {
         trval = 0;
      }
      else
      {
         if ((neg = (val[0] == '-' && val[1])))
         {
            val++;
         }
         for (i = 0; i < numopts; i++)
         {
            if (!strcmp(val, tropts[i].opname))
            {
               if (neg)
               {
                  if (tropts[i].opval) trval &= ~tropts[i].opval;
                  else trval = TRACE_ALL;
               }
               else if (tropts[i].opval) trval |= tropts[i].opval;
               else trval = TRACE_NONE;
               break;
            }
         }
         if (i >= numopts)
         {
            m_eroute.Say("Config warning: ignoring invalid trace option '", val, "'.");
         }
      }
      val = Config.GetWord();
   }
   m_trace.What = trval;
   return 0;
}

