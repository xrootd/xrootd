//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __TEST_ENV_HH__
#define __TEST_ENV_HH__

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClEnv.hh"
#include "XrdCl/XrdClLog.hh"

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
    //! Get default test environment
    //--------------------------------------------------------------------------
    static XrdClient::Env *GetEnv();

    //--------------------------------------------------------------------------
    //! Get default test environment
    //--------------------------------------------------------------------------
    static XrdClient::Log *GetLog();

    //--------------------------------------------------------------------------
    //! Release the environment
    //--------------------------------------------------------------------------
    static void Release();

  private:
    static XrdSysMutex     sEnvMutex;
    static XrdClient::Env *sEnv;
    static XrdClient::Log *sLog;
};

#endif // __TEST_ENV_HH__
