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
#include "XrdCl/XrdClUtils.hh"

#include <map>
#include <iostream>

namespace XrdCl
{
  XrdSysMutex  DefaultEnv::sEnvMutex;
  Env         *DefaultEnv::sEnv             = 0;
  XrdSysMutex  DefaultEnv::sPostMasterMutex;
  PostMaster  *DefaultEnv::sPostMaster      = 0;
  Log         *DefaultEnv::sLog             = 0;

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  DefaultEnv::DefaultEnv()
  {
    PutInt( "ConnectionWindow",     DefaultConnectionWindow  );
    PutInt( "ConnectionRetry",      DefaultConnectionRetry   );
    PutInt( "RequestTimeout",       DefaultRequestTimeout    );
    PutInt( "SubStreamsPerChannel", DefaultSubStreamsPerChannel );
    PutInt( "TimeoutResolution",    DefaultTimeoutResolution );
    PutInt( "StreamErrorWindow",    DefaultStreamErrorWindow );
    PutString( "PollerPreference",  DefaultPollerPreference  );

    ImportInt(    "ConnectionWindow",     "XRD_CONNECTIONWINDOW"     );
    ImportInt(    "ConnectionRetry",      "XRD_CONNECTIONRETRY"      );
    ImportInt(    "RequestTimeout",       "XRD_REQUESTTIMEOUT"       );
    ImportInt(    "SubStreamsPerChannel", "XRD_SUBSTREAMSPERCHANNEL" );
    ImportInt(    "TimeoutResolution",    "XRD_TIMEOUTRESOLUTION"    );
    ImportInt(    "StreamErrorWindow",    "XRD_STREAMERRORWINDOW"    );
    ImportString( "PollerPreference",     "XRD_POLLERPREFERENCE"     );
  }

  //----------------------------------------------------------------------------
  // Get default client environment
  //----------------------------------------------------------------------------
  Env *DefaultEnv::GetEnv()
  {
    if( !sEnv )
    {
      XrdSysMutexHelper scopedLock( sEnvMutex );
      if( sEnv )
        return sEnv;
      sEnv = new DefaultEnv();
    }
    return sEnv;
  }

  //----------------------------------------------------------------------------
  // Get default post master
  //----------------------------------------------------------------------------
  PostMaster *DefaultEnv::GetPostMaster()
  {
    if( !sPostMaster )
    {
      XrdSysMutexHelper scopedLock( sPostMasterMutex );
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
    }
    return sPostMaster;
  }

  Log *DefaultEnv::GetLog()
  {
    //--------------------------------------------------------------------------
    // This is actually thread safe because it is first called from
    // a static initializer in a thread safe context
    //--------------------------------------------------------------------------
    if( unlikely( !sLog ) )
      sLog = new Log();
    return sLog;
  }

  //----------------------------------------------------------------------------
  // Release the environment
  //----------------------------------------------------------------------------
  void DefaultEnv::Release()
  {
    if( sEnv )
    {
      delete sEnv;
      sEnv = 0;
    }

    if( sPostMaster )
    {
      sPostMaster->Stop();
      sPostMaster->Finalize();
      delete sPostMaster;
      sPostMaster = 0;
    }

    delete sLog;
    sLog = 0;
  }
}

//------------------------------------------------------------------------------
// Finalizer
//------------------------------------------------------------------------------
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
      masks["QueryMsg"]           = XrdCl::QueryMsg;
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
        // Check for reseting pseudo topics
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

  static struct EnvInitializer
  {
    //--------------------------------------------------------------------------
    // Initializer
    //--------------------------------------------------------------------------
    EnvInitializer()
    {
      using namespace XrdCl;
      Log *log = DefaultEnv::GetLog();

      //------------------------------------------------------------------------
      // Check if the log level has been defined in the environment
      //------------------------------------------------------------------------
      char *level = getenv( "XRD_LOGLEVEL" );
      if( level )
        log->SetLevel( level );

      //------------------------------------------------------------------------
      // Check if we need to log to a file
      //------------------------------------------------------------------------
      char *file = getenv( "XRD_LOGFILE" );
      if( file )
      {
        LogOutFile *out = new LogOutFile();
        if( out->Open( file ) )
          log->SetOutput( out );
        else
          delete out;
      }

      //------------------------------------------------------------------------
      // Log mask defaults
      //------------------------------------------------------------------------
      MaskTranslator translator;
      log->SetMask( Log::DumpMsg, translator.translateMask( "All|^PollerMsg" ) );

      //------------------------------------------------------------------------
      // Initialize the topic mask
      //------------------------------------------------------------------------
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
    }

    //--------------------------------------------------------------------------
    // Finalizer
    //--------------------------------------------------------------------------
    ~EnvInitializer()
    {
      XrdCl::DefaultEnv::Release();
    }
  } finalizer;
}
