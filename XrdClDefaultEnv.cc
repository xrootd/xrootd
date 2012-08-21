//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"

#include <map>

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
      masks["AppMsg"]     = XrdCl::AppMsg;
      masks["UtilityMsg"] = XrdCl::UtilityMsg;
      masks["FileMsg"]    = XrdCl::FileMsg;
    }

    //--------------------------------------------------------------------------
    // Translate the mask
    //--------------------------------------------------------------------------
    uint64_t translateMask( const std::string mask )
    {
      if( mask == "" || mask == "All" )
        return 0xffffffffffffffff;

      if( mask == "None" )
        return 0;

      std::vector<std::string>           topics;
      std::vector<std::string>::iterator it;
      XrdCl::Utils::splitString( topics, mask, "|" );

      uint64_t resultMask = 0;
      std::map<std::string, uint64_t>::iterator maskIt;
      for( it = topics.begin(); it != topics.end(); ++it )
      {
        maskIt = masks.find( *it );
        if( maskIt != masks.end() )
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
      // Initialize the topic mask
      //------------------------------------------------------------------------
      char *logMask = getenv( "XRD_LOGMASK" );
      if( logMask )
      {
        MaskTranslator translator;
        log->SetMask( translator.translateMask( logMask ) );
      }
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
