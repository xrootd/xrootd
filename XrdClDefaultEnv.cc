//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"

namespace XrdClient
{
  XrdSysMutex  DefaultEnv::sMutex;
  Env         *DefaultEnv::sEnv;

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
      XrdSysMutexHelper scopedLock( sMutex );
      if( sEnv )
        return sEnv;
      sEnv = new DefaultEnv();
      return sEnv;
    }
    return sEnv;
  }
}
