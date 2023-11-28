//------------------------------------------------------------------------------
// Copyright (c) 2023 by European Organization for Nuclear Research (CERN)
// Author: Angelo Galavotti <agalavottib@gmail.com>
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

#include <XrdCl/XrdClFileSystem.hh>
#include <XrdCl/XrdClFile.hh>
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPlugInManager.hh"
#include "GTestXrdHelpers.hh"

#include <pthread.h>
#include <sys/stat.h>
#include "TestEnv.hh"
#include "IdentityPlugIn.hh"

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Class declaration
//------------------------------------------------------------------------------
class FileSystemTest: public ::testing::Test
{
  public:
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
    void XAttrTest();
};

//------------------------------------------------------------------------------
// Tests declaration
//------------------------------------------------------------------------------
TEST_F(FileSystemTest, LocateTest)
{
  LocateTest();
}

TEST_F(FileSystemTest, MvTest)
{
  MvTest();
}

TEST_F(FileSystemTest, ServerQueryTest)
{
  ServerQueryTest();
}

TEST_F(FileSystemTest, TruncateRmTest)
{
  TruncateRmTest();
}

TEST_F(FileSystemTest, MkdirRmdirTest)
{
  MkdirRmdirTest();
}

TEST_F(FileSystemTest, ChmodTest)
{
  ChmodTest();
}

TEST_F(FileSystemTest, PingTest)
{
  PingTest();
}

TEST_F(FileSystemTest, StatTest)
{
  StatTest();
}

TEST_F(FileSystemTest, StatVFSTest)
{
  StatVFSTest();
}

TEST_F(FileSystemTest, ProtocolTest)
{
  ProtocolTest();
}

TEST_F(FileSystemTest, DeepLocateTest)
{
  DeepLocateTest();
}

TEST_F(FileSystemTest, DirListTest)
{
  DirListTest();
}

TEST_F(FileSystemTest, SendInfoTest)
{
  SendInfoTest();
}

TEST_F(FileSystemTest, PrepareTest)
{
  PrepareTest();
}

TEST_F(FileSystemTest, XAttrTest)
{
  XAttrTest();
}

TEST_F(FileSystemTest, PlugInTest)
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

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "RemoteFile", remoteFile ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  FileSystem fs( url );

  LocationInfo *locations = 0;
  GTEST_ASSERT_XRDST( fs.Locate( remoteFile, OpenFlags::Refresh, locations ) );
  EXPECT_TRUE( locations );
  EXPECT_NE( locations->GetSize(), 0 );
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

  EXPECT_TRUE( testEnv->GetString( "DiskServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "RemoteFile",    remoteFile ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string filePath1 = remoteFile;
  std::string filePath2 = remoteFile + "2";


  LocationInfo *info = 0;
  FileSystem fs( url );

  // move the file
  GTEST_ASSERT_XRDST( fs.Mv( filePath1, filePath2 ) );
  // make sure it's not there
  GTEST_ASSERT_XRDST_NOTOK( fs.Locate( filePath1, OpenFlags::Refresh, info ),
                              errErrorResponse );
  // make sure the destination is there
  GTEST_ASSERT_XRDST( fs.Locate( filePath2, OpenFlags::Refresh, info ) );
  delete info;
  // move it back
  GTEST_ASSERT_XRDST( fs.Mv( filePath2, filePath1 ) );
  // make sure it's there
  GTEST_ASSERT_XRDST( fs.Locate( filePath1, OpenFlags::Refresh, info ) );
  delete info;
  // make sure the other one is gone
  GTEST_ASSERT_XRDST_NOTOK( fs.Locate( filePath2, OpenFlags::Refresh, info ),
                              errErrorResponse );
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

  EXPECT_TRUE( testEnv->GetString( "DiskServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "RemoteFile", remoteFile ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  FileSystem fs( url );
  Buffer *response = 0;
  Buffer  arg;
  arg.FromString( remoteFile );
  GTEST_ASSERT_XRDST( fs.Query( QueryCode::Checksum, arg, response ) );
  EXPECT_TRUE( response );
  EXPECT_NE( response->GetSize(), 0 );
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

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string filePath = dataPath + "/testfile";
  std::string fileUrl  = address + "/";
  fileUrl += filePath;

  FileSystem fs( url );
  File       f;
  GTEST_ASSERT_XRDST( f.Open( fileUrl, OpenFlags::Update | OpenFlags::Delete,
                                Access::UR | Access::UW ) );
  GTEST_ASSERT_XRDST( fs.Truncate( filePath, 10000000 ) );
  GTEST_ASSERT_XRDST( fs.Rm( filePath ) );
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

  EXPECT_TRUE( testEnv->GetString( "DiskServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string dirPath1 = dataPath + "/testdir";
  std::string dirPath2 = dataPath + "/testdir/asdads";

  FileSystem fs( url );

  GTEST_ASSERT_XRDST( fs.MkDir( dirPath2, MkDirFlags::MakePath,
                              Access::UR | Access::UW | Access::UX ) );
  GTEST_ASSERT_XRDST( fs.RmDir( dirPath2 ) );
  GTEST_ASSERT_XRDST( fs.RmDir( dirPath1 ) );
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

  EXPECT_TRUE( testEnv->GetString( "DiskServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string dirPath = dataPath + "/testdir";

  FileSystem fs( url );

  GTEST_ASSERT_XRDST( fs.MkDir( dirPath, MkDirFlags::MakePath,
                                  Access::UR | Access::UW | Access::UX ) );
  GTEST_ASSERT_XRDST( fs.ChMod( dirPath,
                                  Access::UR | Access::UW | Access::UX |
                                  Access::GR | Access::GX ) );
  GTEST_ASSERT_XRDST( fs.RmDir( dirPath ) );
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
  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  FileSystem fs( url );
  GTEST_ASSERT_XRDST( fs.Ping() );
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
  std::string localDataPath;

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "RemoteFile",    remoteFile ) );
  EXPECT_TRUE( testEnv->GetString( "LocalDataPath", localDataPath ) );

  std::string localFilePath = localDataPath + "/srv1" + remoteFile;

  struct stat localStatBuf;
  int rc = stat(localFilePath.c_str(), &localStatBuf);
  EXPECT_EQ( rc, 0 );
  uint64_t fileSize = localStatBuf.st_size;

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  FileSystem fs( url );
  StatInfo *response = 0;
  GTEST_ASSERT_XRDST( fs.Stat( remoteFile, response ) );
  EXPECT_TRUE( response );
  EXPECT_EQ( response->GetSize(), fileSize );
  EXPECT_TRUE( response->TestFlags( StatInfo::IsReadable ) );
  EXPECT_TRUE( response->TestFlags( StatInfo::IsWritable ) );
  EXPECT_TRUE( !response->TestFlags( StatInfo::IsDir ) );
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

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  FileSystem fs( url );
  StatInfoVFS *response = 0;
  GTEST_ASSERT_XRDST( fs.StatVFS( dataPath, response ) );
  EXPECT_TRUE( response );
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
  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  FileSystem fs( url );
  ProtocolInfo *response = 0;
  GTEST_ASSERT_XRDST( fs.Protocol( response ) );
  EXPECT_TRUE( response );
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

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "RemoteFile",    remoteFile ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  FileSystem fs( url );

  LocationInfo *locations = 0;
  GTEST_ASSERT_XRDST( fs.DeepLocate( remoteFile, OpenFlags::Refresh, locations ) );
  EXPECT_TRUE( locations );
  EXPECT_NE( locations->GetSize(), 0 );
  LocationInfo::Iterator it = locations->Begin();
  for( ; it != locations->End(); ++it )
    EXPECT_TRUE( it->IsServer() );
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

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string lsPath = dataPath + "/bigdir";

  //----------------------------------------------------------------------------
  // Query the server for all of the file locations
  //----------------------------------------------------------------------------
  FileSystem fs( url );

  DirectoryList *list = 0;
  GTEST_ASSERT_XRDST( fs.DirList( lsPath, DirListFlags::Stat | DirListFlags::Locate, list ) );
  EXPECT_TRUE( list );
  EXPECT_EQ( list->GetSize(), 4000 );

  std::set<std::string> dirls1;
  for( auto itr = list->Begin(); itr != list->End(); ++itr )
  {
    DirectoryList::ListEntry *entry = *itr;
    dirls1.insert( entry->GetName() );
  }

  delete list;
  list = 0;

  //----------------------------------------------------------------------------
  // Now do a chunked query
  //----------------------------------------------------------------------------
  std::set<std::string> dirls2;

  LocationInfo *info = 0;
  GTEST_ASSERT_XRDST( fs.DeepLocate( lsPath, OpenFlags::PrefName, info ) );
  EXPECT_TRUE( info );

  for( auto itr = info->Begin(); itr != info->End(); ++itr )
  {
    XrdSysSemaphore sem( 0 );
    auto handler = XrdCl::ResponseHandler::Wrap( [&]( auto &s, auto &r )
      {
        GTEST_ASSERT_XRDST( s );
        auto &list = To<XrdCl::DirectoryList>( r );
        for( auto itr = list.Begin(); itr != list.End(); ++itr )
          dirls2.insert( ( *itr )->GetName() );
        if( s.code == XrdCl::suDone )
          sem.Post();
      } );

    FileSystem fs1( std::string( itr->GetAddress() ) );
    GTEST_ASSERT_XRDST( fs1.DirList( lsPath, DirListFlags::Stat | DirListFlags::Chunked, handler ) );
    sem.Wait();
  }
  delete info;
  info = 0;

  EXPECT_EQ( dirls1, dirls2 );

  //----------------------------------------------------------------------------
  // Now list an empty directory
  //----------------------------------------------------------------------------
  std::string emptyDirPath = dataPath + "/empty" ;
  GTEST_ASSERT_XRDST( fs.MkDir( emptyDirPath, MkDirFlags::None, Access::None ) );
  GTEST_ASSERT_XRDST( fs.DeepLocate( emptyDirPath, OpenFlags::PrefName, info ) );
  EXPECT_TRUE( info->GetSize() );
  FileSystem fs3( info->Begin()->GetAddress() );
  GTEST_ASSERT_XRDST( fs3.DirList( emptyDirPath, DirListFlags::Stat, list ) );
  EXPECT_TRUE( list );
  EXPECT_EQ( list->GetSize(), 0 );
  GTEST_ASSERT_XRDST( fs.RmDir( emptyDirPath ) );

  delete list;
  list = 0;
  delete info;
  info = 0;
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

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  FileSystem fs( url );

  Buffer *id = 0;
  GTEST_ASSERT_XRDST( fs.SendInfo( "test stuff", id ) );
  EXPECT_TRUE( id );
  EXPECT_EQ( id->GetSize(), 4 );
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

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );
  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  FileSystem fs( url );

  Buffer *id = 0;
  std::vector<std::string> list;

  std::string fileLocation = dataPath + "/1db882c8-8cd6-4df1-941f-ce669bad3458.dat";
  list.push_back( fileLocation );
  list.push_back( fileLocation );

  GTEST_ASSERT_XRDST( fs.Prepare( list, PrepareFlags::Stage, 1, id ) );
  EXPECT_TRUE( id );
  EXPECT_TRUE( id->GetSize() );
  delete id;
}

//------------------------------------------------------------------------------
// Extended attributes test
//------------------------------------------------------------------------------
void FileSystemTest::XAttrTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Get the environment variables
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string remoteFile;

  EXPECT_TRUE( testEnv->GetString( "DiskServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "RemoteFile",    remoteFile ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  FileSystem fs( url );

  std::map<std::string, std::string> attributes
  {
      std::make_pair( "version",  "v1.2.3-45" ),
      std::make_pair( "checksum", "2ccc0e85556a6cd193dd8d2b40aab50c" ),
      std::make_pair( "index",    "4" )
  };

  //----------------------------------------------------------------------------
  // Test SetXAttr
  //----------------------------------------------------------------------------
  std::vector<xattr_t> attrs;
  auto itr1 = attributes.begin();
  for( ; itr1 != attributes.end() ; ++itr1 )
    attrs.push_back( std::make_tuple( itr1->first, itr1->second ) );

  std::vector<XAttrStatus> result1;
  GTEST_ASSERT_XRDST( fs.SetXAttr( remoteFile, attrs, result1 ) );

  auto itr2 = result1.begin();
  for( ; itr2 != result1.end() ; ++itr2 )
    GTEST_ASSERT_XRDST( itr2->status );
  result1.clear();

  //----------------------------------------------------------------------------
  // Test GetXAttr
  //----------------------------------------------------------------------------
  std::vector<std::string> names;
  itr1 = attributes.begin();
  for( ; itr1 != attributes.end() ; ++itr1 )
    names.push_back( itr1->first );

  std::vector<XAttr> result2;
  GTEST_ASSERT_XRDST( fs.GetXAttr( remoteFile, names, result2 ) );

  auto itr3 = result2.begin();
  for( ; itr3 != result2.end() ; ++itr3 )
  {
    GTEST_ASSERT_XRDST( itr3->status );
    auto match = attributes.find( itr3->name );
    EXPECT_NE( match, attributes.end() );
    EXPECT_EQ( match->second, itr3->value );
  }
  result2.clear();

  //----------------------------------------------------------------------------
  // Test ListXAttr
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( fs.ListXAttr( remoteFile, result2 ) );

  itr3 = result2.begin();
  for( ; itr3 != result2.end() ; ++itr3 )
  {
    GTEST_ASSERT_XRDST( itr3->status );
    auto match = attributes.find( itr3->name );
    EXPECT_NE( match, attributes.end() );
    EXPECT_EQ( match->second, itr3->value );
  }

  result2.clear();

  //----------------------------------------------------------------------------
  // Test DelXAttr
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( fs.DelXAttr( remoteFile, names, result1 ) );

  itr2 = result1.begin();
  for( ; itr2 != result1.end() ; ++itr2 )
    GTEST_ASSERT_XRDST( itr2->status );

  result1.clear();
}

