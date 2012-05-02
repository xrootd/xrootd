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
class TestEnv: public XrdCl::Env
{
  public:
    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    TestEnv();

    //--------------------------------------------------------------------------
    //! Get default test environment
    //--------------------------------------------------------------------------
    static XrdCl::Env *GetEnv();

    //--------------------------------------------------------------------------
    //! Get default test environment
    //--------------------------------------------------------------------------
    static XrdCl::Log *GetLog();

    //--------------------------------------------------------------------------
    //! Release the environment
    //--------------------------------------------------------------------------
    static void Release();

  private:
    static XrdSysMutex     sEnvMutex;
    static XrdCl::Env *sEnv;
    static XrdCl::Log *sLog;
};

#endif // __TEST_ENV_HH__
