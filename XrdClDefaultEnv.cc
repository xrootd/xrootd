//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
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
    PutInt( "ConnectionTimeout", DefaultConnectionTimeout );
    PutInt( "DataServerTimeout", DefaultDataServerTimeout );
    PutInt( "ManagerTimeout",    DefaultManagerTimeout    );
  }

  //----------------------------------------------------------------------------
  // Get default client environment
  //----------------------------------------------------------------------------
  Env *DefaultEnv::GetDefaultEnv()
  {
    if( !sEnv )
    {
      XrdSysMutexHelper( sMutex );
      if( sEnv )
        return sEnv;
      sEnv = new DefaultEnv();
      return sEnv;
    }
    return sEnv;
  }
}
