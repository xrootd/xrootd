//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "TestEnv.hh"

XrdSysMutex     TestEnv::sEnvMutex;
XrdClient::Env *TestEnv::sEnv             = 0;

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
XrdClient::Env *TestEnv::GetEnv()
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

//------------------------------------------------------------------------------
// Release the environment
//------------------------------------------------------------------------------
void TestEnv::Release()
{
  if( sEnv )
  {
    delete sEnv;
    sEnv = 0;
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
    // Finalizer
    //--------------------------------------------------------------------------
    ~EnvInitializer()
    {
      TestEnv::Release();
    }
  } finalizer;
}
