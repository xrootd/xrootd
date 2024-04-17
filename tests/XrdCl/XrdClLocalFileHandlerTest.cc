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
#include "TestEnv.hh"
#include "GTestXrdHelpers.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include <climits>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class LocalFileHandlerTest: public ::testing::Test
{
  public:
    void SetUp() override;
    void TearDown() override;

    void CreateTestFileFunc( std::string url, std::string content = "GenericTestFile" );
    void readTestFunc( bool offsetRead, uint32_t offset );
    void OpenCloseTest();
    void ReadTest();
    void ReadWithOffsetTest();
    void WriteTest();
    void WriteWithOffsetTest();
    void WriteMkdirTest();
    void TruncateTest();
    void VectorReadTest();
    void VectorWriteTest();
    void SyncTest();
    void WriteVTest();
    void XAttrTest();

    std::string m_tmpdir;
};

void LocalFileHandlerTest::SetUp()
{
  char cpath[MAXPATHLEN];
  ASSERT_TRUE(getcwd(cpath, sizeof(cpath))) <<
    "Could not get current working directory";
  m_tmpdir = std::string(cpath) + "/tmp";
}

void LocalFileHandlerTest::TearDown()
{
  /* empty */
}

//----------------------------------------------------------------------------
// Create the file to be tested
//----------------------------------------------------------------------------
void LocalFileHandlerTest::CreateTestFileFunc( std::string url, std::string content ){
   errno = 0;
   mode_t openmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
   int fd = open( url.c_str(), O_RDWR | O_CREAT | O_TRUNC, openmode );
   EXPECT_NE( fd, -1 );
   EXPECT_EQ( errno, 0 );
   int rc = write( fd, content.c_str(), content.size() );
   EXPECT_EQ( rc, int( content.size() ) );
   EXPECT_EQ( errno, 0 );
   rc = close( fd );
   EXPECT_EQ( rc, 0 );
}

//----------------------------------------------------------------------------
// Performs a ReadTest
//----------------------------------------------------------------------------
void LocalFileHandlerTest::readTestFunc(bool offsetRead, uint32_t offset){
   using namespace XrdCl;
   std::string targetURL = m_tmpdir + "/lfilehandlertestfileread";
   std::string toBeWritten = "tenBytes10";
   std::string expectedRead = "Byte";
   uint32_t size =
      ( offsetRead ? expectedRead.size() : toBeWritten.size() );
   char *buffer = new char[size];
   uint32_t bytesRead = 0;

   //----------------------------------------------------------------------------
   // Write file with POSIX calls to ensure correct write
   //----------------------------------------------------------------------------
   CreateTestFileFunc( targetURL, toBeWritten );

   //----------------------------------------------------------------------------
   // Open and Read File
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File *file = new File();
   GTEST_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   EXPECT_TRUE( file->IsOpen() );
   GTEST_ASSERT_XRDST( file->Read( offset, size, buffer, bytesRead ) );
   GTEST_ASSERT_XRDST( file->Close() );

   std::string read( buffer, size );
   if (offsetRead) EXPECT_EQ( expectedRead, read );
   else EXPECT_EQ( toBeWritten, read );

   EXPECT_EQ( remove( targetURL.c_str() ), 0 );

   delete[] buffer;
   delete file;

}

TEST_F(LocalFileHandlerTest, SyncTest){
   using namespace XrdCl;
   std::string targetURL = m_tmpdir + "/lfilehandlertestfilesync";
   CreateTestFileFunc( targetURL );

   //----------------------------------------------------------------------------
   // Open and Sync File
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR | Access::UW | Access::GR | Access::OR;
   File *file = new File();
   GTEST_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   GTEST_ASSERT_XRDST( file->Sync() );
   GTEST_ASSERT_XRDST( file->Close() );
   EXPECT_EQ( remove( targetURL.c_str() ), 0 );
   delete file;
}

TEST_F(LocalFileHandlerTest, OpenCloseTest){
   using namespace XrdCl;
   std::string targetURL = m_tmpdir + "/lfilehandlertestfileopenclose";
   CreateTestFileFunc( targetURL );

   //----------------------------------------------------------------------------
   // Open existing file
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File *file = new File();
   GTEST_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   EXPECT_TRUE( file->IsOpen() );
   GTEST_ASSERT_XRDST( file->Close() );
   EXPECT_EQ( remove( targetURL.c_str() ), 0 );

   //----------------------------------------------------------------------------
   // Try open non-existing file
   //----------------------------------------------------------------------------
   EXPECT_TRUE( !file->Open( targetURL, flags, mode ).IsOK() );
   EXPECT_TRUE( !file->IsOpen() );

   //----------------------------------------------------------------------------
   // Try close non-opened file, return has to be error
   //----------------------------------------------------------------------------
   EXPECT_EQ( file->Close().status, stError );
   delete file;
}

TEST_F(LocalFileHandlerTest, WriteTest){
   using namespace XrdCl;
   std::string targetURL = m_tmpdir + "/lfilehandlertestfilewrite";
   std::string toBeWritten = "tenBytes1\0";
   uint32_t writeSize = toBeWritten.size();
   CreateTestFileFunc( targetURL, "" );
   char *buffer = new char[writeSize];
   //----------------------------------------------------------------------------
   // Open and Write File
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File *file = new File();
   GTEST_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   EXPECT_TRUE( file->IsOpen() );
   GTEST_ASSERT_XRDST( file->Write( 0, writeSize, toBeWritten.c_str()) );
   GTEST_ASSERT_XRDST( file->Close() );

   //----------------------------------------------------------------------------
   // Read file with POSIX calls to confirm correct write
   //----------------------------------------------------------------------------
   int fd = open( targetURL.c_str(), O_RDWR );
   int rc = read( fd, buffer, int( writeSize ) );
   EXPECT_EQ( rc, int( writeSize ) );
   std::string read( (char *)buffer, writeSize );
   EXPECT_EQ( toBeWritten, read );
   EXPECT_EQ( remove( targetURL.c_str() ), 0 );
   delete[] buffer;
   delete file;
}

TEST_F(LocalFileHandlerTest, WriteWithOffsetTest){
   using namespace XrdCl;
   std::string targetURL = m_tmpdir + "/lfilehandlertestfilewriteoffset";
   std::string toBeWritten = "tenBytes10";
   std::string notToBeOverwritten = "front";
   uint32_t writeSize = toBeWritten.size();
   uint32_t offset = notToBeOverwritten.size();
   void *buffer = new char[offset];
   CreateTestFileFunc( targetURL, notToBeOverwritten );

   //----------------------------------------------------------------------------
   // Open and Write File
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File *file = new File();
   GTEST_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   EXPECT_TRUE( file->IsOpen() );
   GTEST_ASSERT_XRDST( file->Write( offset, writeSize, toBeWritten.c_str()) );
   GTEST_ASSERT_XRDST( file->Close() );

   //----------------------------------------------------------------------------
   // Read file with POSIX calls to confirm correct write
   //----------------------------------------------------------------------------
   int fd = open( targetURL.c_str(), O_RDWR );
   int rc = read( fd, buffer, offset );
   EXPECT_EQ( rc, int( offset ) );
   std::string read( (char *)buffer, offset );
   EXPECT_EQ( notToBeOverwritten, read );
   EXPECT_EQ( remove( targetURL.c_str() ), 0 );
   delete[] (char*)buffer;
   delete file;
}

TEST_F(LocalFileHandlerTest, WriteMkdirTest){
   using namespace XrdCl;
   std::string targetURL = m_tmpdir + "/testdir/further/muchfurther/evenfurther/lfilehandlertestfilewrite";
   std::string toBeWritten = "tenBytes10";
   uint32_t writeSize = toBeWritten.size();
   char *buffer = new char[writeSize];

   //----------------------------------------------------------------------------
   // Open and Write File
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update | OpenFlags::MakePath | OpenFlags::New;
   Access::Mode mode = Access::UR|Access::UW|Access::UX;
   File *file = new File();
   GTEST_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   EXPECT_TRUE( file->IsOpen() );
   GTEST_ASSERT_XRDST( file->Write( 0, writeSize, toBeWritten.c_str()) );
   GTEST_ASSERT_XRDST( file->Close() );

   //----------------------------------------------------------------------------
   // Read file with POSIX calls to confirm correct write
   //----------------------------------------------------------------------------
   int fd = open( targetURL.c_str(), O_RDWR );
   int rc = read( fd, buffer, writeSize );
   EXPECT_EQ( rc, int( writeSize ) );
   std::string read( buffer, writeSize );
   EXPECT_EQ( toBeWritten, read );
   EXPECT_EQ( remove( targetURL.c_str() ), 0 );
   delete[] buffer;
   delete file;
}

TEST_F(LocalFileHandlerTest, ReadTests){
   // Normal read test
   readTestFunc(false, 0);
   // Read with offset test
   readTestFunc(true, 3);
}

TEST_F(LocalFileHandlerTest, TruncateTest){
   using namespace XrdCl;
   //----------------------------------------------------------------------------
   // Initialize
   //----------------------------------------------------------------------------
   std::string targetURL = m_tmpdir + "/lfilehandlertestfiletruncate";

   CreateTestFileFunc(targetURL);
   //----------------------------------------------------------------------------
   // Prepare truncate
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update | OpenFlags::Force;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File *file = new File();
   uint32_t bytesRead = 0;
   uint32_t truncateSize = 5;

   //----------------------------------------------------------------------------
   // Read after truncate, but with greater length. bytesRead must still be
   // truncate size if truncate works as intended
   //----------------------------------------------------------------------------
   GTEST_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   GTEST_ASSERT_XRDST( file->Truncate( truncateSize ) );
   char *buffer = new char[truncateSize + 3];
   GTEST_ASSERT_XRDST( file->Read( 0, truncateSize + 3, buffer, bytesRead ) );
   EXPECT_EQ( truncateSize, bytesRead );
   GTEST_ASSERT_XRDST( file->Close() );
   EXPECT_EQ( remove( targetURL.c_str() ), 0 );
   delete file;
   delete[] buffer;
}

TEST_F(LocalFileHandlerTest, VectorReadTest)
{
   using namespace XrdCl;

   //----------------------------------------------------------------------------
   // Initialize
   //----------------------------------------------------------------------------
   std::string targetURL = m_tmpdir + "/lfilehandlertestfilevectorread";
   CreateTestFileFunc( targetURL );
   VectorReadInfo *info = 0;
   ChunkList chunks;

   //----------------------------------------------------------------------------
   // Prepare VectorRead
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File file;
   GTEST_ASSERT_XRDST( file.Open( targetURL, flags, mode ) );

   //----------------------------------------------------------------------------
   // VectorRead no cursor
   //----------------------------------------------------------------------------

   chunks.push_back( ChunkInfo( 0, 5, new char[5] ) );
   chunks.push_back( ChunkInfo( 10, 5, new char[5] ) );
   GTEST_ASSERT_XRDST( file.VectorRead( chunks, NULL, info ) );
   GTEST_ASSERT_XRDST( file.Close() );
   EXPECT_EQ( info->GetSize(), 10 );
   EXPECT_EQ( 0, memcmp( "Gener",
                                    info->GetChunks()[0].buffer,
                                    info->GetChunks()[0].length ) );
   EXPECT_EQ( 0, memcmp( "tFile",
                                    info->GetChunks()[1].buffer,
                                    info->GetChunks()[1].length ) );
   delete[] (char*)chunks[0].buffer;
   delete[] (char*)chunks[1].buffer;
   delete info;

   //----------------------------------------------------------------------------
   // VectorRead cursor
   //----------------------------------------------------------------------------
   char *buffer = new char[10];
   chunks.clear();
   chunks.push_back( ChunkInfo( 0, 5, 0 ) );
   chunks.push_back( ChunkInfo( 10, 5, 0 ) );
   info = 0;

   GTEST_ASSERT_XRDST( file.Open( targetURL, flags, mode ) );
   GTEST_ASSERT_XRDST( file.VectorRead( chunks, buffer, info ) );
   GTEST_ASSERT_XRDST( file.Close() );
   EXPECT_EQ( info->GetSize(), 10 );
   EXPECT_EQ( 0, memcmp( "GenertFile",
                                    info->GetChunks()[0].buffer,
                                    info->GetChunks()[0].length ) );
   EXPECT_EQ( remove( targetURL.c_str() ), 0 );

   delete[] buffer;
   delete info;
}

TEST_F(LocalFileHandlerTest, VectorWriteTest)
{
   using namespace XrdCl;

   //----------------------------------------------------------------------------
   // Initialize
   //----------------------------------------------------------------------------
   std::string targetURL = m_tmpdir + "/lfilehandlertestfilevectorwrite";
   CreateTestFileFunc( targetURL );
   ChunkList chunks;

   //----------------------------------------------------------------------------
   // Prepare VectorWrite
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File file;
   GTEST_ASSERT_XRDST( file.Open( targetURL, flags, mode ) );

   //----------------------------------------------------------------------------
   // VectorWrite
   //----------------------------------------------------------------------------

   ChunkInfo chunk( 0, 5, new char[5] );
   memset( chunk.buffer, 'A', chunk.length );
   chunks.push_back( chunk );
   chunk = ChunkInfo( 10, 5, new char[5] );
   memset( chunk.buffer, 'B', chunk.length );
   chunks.push_back( chunk );

   GTEST_ASSERT_XRDST( file.VectorWrite( chunks ) );

   //----------------------------------------------------------------------------
   // Verify with VectorRead
   //----------------------------------------------------------------------------

   VectorReadInfo *info = 0;
   char *buffer = new char[10];
   GTEST_ASSERT_XRDST( file.VectorRead( chunks, buffer, info ) );

   EXPECT_EQ( 0, memcmp( buffer, "AAAAABBBBB", 10 ) );

   GTEST_ASSERT_XRDST( file.Close() );
   EXPECT_EQ( info->GetSize(), 10 );

   delete[] (char*)chunks[0].buffer;
   delete[] (char*)chunks[1].buffer;
   delete[] buffer;
   delete   info;

   EXPECT_EQ( remove( targetURL.c_str() ), 0 );
}

TEST_F(LocalFileHandlerTest, WriteVTest)
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  std::string targetURL = m_tmpdir + "/lfilehandlertestfilewritev";
  CreateTestFileFunc( targetURL );

  //----------------------------------------------------------------------------
  // Prepare WriteV
  //----------------------------------------------------------------------------
  OpenFlags::Flags flags = OpenFlags::Update;
  Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
  File file;
  GTEST_ASSERT_XRDST( file.Open( targetURL, flags, mode ) );

  char str[] = "WriteVTest";
  std::vector<char> buffer( 10 );
  std::copy( str, str + sizeof( str ) - 1, buffer.begin() );
  int iovcnt = 2;
  iovec iov[iovcnt];
  iov[0].iov_base = buffer.data();
  iov[0].iov_len  = 6;
  iov[1].iov_base = buffer.data() + 6;
  iov[1].iov_len  = 4;
  GTEST_ASSERT_XRDST( file.WriteV( 7, iov, iovcnt ) );

  uint32_t bytesRead = 0;
  buffer.resize( 17 );
  GTEST_ASSERT_XRDST( file.Read( 0, 17, buffer.data(), bytesRead ) );
  EXPECT_EQ( buffer.size(), 17 );
  std::string expected = "GenericWriteVTest";
  EXPECT_EQ( std::string( buffer.data(), buffer.size() ), expected );
  GTEST_ASSERT_XRDST( file.Close() );
  EXPECT_EQ( remove( targetURL.c_str() ), 0 );
}

TEST_F(LocalFileHandlerTest, XAttrTest)
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  // (we do the test in /data as /tmp might be on tpmfs,
  //  which does not support xattrs)
  // In this case, /data is /data inside of the build directory
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();
  std::string localDataPath;
  EXPECT_TRUE( testEnv->GetString( "LocalDataPath", localDataPath ) );

  char resolved_path[MAXPATHLEN];
  localDataPath = realpath(localDataPath.c_str(), resolved_path);

  std::string targetURL = localDataPath + "/metaman/lfilehandlertestfilexattr";
  CreateTestFileFunc( targetURL );

  File f;
  GTEST_ASSERT_XRDST( f.Open( targetURL, OpenFlags::Update ) );

  //----------------------------------------------------------------------------
  // Test XAttr Set
  //----------------------------------------------------------------------------
  std::vector<xattr_t> attrs;
  attrs.push_back( xattr_t( "version", "v3.3.3" ) );
  attrs.push_back( xattr_t( "description", "a very important file" ) );
  attrs.push_back( xattr_t( "checksum", "0x22334455" ) );

  std::vector<XAttrStatus> st_resp;

  GTEST_ASSERT_XRDST( f.SetXAttr( attrs, st_resp ) );

  std::vector<XAttrStatus>::iterator itr1;
  for( itr1 = st_resp.begin(); itr1 != st_resp.end(); ++itr1 )
    GTEST_ASSERT_XRDST( itr1->status );

  //----------------------------------------------------------------------------
  // Test XAttr Get
  //----------------------------------------------------------------------------
  std::vector<std::string> names;
  names.push_back( "version" );
  names.push_back( "description" );
  std::vector<XAttr> resp;
  GTEST_ASSERT_XRDST( f.GetXAttr( names, resp ) );

  GTEST_ASSERT_XRDST( resp[0].status );
  GTEST_ASSERT_XRDST( resp[1].status );

  EXPECT_EQ( resp.size(), 2 );
  int vid = resp[0].name == "version" ? 0 : 1;
  int did = vid == 0 ? 1 : 0;
  EXPECT_EQ( resp[vid].name, std::string("version") );
  EXPECT_EQ( resp[vid].value, std::string("v3.3.3") );
  EXPECT_EQ( resp[did].name, std::string("description") );
  EXPECT_EQ( resp[did].value, std::string("a very important file") );

  //----------------------------------------------------------------------------
  // Test XAttr Del
  //----------------------------------------------------------------------------
  names.clear();
  names.push_back( "description" );
  st_resp.clear();
  GTEST_ASSERT_XRDST( f.DelXAttr( names, st_resp ) );
  EXPECT_EQ( st_resp.size(), 1 );
  GTEST_ASSERT_XRDST( st_resp[0].status );

  //----------------------------------------------------------------------------
  // Test XAttr List
  //----------------------------------------------------------------------------
  resp.clear();
  GTEST_ASSERT_XRDST( f.ListXAttr( resp ) );
  EXPECT_EQ( resp.size(), 2 );
  vid = resp[0].name == "version" ? 0 : 1;
  int cid = vid == 0 ? 1 : 0;
  EXPECT_EQ( resp[vid].name, std::string("version") );
  EXPECT_EQ( resp[vid].value, std::string("v3.3.3") );
  EXPECT_EQ( resp[cid].name, std::string("checksum") );
  EXPECT_EQ( resp[cid].value, std::string("0x22334455") );

  //----------------------------------------------------------------------------
  // Cleanup
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f.Close() );
  EXPECT_EQ( remove( targetURL.c_str() ), 0 );
}
