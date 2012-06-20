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

#include "TestEnv.hh"

XrdSysMutex     TestEnv::sEnvMutex;
XrdCl::Env *TestEnv::sEnv       = 0;
XrdCl::Log *TestEnv::sLog       = 0;

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
TestEnv::TestEnv()
{
  PutString( "MainServerURL",    "localhost:1094" );
  PutString( "DiskServerURL",    "localhost:1094" );
  PutString( "DataPath",         "/data"         );
  PutString( "LocalFile",        "/data/testFile.dat" );
  ImportString( "MainServerURL", "XRDTEST_MAINSERVERURL" );
  ImportString( "DiskServerURL", "XRDTEST_DISKSERVERURL" );
  ImportString( "DataPath",      "XRDTEST_DATAPATH" );
  ImportString( "LocalFile",     "XRDTEST_LOCALFILE" );
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
  delete sLog;
  sLog = 0;
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
      Log *log = TestEnv::GetLog();
      char *level = getenv( "XRDTEST_LOGLEVEL" );
      if( level )
        log->SetLevel( level );
    }

    //--------------------------------------------------------------------------
    // Finalizer
    //--------------------------------------------------------------------------
    ~EnvInitializer()
    {
      TestEnv::Release();
    }
  } initializer;
}
