//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_DEFAULT_ENV_HH__
#define __XRD_CL_DEFAULT_ENV_HH__

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClEnv.hh"

namespace XrdCl
{
  class PostMaster;
  class Log;

  //----------------------------------------------------------------------------
  //! Default environment for the client. Responsible for setting/importing
  //! defaults for the variables used by the client. And holding other
  //! global stuff.
  //----------------------------------------------------------------------------
  class DefaultEnv: public Env
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      DefaultEnv();

      //------------------------------------------------------------------------
      //! Get default client environment
      //------------------------------------------------------------------------
      static Env *GetEnv();

      //------------------------------------------------------------------------
      //! Get default post master
      //------------------------------------------------------------------------
      static PostMaster *GetPostMaster();

      //------------------------------------------------------------------------
      //! Get default log
      //------------------------------------------------------------------------
      static Log *GetLog();

      //------------------------------------------------------------------------
      //! Free the globals, called by a static finalizer, no need to call
      //! by hand
      //------------------------------------------------------------------------
      static void Release();

    private:
      static XrdSysMutex  sEnvMutex;
      static Env         *sEnv;
      static XrdSysMutex  sPostMasterMutex;
      static PostMaster  *sPostMaster;
      static Log         *sLog;
  };
}

#endif // __XRD_CL_DEFAULT_ENV_HH__
