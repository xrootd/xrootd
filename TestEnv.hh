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

#ifndef __TEST_ENV_HH__
#define __TEST_ENV_HH__

#include "XrdSys/XrdSysPthread.hh"
#include "XrdCl/XrdClEnv.hh"
#include "XrdCl/XrdClLog.hh"

namespace XrdClTests {

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

}

#endif // __TEST_ENV_HH__
