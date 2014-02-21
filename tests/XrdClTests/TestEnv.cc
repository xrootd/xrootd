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

#include "TestEnv.hh"

namespace XrdClTests {

XrdSysMutex     TestEnv::sEnvMutex;
XrdCl::Env *TestEnv::sEnv       = 0;
XrdCl::Log *TestEnv::sLog       = 0;

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
TestEnv::TestEnv()
{
  PutString( "MainServerURL",    "localhost:1094" );
  PutString( "Manager1URL",      "localhost:1094" );
  PutString( "Manager2URL",      "localhost:1094" );
  PutString( "DiskServerURL",    "localhost:1094" );
  PutString( "DataPath",         "/data"         );
  PutString( "RemoteFile",       "/data/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat" );
  PutString( "LocalFile",        "/data/testFile.dat" );
  PutString( "MultiIPServerURL", "multiip:1099" );

  ImportString( "MainServerURL",    "XRDTEST_MAINSERVERURL" );
  ImportString( "DiskServerURL",    "XRDTEST_DISKSERVERURL" );
  ImportString( "Manager1URL",      "XRDTEST_MANAGER1URL" );
  ImportString( "Manager2URL",      "XRDTEST_MANAGER2URL" );
  ImportString( "DataPath",         "XRDTEST_DATAPATH" );
  ImportString( "LocalFile",        "XRDTEST_LOCALFILE" );
  ImportString( "RemoteFile",       "XRDTEST_REMOTEFILE" );
  ImportString( "MultiIPServerURL", "XRDTEST_MULTIIPSERVERURL" );
}

//------------------------------------------------------------------------------
// Get default client environment
//------------------------------------------------------------------------------
XrdCl::Env *TestEnv::GetEnv()
{
  if( !sEnv )
  {
    XrdSysMutexHelper scopedLock( sEnvMutex );
    if( sEnv )
      return sEnv;
    sEnv = new TestEnv();
  }
  return sEnv;
}

XrdCl::Log *TestEnv::GetLog()
{
  //----------------------------------------------------------------------------
  // This is actually thread safe because it is first called from
  // a static initializer in a thread safe context
  //----------------------------------------------------------------------------
  if( unlikely( !sLog ) )
    sLog = new XrdCl::Log();
  return sLog;
}

//------------------------------------------------------------------------------
// Release the environment
//------------------------------------------------------------------------------
void TestEnv::Release()
{
  delete sEnv;
  sEnv = 0;
//  delete sLog;
//  sLog = 0;
}
}

//------------------------------------------------------------------------------
// Finalizer
//------------------------------------------------------------------------------
namespace
{
  static struct EnvInitializer
  {
    //--------------------------------------------------------------------------
    // Initializer
    //--------------------------------------------------------------------------
    EnvInitializer()
    {
      using namespace XrdCl;
      Log *log = XrdClTests::TestEnv::GetLog();
      char *level = getenv( "XRDTEST_LOGLEVEL" );
      if( level )
        log->SetLevel( level );
    }

    //--------------------------------------------------------------------------
    // Finalizer
    //--------------------------------------------------------------------------
    ~EnvInitializer()
    {
      XrdClTests::TestEnv::Release();
    }
  } initializer;
}
