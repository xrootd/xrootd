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
#include "XrdSys/XrdSysPwd.hh"

#include <libgen.h>
#include <cstring>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <string>
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

  //----------------------------------------------------------------------------
  // Helper for handling environment variables
  //----------------------------------------------------------------------------
  template<typename Item>
  struct EnvVarHolder
  {
    EnvVarHolder( const std::string &name_, const Item &def_ ):
      name( name_ ), def( def_ ) {}
    std::string name;
    Item        def;
  };
}

#define REGISTER_VAR_INT( array, name,  def ) \
    array.push_back( EnvVarHolder<int>( name, def ) )

#define REGISTER_VAR_STR( array, name,  def ) \
    array.push_back( EnvVarHolder<std::string>( name, def ) )

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
    Log *log = GetLog();

    //--------------------------------------------------------------------------
    // Declate the variables to be processed
    //--------------------------------------------------------------------------
    std::vector<EnvVarHolder<int> >         varsInt;
    std::vector<EnvVarHolder<std::string> > varsStr;
    REGISTER_VAR_INT( varsInt, "ConnectionWindow",     DefaultConnectionWindow     );
    REGISTER_VAR_INT( varsInt, "ConnectionRetry",      DefaultConnectionRetry      );
    REGISTER_VAR_INT( varsInt, "RequestTimeout",       DefaultRequestTimeout       );
    REGISTER_VAR_INT( varsInt, "StreamTimeout",        DefaultStreamTimeout        );
    REGISTER_VAR_INT( varsInt, "SubStreamsPerChannel", DefaultSubStreamsPerChannel );
    REGISTER_VAR_INT( varsInt, "TimeoutResolution",    DefaultTimeoutResolution    );
    REGISTER_VAR_INT( varsInt, "StreamErrorWindow",    DefaultStreamErrorWindow    );
    REGISTER_VAR_INT( varsInt, "RunForkHandler",       DefaultRunForkHandler       );
    REGISTER_VAR_INT( varsInt, "RedirectLimit",        DefaultRedirectLimit        );
    REGISTER_VAR_INT( varsInt, "WorkerThreads",        DefaultWorkerThreads        );
    REGISTER_VAR_INT( varsInt, "CPChunkSize",          DefaultCPChunkSize          );
    REGISTER_VAR_INT( varsInt, "CPParallelChunks",     DefaultCPParallelChunks     );
    REGISTER_VAR_INT( varsInt, "DataServerTTL",        DefaultDataServerTTL        );
    REGISTER_VAR_INT( varsInt, "LoadBalancerTTL",      DefaultLoadBalancerTTL      );
    REGISTER_VAR_INT( varsInt, "CPInitTimeout",        DefaultCPInitTimeout        );
    REGISTER_VAR_INT( varsInt, "CPTPCTimeout",         DefaultCPTPCTimeout         );

    REGISTER_VAR_STR( varsStr, "PollerPreference",     DefaultPollerPreference     );
    REGISTER_VAR_STR( varsStr, "ClientMonitor",        DefaultClientMonitor        );
    REGISTER_VAR_STR( varsStr, "ClientMonitorParam",   DefaultClientMonitorParam   );
    REGISTER_VAR_STR( varsStr, "NetworkStack",         DefaultNetworkStack         );
    REGISTER_VAR_STR( varsStr, "PlugIn",               DefaultPlugIn               );
    REGISTER_VAR_STR( varsStr, "PlugInConfDir",        DefaultPlugInConfDir        );

    //--------------------------------------------------------------------------
    // Process the configuration files
    //--------------------------------------------------------------------------
    std::map<std::string, std::string> config, userConfig;
    Status st = Utils::ProcessConfig( config, "/etc/xrootd/client.conf" );

    if( !st.IsOK() )
      log->Warning( UtilityMsg, "Unable to process global config file: %s",
                    st.ToString().c_str() );

    XrdSysPwd pwdHandler;
    passwd *pwd = pwdHandler.Get( getuid() );
    std::string userConfigFile = pwd->pw_dir;
    userConfigFile += "/.xrootd/client.conf";

    st = Utils::ProcessConfig( userConfig, userConfigFile );

    if( !st.IsOK() )
      log->Debug( UtilityMsg, "Unable to process user config file: %s",
                  st.ToString().c_str() );

    std::map<std::string, std::string>::iterator it;

    for( it = config.begin(); it != config.end(); ++it )
      log->Dump( UtilityMsg, "[Global config] \"%s\" = \"%s\"",
                 it->first.c_str(), it->second.c_str() );

    for( it = userConfig.begin(); it != userConfig.end(); ++it )
    {
      config[it->first] = it->second;
      log->Dump( UtilityMsg, "[User config] \"%s\" = \"%s\"",
                 it->first.c_str(), it->second.c_str() );
    }

    for( it = config.begin(); it != config.end(); ++it )
      log->Debug( UtilityMsg, "[Effective config] \"%s\" = \"%s\"",
                  it->first.c_str(), it->second.c_str() );

    //--------------------------------------------------------------------------
    // Monitoring settings
    //--------------------------------------------------------------------------
    char *tmp = strdup( XrdSysUtils::ExecName() );
    char *appName = basename( tmp );
    PutString( "AppName", appName );
    free( tmp );
    ImportString( "AppName", "XRD_APPNAME" );
    PutString( "MonInfo", "" );
    ImportString( "MonInfo", "XRD_MONINFO" );

    //--------------------------------------------------------------------------
    // Process ints
    //--------------------------------------------------------------------------
    for( size_t i = 0; i < varsInt.size(); ++i )
    {
      PutInt( varsInt[i].name, varsInt[i].def );

      it = config.find( varsInt[i].name );
      if( it != config.end() )
      {
        char *endPtr = 0;
        int value = (int)strtol( it->second.c_str(), &endPtr, 0 );
        if( *endPtr )
          log->Warning( UtilityMsg, "Unable to set %s to %s: not a proper "
                        "integer", varsInt[i].name.c_str(),
                        it->second.c_str() );
        else
          PutInt( varsInt[i].name, value );
      }

      std::string name = "XRD_" + varsInt[i].name;
      std::transform( name.begin(), name.end(), name.begin(), ::toupper );
      ImportInt( varsInt[i].name, name );
    }

    //--------------------------------------------------------------------------
    // Process strings
    //--------------------------------------------------------------------------
    for( size_t i = 0; i < varsStr.size(); ++i )
    {
      PutString( varsStr[i].name, varsStr[i].def );

      it = config.find( varsStr[i].name );
      if( it != config.end() )
        PutString( varsStr[i].name, it->second );

      std::string name = "XRD_" + varsStr[i].name;
      std::transform( name.begin(), name.end(), name.begin(), ::toupper );
      ImportString( varsStr[i].name, name );
    }
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
  // Retrieve the plug-in factory for the given URL
  //----------------------------------------------------------------------------
  PlugInFactory *DefaultEnv::GetPlugInFactory( const std::string url )
  {
    return  sPlugInManager->GetFactory( url );
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
