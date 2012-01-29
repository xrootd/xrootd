//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include <cppunit/extensions/HelperMacros.h>
#include <XrdCl/XrdClQuery.hh>

#include <pthread.h>

#include "TestEnv.hh"

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class QueryTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( QueryTest );
      CPPUNIT_TEST( LocateTest );
      CPPUNIT_TEST( MvTest );
      CPPUNIT_TEST( ServerQueryTest );
      CPPUNIT_TEST( TruncateRmTest );
      CPPUNIT_TEST( MkdirRmdirTest );
      CPPUNIT_TEST( ChmodTest );
    CPPUNIT_TEST_SUITE_END();
    void LocateTest();
    void MvTest();
    void ServerQueryTest();
    void TruncateRmTest();
    void MkdirRmdirTest();
    void ChmodTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( QueryTest );

//------------------------------------------------------------------------------
// Locate test
//------------------------------------------------------------------------------
void QueryTest::LocateTest()
{
  using namespace XrdClient;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string filePath = dataPath + "/89120cec-5244-444c-9313-703e4bee72de.dat";

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  Query query( url );

  LocationInfo *locations = 0;
  XRootDStatus st = query.Locate( filePath, OpenFlags::Refresh, locations );
  CPPUNIT_ASSERT( st.IsOK() );
  CPPUNIT_ASSERT( locations );
  CPPUNIT_ASSERT( locations->GetSize() != 0 );
}

//------------------------------------------------------------------------------
// Mv test
//------------------------------------------------------------------------------
void QueryTest::MvTest()
{
  using namespace XrdClient;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "DiskServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string filePath1 = dataPath + "/89120cec-5244-444c-9313-703e4bee72de.dat";
  std::string filePath2 = dataPath + "/89120cec-5244-444c-9313-703e4bee72de.dat2";

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  Query query( url );

  XRootDStatus st = query.Mv( filePath1, filePath2 );
  CPPUNIT_ASSERT( st.IsOK() );
  st = query.Mv( filePath2, filePath1 );
  CPPUNIT_ASSERT( st.IsOK() );
}

//------------------------------------------------------------------------------
// Query test
//------------------------------------------------------------------------------
void QueryTest::ServerQueryTest()
{
  using namespace XrdClient;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string filePath = dataPath + "/89120cec-5244-444c-9313-703e4bee72de.dat";

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  Query query( url );
  Buffer *response = 0;
  Buffer  arg;
  arg.FromString( filePath );
  XRootDStatus st = query.ServerQuery( QueryCode::Checksum, arg, response );
  CPPUNIT_ASSERT( st.IsOK() );
  CPPUNIT_ASSERT( response );
  CPPUNIT_ASSERT( response->GetSize() != 0 );
  delete response;
}

//------------------------------------------------------------------------------
// Truncate/Rm test
//------------------------------------------------------------------------------
void QueryTest::TruncateRmTest()
{
  using namespace XrdClient;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "DiskServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string filePath = dataPath + "/testfile";

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  Query query( url );

  XRootDStatus st = query.Truncate( filePath, 10000000 );
  CPPUNIT_ASSERT( st.IsOK() );
  st = query.Rm( filePath );
  CPPUNIT_ASSERT( st.IsOK() );
}

//------------------------------------------------------------------------------
// Mkdir/Rmdir test
//------------------------------------------------------------------------------
void QueryTest::MkdirRmdirTest()
{
  using namespace XrdClient;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "DiskServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string dirPath1 = dataPath + "/testdir";
  std::string dirPath2 = dataPath + "/testdir/asdads";

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  Query query( url );

  XRootDStatus st = query.MkDir( dirPath2, MkDirFlags::MakePath,
                          AccessMode::UR | AccessMode::UW | AccessMode::UX );
  CPPUNIT_ASSERT( st.IsOK() );
  st = query.RmDir( dirPath2 );
  CPPUNIT_ASSERT( st.IsOK() );
  st = query.RmDir( dirPath1 );
  CPPUNIT_ASSERT( st.IsOK() );
}

//------------------------------------------------------------------------------
// Chmod test
//------------------------------------------------------------------------------
void QueryTest::ChmodTest()
{
  using namespace XrdClient;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "DiskServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string dirPath = dataPath + "/testdir";

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  Query query( url );

  XRootDStatus st = query.MkDir( dirPath, MkDirFlags::MakePath,
                          AccessMode::UR | AccessMode::UW | AccessMode::UX );
  CPPUNIT_ASSERT( st.IsOK() );
  st = query.ChMod( dirPath,
                    AccessMode::UR | AccessMode::UW | AccessMode::UX |
                    AccessMode::GR | AccessMode::GX );
  CPPUNIT_ASSERT( st.IsOK() );
  st = query.RmDir( dirPath );
  CPPUNIT_ASSERT( st.IsOK() );
}
