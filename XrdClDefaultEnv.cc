//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClPostMaster.hh"

namespace XrdClient
{
  XrdSysMutex  DefaultEnv::sEnvMutex;
  Env         *DefaultEnv::sEnv             = 0;
  XrdSysMutex  DefaultEnv::sPostMasterMutex;
  PostMaster  *DefaultEnv::sPostMaster      = 0;

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  DefaultEnv::DefaultEnv()
  {
    PutInt( "ConnectionWindow",  DefaultConnectionWindow  );
    PutInt( "ConnectionRetry",   DefaultConnectionRetry   );
    PutInt( "RequestTimeout",    DefaultRequestTimeout    );
    PutInt( "DataServerTTL",     DefaultDataServerTTL     );
    PutInt( "ManagerTTL",        DefaultManagerTTL        );
    PutInt( "StreamsPerChannel", DefaultStreamsPerChannel );
    PutInt( "TimeoutResolution", DefaultTimeoutResolution );
    PutInt( "StreamErrorWindow", DefaultStreamErrorWindow );
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
      }
    }
    return sPostMaster;
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
      sPostMaster->Finalize();
      sPostMaster->Stop();
      delete sPostMaster;
      sPostMaster = 0;
    }
  }
}

//------------------------------------------------------------------------------
// Finalizer
//------------------------------------------------------------------------------
namespace
{
  static struct EnvFinalizer
  {
    ~EnvFinalizer()
    {
      XrdClient::DefaultEnv::Release();
    }
  } finalizer;
}
