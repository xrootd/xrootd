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

#include <cppunit/extensions/HelperMacros.h>
#include <XrdCl/XrdClFileSystem.hh>
#include <XrdCl/XrdClFile.hh>
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPlugInManager.hh"
#include "CppUnitXrdHelpers.hh"

#include <pthread.h>

#include "TestEnv.hh"
#include "IdentityPlugIn.hh"

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class FileSystemTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( FileSystemTest );
      CPPUNIT_TEST( LocateTest );
      CPPUNIT_TEST( MvTest );
      CPPUNIT_TEST( ServerQueryTest );
      CPPUNIT_TEST( TruncateRmTest );
      CPPUNIT_TEST( MkdirRmdirTest );
      CPPUNIT_TEST( ChmodTest );
      CPPUNIT_TEST( PingTest );
      CPPUNIT_TEST( StatTest );
      CPPUNIT_TEST( StatVFSTest );
      CPPUNIT_TEST( ProtocolTest );
      CPPUNIT_TEST( DeepLocateTest );
      CPPUNIT_TEST( DirListTest );
      CPPUNIT_TEST( SendInfoTest );
      CPPUNIT_TEST( PrepareTest );
      CPPUNIT_TEST( PlugInTest );
    CPPUNIT_TEST_SUITE_END();
    void LocateTest();
    void MvTest();
    void ServerQueryTest();
    void TruncateRmTest();
    void MkdirRmdirTest();
    void ChmodTest();
    void PingTest();
    void StatTest();
    void StatVFSTest();
    void ProtocolTest();
    void DeepLocateTest();
    void DirListTest();
    void SendInfoTest();
    void PrepareTest();
    void PlugInTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( FileSystemTest );

//------------------------------------------------------------------------------
// Locate test
//------------------------------------------------------------------------------
void FileSystemTest::LocateTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string remoteFile;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "RemoteFile", remoteFile ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  FileSystem fs( url );

  LocationInfo *locations = 0;
  CPPUNIT_ASSERT_XRDST( fs.Locate( remoteFile, OpenFlags::Refresh, locations ) );
  CPPUNIT_ASSERT( locations );
  CPPUNIT_ASSERT( locations->GetSize() != 0 );
  delete locations;
}

//------------------------------------------------------------------------------
// Mv test
//------------------------------------------------------------------------------
void FileSystemTest::MvTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;
  std::string remoteFile;

  CPPUNIT_ASSERT( testEnv->GetString( "DiskServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "RemoteFile",    remoteFile ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string filePath1 = remoteFile;
  std::string filePath2 = remoteFile + "2";

  FileSystem fs( url );

  CPPUNIT_ASSERT_XRDST( fs.Mv( filePath1, filePath2 ) );
  CPPUNIT_ASSERT_XRDST( fs.Mv( filePath2, filePath1 ) );

}

//------------------------------------------------------------------------------
// Query test
//------------------------------------------------------------------------------
void FileSystemTest::ServerQueryTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string remoteFile;

  CPPUNIT_ASSERT( testEnv->GetString( "DiskServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "RemoteFile", remoteFile ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  FileSystem fs( url );
  Buffer *response = 0;
  Buffer  arg;
  arg.FromString( remoteFile );
  CPPUNIT_ASSERT_XRDST( fs.Query( QueryCode::Checksum, arg, response ) );
  CPPUNIT_ASSERT( response );
  CPPUNIT_ASSERT( response->GetSize() != 0 );
  delete response;
}

//------------------------------------------------------------------------------
// Truncate/Rm test
//------------------------------------------------------------------------------
void FileSystemTest::TruncateRmTest()
{
  using namespace XrdCl;

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

  std::string filePath = dataPath + "/testfile";
  std::string fileUrl  = address + "/";
  fileUrl += filePath;

  FileSystem fs( url );
  File       f;
  CPPUNIT_ASSERT_XRDST( f.Open( fileUrl, OpenFlags::Update | OpenFlags::Delete,
                                Access::UR | Access::UW ) );
  CPPUNIT_ASSERT_XRDST( fs.Truncate( filePath, 10000000 ) );
  CPPUNIT_ASSERT_XRDST( fs.Rm( filePath ) );
}

//------------------------------------------------------------------------------
// Mkdir/Rmdir test
//------------------------------------------------------------------------------
void FileSystemTest::MkdirRmdirTest()
{
  using namespace XrdCl;

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

  FileSystem fs( url );

  CPPUNIT_ASSERT_XRDST( fs.MkDir( dirPath2, MkDirFlags::MakePath,
                              Access::UR | Access::UW | Access::UX ) );
  CPPUNIT_ASSERT_XRDST( fs.RmDir( dirPath2 ) );
  CPPUNIT_ASSERT_XRDST( fs.RmDir( dirPath1 ) );
}

//------------------------------------------------------------------------------
// Chmod test
//------------------------------------------------------------------------------
void FileSystemTest::ChmodTest()
{
  using namespace XrdCl;

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

  FileSystem fs( url );

  CPPUNIT_ASSERT_XRDST( fs.MkDir( dirPath, MkDirFlags::MakePath,
                                  Access::UR | Access::UW | Access::UX ) );
  CPPUNIT_ASSERT_XRDST( fs.ChMod( dirPath,
                                  Access::UR | Access::UW | Access::UX |
                                  Access::GR | Access::GX ) );
  CPPUNIT_ASSERT_XRDST( fs.RmDir( dirPath ) );
}

//------------------------------------------------------------------------------
// Locate test
//------------------------------------------------------------------------------
void FileSystemTest::PingTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  FileSystem fs( url );
  CPPUNIT_ASSERT_XRDST( fs.Ping() );
}

//------------------------------------------------------------------------------
// Stat test
//------------------------------------------------------------------------------
void FileSystemTest::StatTest()
{
  using namespace XrdCl;

  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string remoteFile;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "RemoteFile",    remoteFile ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  FileSystem fs( url );
  StatInfo *response = 0;
  CPPUNIT_ASSERT_XRDST( fs.Stat( remoteFile, response ) );
  CPPUNIT_ASSERT( response );
  CPPUNIT_ASSERT( response->GetSize() == 1048576000 );
  CPPUNIT_ASSERT( response->TestFlags( StatInfo::IsReadable ) );
  CPPUNIT_ASSERT( response->TestFlags( StatInfo::IsWritable ) );
  CPPUNIT_ASSERT( !response->TestFlags( StatInfo::IsDir ) );
  delete response;
}

//------------------------------------------------------------------------------
// Stat VFS test
//------------------------------------------------------------------------------
void FileSystemTest::StatVFSTest()
{
  using namespace XrdCl;

  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  FileSystem fs( url );
  StatInfoVFS *response = 0;
  CPPUNIT_ASSERT_XRDST( fs.StatVFS( dataPath, response ) );
  CPPUNIT_ASSERT( response );
  delete response;
}

//------------------------------------------------------------------------------
// Protocol test
//------------------------------------------------------------------------------
void FileSystemTest::ProtocolTest()
{
  using namespace XrdCl;

  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  FileSystem fs( url );
  ProtocolInfo *response = 0;
  CPPUNIT_ASSERT_XRDST( fs.Protocol( response ) );
  CPPUNIT_ASSERT( response );
  delete response;
}

//------------------------------------------------------------------------------
// Deep locate test
//------------------------------------------------------------------------------
void FileSystemTest::DeepLocateTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string remoteFile;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "RemoteFile",    remoteFile ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  FileSystem fs( url );

  LocationInfo *locations = 0;
  CPPUNIT_ASSERT_XRDST( fs.DeepLocate( remoteFile, OpenFlags::Refresh, locations ) );
  CPPUNIT_ASSERT( locations );
  CPPUNIT_ASSERT( locations->GetSize() != 0 );
  LocationInfo::Iterator it = locations->Begin();
  for( ; it != locations->End(); ++it )
    CPPUNIT_ASSERT( it->IsServer() );
  delete locations;
}

//------------------------------------------------------------------------------
// Dir list
//------------------------------------------------------------------------------
void FileSystemTest::DirListTest()
{
  using namespace XrdCl;

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

  std::string lsPath = dataPath + "/bigdir";

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  FileSystem fs( url );

  DirectoryList *list = 0;
  CPPUNIT_ASSERT_XRDST( fs.DirList( lsPath, DirListFlags::Stat | DirListFlags::Locate, list ) );
  CPPUNIT_ASSERT( list );
  CPPUNIT_ASSERT( list->GetSize() == 40000 );

  delete list;
}


//------------------------------------------------------------------------------
// Set
//------------------------------------------------------------------------------
void FileSystemTest::SendInfoTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  FileSystem fs( url );

  Buffer *id = 0;
  CPPUNIT_ASSERT_XRDST( fs.SendInfo( "test stuff", id ) );
  CPPUNIT_ASSERT( id );
  CPPUNIT_ASSERT( id->GetSize() == 4 );
  delete id;
}


//------------------------------------------------------------------------------
// Set
//------------------------------------------------------------------------------
void FileSystemTest::PrepareTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  FileSystem fs( url );

  Buffer *id = 0;
  std::vector<std::string> list;
  list.push_back( "/data/1db882c8-8cd6-4df1-941f-ce669bad3458.dat" );
  list.push_back( "/data/1db882c8-8cd6-4df1-941f-ce669bad3458.dat" );

  CPPUNIT_ASSERT_XRDST( fs.Prepare( list, PrepareFlags::Stage, 1, id ) );
  CPPUNIT_ASSERT( id );
  CPPUNIT_ASSERT( id->GetSize() );
  delete id;
}

//------------------------------------------------------------------------------
// Plug-in test
//------------------------------------------------------------------------------
void FileSystemTest::PlugInTest()
{
  XrdCl::PlugInFactory *f = new IdentityFactory;
  XrdCl::DefaultEnv::GetPlugInManager()->RegisterDefaultFactory(f);
  LocateTest();
  MvTest();
  ServerQueryTest();
  TruncateRmTest();
  MkdirRmdirTest();
  ChmodTest();
  PingTest();
  StatTest();
  StatVFSTest();
  ProtocolTest();
  DeepLocateTest();
  DirListTest();
  SendInfoTest();
  PrepareTest();
  XrdCl::DefaultEnv::GetPlugInManager()->RegisterDefaultFactory(0);
}
