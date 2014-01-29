//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClForkHandler.hh"
#include "XrdCl/XrdClFileTimer.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClMonitor.hh"
#include "XrdCl/XrdClCheckSumManager.hh"
#include "XrdCl/XrdClTransportManager.hh"
#include "XrdCl/XrdClPlugInManager.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdSys/XrdSysUtils.hh"

#include <libgen.h>
#include <cstring>
#include <map>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

XrdVERSIONINFO( XrdCl, client );

namespace
{
  //----------------------------------------------------------------------------
  // Translate a string into a topic mask
  //----------------------------------------------------------------------------
  struct MaskTranslator
  {
    //--------------------------------------------------------------------------
    // Initialize the translation array
    //--------------------------------------------------------------------------
    MaskTranslator()
    {
      masks["AppMsg"]             = XrdCl::AppMsg;
      masks["UtilityMsg"]         = XrdCl::UtilityMsg;
      masks["FileMsg"]            = XrdCl::FileMsg;
      masks["PollerMsg"]          = XrdCl::PollerMsg;
      masks["PostMasterMsg"]      = XrdCl::PostMasterMsg;
      masks["XRootDTransportMsg"] = XrdCl::XRootDTransportMsg;
      masks["TaskMgrMsg"]         = XrdCl::TaskMgrMsg;
      masks["XRootDMsg"]          = XrdCl::XRootDMsg;
      masks["FileSystemMsg"]      = XrdCl::FileSystemMsg;
      masks["AsyncSockMsg"]       = XrdCl::AsyncSockMsg;
      masks["JobMgrMsg"]          = XrdCl::JobMgrMsg;
      masks["PlugInMgrMsg"]       = XrdCl::PlugInMgrMsg;
    }

    //--------------------------------------------------------------------------
    // Translate the mask
    //--------------------------------------------------------------------------
    uint64_t translateMask( const std::string mask )
    {
      if( mask == "" )
        return 0xffffffffffffffffULL;

      std::vector<std::string>           topics;
      std::vector<std::string>::iterator it;
      XrdCl::Utils::splitString( topics, mask, "|" );

      uint64_t resultMask = 0;
      std::map<std::string, uint64_t>::iterator maskIt;
      for( it = topics.begin(); it != topics.end(); ++it )
      {
        //----------------------------------------------------------------------
        // Check for resetting pseudo topics
        //----------------------------------------------------------------------
        if( *it == "All" )
        {
          resultMask = 0xffffffffffffffffULL;
          continue;
        }

        if( *it == "None" )
        {
          resultMask = 0ULL;
          continue;
        }

        //----------------------------------------------------------------------
        // Check whether given topic should be disabled or enabled
        //----------------------------------------------------------------------
        std::string topic = *it;
        bool disable      = false;
        if( !topic.empty() && topic[0] == '^' )
        {
          disable = true;
          topic   = topic.substr( 1, topic.length()-1 );
        }

        maskIt = masks.find( topic );
        if( maskIt == masks.end() )
          continue;

        if( disable )
          resultMask &= (0xffffffffffffffffULL ^ maskIt->second);
        else
          resultMask |= maskIt->second;
      }

      return resultMask;
    }

    std::map<std::string, uint64_t> masks;
  };
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Statics
  //----------------------------------------------------------------------------
  XrdSysMutex        DefaultEnv::sInitMutex;
  Env               *DefaultEnv::sEnv                = 0;
  PostMaster        *DefaultEnv::sPostMaster         = 0;
  Log               *DefaultEnv::sLog                = 0;
  ForkHandler       *DefaultEnv::sForkHandler        = 0;
  FileTimer         *DefaultEnv::sFileTimer          = 0;
  Monitor           *DefaultEnv::sMonitor            = 0;
  XrdSysPlugin      *DefaultEnv::sMonitorLibHandle   = 0;
  bool               DefaultEnv::sMonitorInitialized = false;
  CheckSumManager   *DefaultEnv::sCheckSumManager    = 0;
  TransportManager  *DefaultEnv::sTransportManager   = 0;
  PlugInManager     *DefaultEnv::sPlugInManager      = 0;

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  DefaultEnv::DefaultEnv()
  {
    PutInt( "ConnectionWindow",      DefaultConnectionWindow     );
    PutInt( "ConnectionRetry",       DefaultConnectionRetry      );
    PutInt( "RequestTimeout",        DefaultRequestTimeout       );
    PutInt( "SubStreamsPerChannel",  DefaultSubStreamsPerChannel );
    PutInt( "TimeoutResolution",     DefaultTimeoutResolution    );
    PutInt( "StreamErrorWindow",     DefaultStreamErrorWindow    );
    PutInt( "RunForkHandler",        DefaultRunForkHandler       );
    PutInt( "RedirectLimit",         DefaultRedirectLimit        );
    PutInt( "WorkerThreads",         DefaultWorkerThreads        );
    PutInt( "CPChunkSize",           DefaultCPChunkSize          );
    PutInt( "CPParallelChunks",      DefaultCPParallelChunks     );
    PutInt( "DataServerTTL",         DefaultDataServerTTL        );
    PutInt( "LoadBalancerTTL",       DefaultLoadBalancerTTL      );
    PutString( "PollerPreference",   DefaultPollerPreference     );
    PutString( "ClientMonitor",      DefaultClientMonitor        );
    PutString( "ClientMonitorParam", DefaultClientMonitorParam   );
    PutString( "NetworkStack",       DefaultNetworkStack         );
    PutString( "PlugInConfDir",      DefaultPlugInConfDir        );
    PutString( "PlugIn",             DefaultPlugIn               );

    char *tmp = strdup( XrdSysUtils::ExecName() );
    char *appName = basename( tmp );
    PutString( "AppName", appName );
    free( tmp );

    ImportInt(    "ConnectionWindow",     "XRD_CONNECTIONWINDOW"     );
    ImportInt(    "ConnectionRetry",      "XRD_CONNECTIONRETRY"      );
    ImportInt(    "RequestTimeout",       "XRD_REQUESTTIMEOUT"       );
    ImportInt(    "SubStreamsPerChannel", "XRD_SUBSTREAMSPERCHANNEL" );
    ImportInt(    "TimeoutResolution",    "XRD_TIMEOUTRESOLUTION"    );
    ImportInt(    "StreamErrorWindow",    "XRD_STREAMERRORWINDOW"    );
    ImportInt(    "RunForkHandler",       "XRD_RUNFORKHANDLER"       );
    ImportInt(    "RedirectLimit",        "XRD_REDIRECTLIMIT"        );
    ImportInt(    "WorkerThreads",        "XRD_WORKERTHREADS"        );
    ImportInt(    "CPChunkSize",          "XRD_CPCHUNKSIZE"          );
    ImportInt(    "CPParallelChunks",     "XRD_CPPARALLELCHUNKS"     );
    ImportInt(    "DataServerTTL",        "XRD_DATASERVERTTL"        );
    ImportInt(    "LoadBalancerTTL",      "XRD_LOADBALANCERTTL"      );
    ImportString( "PollerPreference",     "XRD_POLLERPREFERENCE"     );
    ImportString( "ClientMonitor",        "XRD_CLIENTMONITOR"        );
    ImportString( "ClientMonitorParam",   "XRD_CLIENTMONITORPARAM"   );
    ImportString( "NetworkStack",         "XRD_NETWORKSTACK"         );
    ImportString( "AppName",              "XRD_APPNAME"              );
    ImportString( "PlugIn",               "XRD_PLUGIN"               );
    ImportString( "PlugInConfDir",        "XRD_PLUGINCONFDIR"        );
  }

  //----------------------------------------------------------------------------
  // Get default client environment
  //----------------------------------------------------------------------------
  Env *DefaultEnv::GetEnv()
  {
    return sEnv;
  }

  //----------------------------------------------------------------------------
  // Get default post master
  //----------------------------------------------------------------------------
  PostMaster *DefaultEnv::GetPostMaster()
  {
    if( unlikely(!sPostMaster) )
    {
      XrdSysMutexHelper scopedLock( sInitMutex );
      if( sPostMaster )
        return sPostMaster;
      sPostMaster = new PostMaster();

      if( !sPostMaster->Initialize() )
      {
        delete sPostMaster;
        sPostMaster = 0;
        return 0;
      }

      if( !sPostMaster->Start() )
      {
        sPostMaster->Finalize();
        delete sPostMaster;
        sPostMaster = 0;
        return 0;
      }
      sForkHandler->RegisterPostMaster( sPostMaster );
      sPostMaster->GetTaskManager()->RegisterTask( sFileTimer, time(0), false );
    }
    return sPostMaster;
  }

  //----------------------------------------------------------------------------
  // Get log
  //----------------------------------------------------------------------------
  Log *DefaultEnv::GetLog()
  {
    return sLog;
  }

  //----------------------------------------------------------------------------
  // Set log level
  //----------------------------------------------------------------------------
  void DefaultEnv::SetLogLevel( const std::string &level )
  {
    Log *log = GetLog();
    log->SetLevel( level );
  }

  //----------------------------------------------------------------------------
  // Set log file
  //----------------------------------------------------------------------------
  bool DefaultEnv::SetLogFile( const std::string &filepath )
  {
    Log *log = GetLog();
    LogOutFile *out = new LogOutFile();

    if( out->Open( filepath ) )
    {
      log->SetOutput( out );
      return true;
    }

    delete out;
    return false;
  }

  //----------------------------------------------------------------------------
  //! Set log mask.
  //------------------------------------------------------------------------
  void DefaultEnv::SetLogMask( const std::string &level,
                               const std::string &mask )
  {
    Log *log = GetLog();
    MaskTranslator translator;
    uint64_t topicMask = translator.translateMask( mask );

    if( level == "All" )
    {
      log->SetMask( Log::ErrorMsg,   topicMask );
      log->SetMask( Log::WarningMsg, topicMask );
      log->SetMask( Log::InfoMsg,    topicMask );
      log->SetMask( Log::DebugMsg,   topicMask );
      log->SetMask( Log::DumpMsg,    topicMask );
      return;
    }

    log->SetMask( level, topicMask );
  }

  //----------------------------------------------------------------------------
  // Get fork handler
  //----------------------------------------------------------------------------
  ForkHandler *DefaultEnv::GetForkHandler()
  {
    return sForkHandler;
  }

  //----------------------------------------------------------------------------
  // Get fork handler
  //----------------------------------------------------------------------------
  FileTimer *DefaultEnv::GetFileTimer()
  {
    return sFileTimer;
  }

  //----------------------------------------------------------------------------
  // Get the monitor object
  //----------------------------------------------------------------------------
  Monitor *DefaultEnv::GetMonitor()
  {
    if( unlikely( !sMonitorInitialized ) )
    {
      XrdSysMutexHelper scopedLock( sInitMutex );
      if( !sMonitorInitialized )
      {
        //----------------------------------------------------------------------
        // Check the environment settings
        //----------------------------------------------------------------------
        Env *env = GetEnv();
        Log *log = GetLog();
        sMonitorInitialized = true;
        std::string monitorLib = DefaultClientMonitor;
        env->GetString( "ClientMonitor", monitorLib );
        if( monitorLib.empty() )
        {
          log->Debug( UtilityMsg, "Monitor library name not set. No "
                      "monitoring" );
          return 0;
        }

        std::string monitorParam = DefaultClientMonitorParam;
        env->GetString( "ClientMonitorParam", monitorParam );

        log->Debug( UtilityMsg, "Initializing monitoring, lib: %s, param: %s",
                    monitorLib.c_str(), monitorParam.c_str() );

        //----------------------------------------------------------------------
        // Loading the plugin
        //----------------------------------------------------------------------
        char *errBuffer = new char[4000];
        sMonitorLibHandle = new XrdSysPlugin(
                                 errBuffer, 4000, monitorLib.c_str(),
                                 monitorLib.c_str(),
                                 &XrdVERSIONINFOVAR( XrdCl ) );

        typedef XrdCl::Monitor *(*MonLoader)(const char *, const char *);
        MonLoader loader;
        loader = (MonLoader)sMonitorLibHandle->getPlugin( "XrdClGetMonitor" );
        if( !loader )
        {
          log->Error( UtilityMsg, "Unable to initialize user monitoring: %s",
                      errBuffer );
          delete [] errBuffer;
          delete sMonitorLibHandle; sMonitorLibHandle = 0;
          return 0;
        }

        //----------------------------------------------------------------------
        // Instantiating the monitor object
        //----------------------------------------------------------------------
        const char *param = monitorParam.empty() ? 0 : monitorParam.c_str();
        sMonitor = (*loader)( XrdSysUtils::ExecName(), param );

        if( !sMonitor )
        {
          log->Error( UtilityMsg, "Unable to initialize user monitoring: %s",
                      errBuffer );
          delete [] errBuffer;
          delete sMonitorLibHandle; sMonitorLibHandle = 0;
          return 0;
        }
        log->Debug( UtilityMsg, "Successfully initialized monitoring from: %s",
                    monitorLib.c_str() );
        delete [] errBuffer;
      }
    }
    return sMonitor;
  }

  //----------------------------------------------------------------------------
  // Get checksum manager
  //----------------------------------------------------------------------------
  CheckSumManager *DefaultEnv::GetCheckSumManager()
  {
    if( unlikely( !sCheckSumManager ) )
    {
      XrdSysMutexHelper scopedLock( sInitMutex );
      if( !sCheckSumManager )
        sCheckSumManager = new CheckSumManager();
    }
    return sCheckSumManager;
  }

  //----------------------------------------------------------------------------
  // Get transport manager
  //----------------------------------------------------------------------------
  TransportManager *DefaultEnv::GetTransportManager()
  {
    if( unlikely( !sTransportManager ) )
    {
      XrdSysMutexHelper scopedLock( sInitMutex );
      if( !sTransportManager )
        sTransportManager = new TransportManager();
    }
    return sTransportManager;
  }

  //----------------------------------------------------------------------------
  // Get plug-in manager
  //----------------------------------------------------------------------------
  PlugInManager *DefaultEnv::GetPlugInManager()
  {
    return sPlugInManager;
  }


  //----------------------------------------------------------------------------
  // Initialize the environment
  //----------------------------------------------------------------------------
  void DefaultEnv::Initialize()
  {
    sLog           = new Log();
    SetUpLog();

    sEnv           = new DefaultEnv();
    sForkHandler   = new ForkHandler();
    sFileTimer     = new FileTimer();
    sPlugInManager = new PlugInManager();

    sPlugInManager->ProcessEnvironmentSettings();
    sForkHandler->RegisterFileTimer( sFileTimer );


    //--------------------------------------------------------------------------
    // MacOSX library loading is completely moronic. We cannot dlopen a library
    // from a thread other than a main thread, so we-pre dlopen all the
    // libraries that we may potentially want.
    //--------------------------------------------------------------------------
#ifdef __APPLE__
    char *errBuff = new char[1024];

    const char *libs[] =
    {
      "libXrdSeckrb5.dylib",
      "libXrdSecgsi.dylib",
      "libXrdSecgsiAuthzVO.dylib",
      "libXrdSecgsiGMAPDN.dylib",
      "libXrdSecgsiGMAPLDAP.dylib",
      "libXrdSecpwd.dylib",
      "libXrdSecsss.dylib",
      "libXrdSecunix.dylib",
      0
    };

    for( int i = 0; libs[i]; ++i )
    {
      sLog->Debug( UtilityMsg, "Attempting to pre-load: %s", libs[i] );
      bool ok = XrdSysPlugin::Preload( libs[i], errBuff, 1024 );
      if( !ok )
        sLog->Error( UtilityMsg, "Unable to pre-load %s: %s", libs[i], errBuff );
    }
    delete [] errBuff;
#endif
  }

  //----------------------------------------------------------------------------
  // Finalize the environment
  //----------------------------------------------------------------------------
  void DefaultEnv::Finalize()
  {
    if( sPostMaster )
    {
      sPostMaster->Stop();
      sPostMaster->Finalize();
      delete sPostMaster;
      sPostMaster = 0;
    }

    delete sTransportManager;
    sTransportManager = 0;

    delete sCheckSumManager;
    sCheckSumManager = 0;

    delete sMonitor;
    sMonitor = 0;

    delete sMonitorLibHandle;
    sMonitorLibHandle = 0;

    delete sForkHandler;
    sForkHandler = 0;

    delete sFileTimer;
    sFileTimer = 0;

    delete sPlugInManager;
    sPlugInManager = 0;

    delete sEnv;
    sEnv = 0;

    delete sLog;
    sLog = 0;
  }

  //----------------------------------------------------------------------------
  // Re-initialize the logging
  //----------------------------------------------------------------------------
  void DefaultEnv::ReInitializeLogging()
  {
    delete sLog;
    sLog = new Log();
    SetUpLog();
  }

  //----------------------------------------------------------------------------
  // Set up the log
  //----------------------------------------------------------------------------
  void DefaultEnv::SetUpLog()
  {
    Log *log = GetLog();

    //--------------------------------------------------------------------------
    // Check if the log level has been defined in the environment
    //--------------------------------------------------------------------------
    char *level = getenv( "XRD_LOGLEVEL" );
    if( level )
      log->SetLevel( level );

    //--------------------------------------------------------------------------
    // Check if we need to log to a file
    //--------------------------------------------------------------------------
    char *file = getenv( "XRD_LOGFILE" );
    if( file )
    {
      LogOutFile *out = new LogOutFile();
      if( out->Open( file ) )
        log->SetOutput( out );
      else
        delete out;
    }

    //--------------------------------------------------------------------------
    // Log mask defaults
    //--------------------------------------------------------------------------
    MaskTranslator translator;
    log->SetMask( Log::DumpMsg, translator.translateMask( "All|^PollerMsg" ) );

    //--------------------------------------------------------------------------
    // Initialize the topic mask
    //--------------------------------------------------------------------------
    char *logMask = getenv( "XRD_LOGMASK" );
    if( logMask )
    {
      uint64_t mask = translator.translateMask( logMask );
      log->SetMask( Log::ErrorMsg,   mask );
      log->SetMask( Log::WarningMsg, mask );
      log->SetMask( Log::InfoMsg,    mask );
      log->SetMask( Log::DebugMsg,   mask );
      log->SetMask( Log::DumpMsg,    mask );
    }

    logMask = getenv( "XRD_LOGMASK_ERROR" );
    if( logMask ) log->SetMask( Log::ErrorMsg, translator.translateMask( logMask ) );

    logMask = getenv( "XRD_LOGMASK_WARNING" );
    if( logMask ) log->SetMask( Log::WarningMsg, translator.translateMask( logMask ) );

    logMask = getenv( "XRD_LOGMASK_INFO" );
    if( logMask ) log->SetMask( Log::InfoMsg, translator.translateMask( logMask ) );

    logMask = getenv( "XRD_LOGMASK_DEBUG" );
    if( logMask ) log->SetMask( Log::DebugMsg, translator.translateMask( logMask ) );

    logMask = getenv( "XRD_LOGMASK_DUMP" );
    if( logMask ) log->SetMask( Log::DumpMsg, translator.translateMask( logMask ) );

    //--------------------------------------------------------------------------
    // Set up the topic strings
    //--------------------------------------------------------------------------
    log->SetTopicName( AppMsg,             "App" );
    log->SetTopicName( UtilityMsg,         "Utility" );
    log->SetTopicName( FileMsg,            "File" );
    log->SetTopicName( PollerMsg,          "Poller" );
    log->SetTopicName( PostMasterMsg,      "PostMaster" );
    log->SetTopicName( XRootDTransportMsg, "XRootDTransport" );
    log->SetTopicName( TaskMgrMsg,         "TaskMgr" );
    log->SetTopicName( XRootDMsg,          "XRootD" );
    log->SetTopicName( FileSystemMsg,      "FileSystem" );
    log->SetTopicName( AsyncSockMsg,       "AsyncSock" );
    log->SetTopicName( JobMgrMsg,          "JobMgr" );
    log->SetTopicName( PlugInMgrMsg,       "PlugInMgr" );
  }
}

//------------------------------------------------------------------------------
// Forking functions
//------------------------------------------------------------------------------
extern "C"
{
  //----------------------------------------------------------------------------
  // Prepare for the forking
  //----------------------------------------------------------------------------
  static void prepare()
  {
    //--------------------------------------------------------------------------
    // Prepare
    //--------------------------------------------------------------------------
    using namespace XrdCl;
    Log         *log         = DefaultEnv::GetLog();
    Env         *env         = DefaultEnv::GetEnv();
    ForkHandler *forkHandler = DefaultEnv::GetForkHandler();

    log->Debug( UtilityMsg, "In the prepare fork handler for process %d",
                getpid() );

    //--------------------------------------------------------------------------
    // Run the fork handler if it's enabled
    //--------------------------------------------------------------------------
    int runForkHandler = DefaultRunForkHandler;
    env->GetInt( "RunForkHandler", runForkHandler );
    if( runForkHandler )
      forkHandler->Prepare();
    env->WriteLock();
  }

  //----------------------------------------------------------------------------
  // Parent handler
  //----------------------------------------------------------------------------
  static void parent()
  {
    //--------------------------------------------------------------------------
    // Prepare
    //--------------------------------------------------------------------------
    using namespace XrdCl;
    Log         *log         = DefaultEnv::GetLog();
    Env         *env         = DefaultEnv::GetEnv();
    ForkHandler *forkHandler = DefaultEnv::GetForkHandler();
    env->UnLock();

    log->Debug( UtilityMsg, "In the parent fork handler for process %d",
                getpid() );

    //--------------------------------------------------------------------------
    // Run the fork handler if it's enabled
    //--------------------------------------------------------------------------
    int runForkHandler = DefaultRunForkHandler;
    env->GetInt( "RunForkHandler", runForkHandler );
    if( runForkHandler )
      forkHandler->Parent();
  }

  //----------------------------------------------------------------------------
  // Child handler
  //----------------------------------------------------------------------------
  static void child()
  {
    //--------------------------------------------------------------------------
    // Prepare
    //--------------------------------------------------------------------------
    using namespace XrdCl;
    DefaultEnv::ReInitializeLogging();
    Log         *log         = DefaultEnv::GetLog();
    Env         *env         = DefaultEnv::GetEnv();
    ForkHandler *forkHandler = DefaultEnv::GetForkHandler();
    env->ReInitializeLock();

    log->Debug( UtilityMsg, "In the child fork handler for process %d",
                getpid() );

    //--------------------------------------------------------------------------
    // Run the fork handler if it's enabled
    //--------------------------------------------------------------------------
    int runForkHandler = DefaultRunForkHandler;
    env->GetInt( "RunForkHandler", runForkHandler );
    if( runForkHandler )
      forkHandler->Child();
  }
}

//------------------------------------------------------------------------------
// Static initialization and finalization
//------------------------------------------------------------------------------
namespace
{

  static struct EnvInitializer
  {
    //--------------------------------------------------------------------------
    // Initializer
    //--------------------------------------------------------------------------
    EnvInitializer()
    {
      XrdCl::DefaultEnv::Initialize();
      pthread_atfork( prepare, parent, child );
    }

    //--------------------------------------------------------------------------
    // Finalizer
    //--------------------------------------------------------------------------
    ~EnvInitializer()
    {
      XrdCl::DefaultEnv::Finalize();
    }
  } finalizer;
}
