//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __TEST_ENV_HH__
#define __TEST_ENV_HH__

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClEnv.hh"

//------------------------------------------------------------------------------
//! Envornment holding the variables for tests
//------------------------------------------------------------------------------
class TestEnv: public XrdClient::Env
{
  public:
    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    TestEnv();

    //--------------------------------------------------------------------------
    //! Get default client environment
    //--------------------------------------------------------------------------
    static XrdClient::Env *GetEnv();

    //--------------------------------------------------------------------------
    //! Release the environment
    //--------------------------------------------------------------------------
    static void Release();

  private:
    static XrdSysMutex     sEnvMutex;
    static XrdClient::Env *sEnv;
};

#endif // __TEST_ENV_HH__
