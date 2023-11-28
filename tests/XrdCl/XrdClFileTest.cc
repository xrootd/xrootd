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
#include "Utils.hh"
#include "IdentityPlugIn.hh"

#include "GTestXrdHelpers.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPlugInManager.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClXRootDMsgHandler.hh"
#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClZipArchive.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClZipOperations.hh"
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Class declaration
//------------------------------------------------------------------------------
class FileTest: public ::testing::Test
{
  public:
    void RedirectReturnTest();
    void ReadTest();
    void WriteTest();
    void WriteVTest();
    void VectorReadTest();
    void VectorWriteTest();
    void VirtualRedirectorTest();
    void XAttrTest();
};

//------------------------------------------------------------------------------
// Tests declaration
//------------------------------------------------------------------------------
TEST_F(FileTest, RedirectReturnTest)
{
  RedirectReturnTest();
}

TEST_F(FileTest, ReadTest)
{
  ReadTest();
}

TEST_F(FileTest, WriteTest)
{
  WriteTest();
}

TEST_F(FileTest, WriteVTest)
{
  WriteVTest();
}

TEST_F(FileTest, VectorReadTest)
{
  VectorReadTest();
}

TEST_F(FileTest, VectorWriteTest)
{
  VectorWriteTest();
}

// This test is commented out as of now since we think that there's a bug in the code

// TEST_F(FileTest, VirtualRedirectorTest)
// {
//   VirtualRedirectorTest();
// }

TEST_F(FileTest, XAttrTest)
{
  XAttrTest();
}

TEST_F(FileTest, PlugInTest)
{
  XrdCl::PlugInFactory *f = new IdentityFactory;
  XrdCl::DefaultEnv::GetPlugInManager()->RegisterDefaultFactory(f);
  RedirectReturnTest();
  ReadTest();
  WriteTest();
  VectorReadTest();
  XrdCl::DefaultEnv::GetPlugInManager()->RegisterDefaultFactory(0);
}

//------------------------------------------------------------------------------
// Redirect return test
//------------------------------------------------------------------------------
void FileTest::RedirectReturnTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string path = dataPath + "/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat";
  std::string fileUrl = address + "/" + path;

  //----------------------------------------------------------------------------
  // Build the open request
  //----------------------------------------------------------------------------
  Message           *msg;
  ClientOpenRequest *req;
  MessageUtils::CreateRequest( msg, req, path.length() );
  req->requestid = kXR_open;
  req->options   = kXR_open_read | kXR_retstat;
  req->dlen      = path.length();
  msg->Append( path.c_str(), path.length(), 24 );
  XRootDTransport::SetDescription( msg );

  SyncResponseHandler *handler = new SyncResponseHandler();
  MessageSendParams params; params.followRedirects = false;
  MessageUtils::ProcessSendParams( params );
  OpenInfo *response = 0;
  GTEST_ASSERT_XRDST( MessageUtils::SendMessage( url, msg, handler, params, 0 ) );
  XRootDStatus st1 = MessageUtils::WaitForResponse( handler, response );
  delete handler;
  GTEST_ASSERT_XRDST_NOTOK( st1, errRedirect );
  EXPECT_TRUE( !response );
  delete response;
}

//------------------------------------------------------------------------------
// Read test
//------------------------------------------------------------------------------
void FileTest::ReadTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;
  std::string localDataPath;

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );
  EXPECT_TRUE( testEnv->GetString( "LocalDataPath", localDataPath ) );


  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string filePath = dataPath + "/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat";
  std::string fileUrl = address + "/";
  fileUrl += filePath;
  localDataPath = realpath(localDataPath.c_str(), NULL);
  // using the file protocol to access local files, so that we can use XRootD's own functions
  std::string localFileUrl = "file://localhost" + localDataPath + "/srv1" + filePath;

  //----------------------------------------------------------------------------
  // Fetch some data and checksum
  //----------------------------------------------------------------------------
  const uint32_t MB = 1024*1024;
  // buffers containing remote file data
  char *buffer1 = new char[10*MB];
  char *buffer2 = new char[10*MB];
  // comparison buffers containing local file data
  char *buffer1Comp = new char[10*MB];
  char *buffer2Comp = new char[10*MB];
  uint32_t bytesRead1 = 0, bytesRead2 = 0;
  File f, fLocal;
  StatInfo *statInfo;

  //----------------------------------------------------------------------------
  // Open the file
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f.Open( fileUrl, OpenFlags::Read ) );
  GTEST_ASSERT_XRDST( fLocal.Open( localFileUrl, OpenFlags::Read ) );

  //----------------------------------------------------------------------------
  // Stat1
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f.Stat( false, statInfo ) );
  EXPECT_TRUE( statInfo );

  struct stat localStatBuf;

  std::string localFilePath = localDataPath + "/srv1" + filePath;

  int rc = stat(localFilePath.c_str(), &localStatBuf);
  EXPECT_EQ( rc, 0 );
  uint64_t fileSize = localStatBuf.st_size;
  EXPECT_EQ( statInfo->GetSize(), fileSize );
  EXPECT_TRUE( statInfo->TestFlags( StatInfo::IsReadable ) );

  delete statInfo;
  statInfo = 0;

  //----------------------------------------------------------------------------
  // Stat2
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f.Stat( true, statInfo ) );
  EXPECT_TRUE( statInfo );
  EXPECT_EQ( statInfo->GetSize(), fileSize );
  EXPECT_TRUE( statInfo->TestFlags( StatInfo::IsReadable ) );
  delete statInfo;

  //----------------------------------------------------------------------------
  // Read test
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f.Read( 5*MB, 5*MB, buffer1, bytesRead1 ) );
  GTEST_ASSERT_XRDST( f.Read( fileSize - 3*MB, 4*MB, buffer2, bytesRead2 ) );
  EXPECT_EQ( bytesRead1, 5*MB );
  EXPECT_EQ( bytesRead2, 3*MB );

  uint32_t crc = XrdClTests::Utils::ComputeCRC32( buffer1, 5*MB );

  // reinitializing the file cursor
  bytesRead1 = bytesRead2 = 0;
  GTEST_ASSERT_XRDST( fLocal.Read( 5*MB, 5*MB, buffer1Comp, bytesRead1 ) );
  GTEST_ASSERT_XRDST( fLocal.Read( fileSize - 3*MB, 4*MB, buffer2Comp, bytesRead2 ) );

  // compute and compare local file buffer crc
  uint32_t crcComp = XrdClTests::Utils::ComputeCRC32( buffer1Comp, 5*MB );

  EXPECT_EQ( crc, crcComp );

  // do the same for the second buffer
  crc = XrdClTests::Utils::ComputeCRC32( buffer2, 3*MB );
  crcComp = XrdClTests::Utils::ComputeCRC32( buffer2Comp, 3*MB );

  EXPECT_EQ( crc, crcComp );

  // cleanup after ourselves
  delete [] buffer1;
  delete [] buffer2;
  delete [] buffer1Comp;
  delete [] buffer2Comp;

  GTEST_ASSERT_XRDST( f.Close() );
  GTEST_ASSERT_XRDST( fLocal.Close() );

  //----------------------------------------------------------------------------
  // Read ZIP archive test (uncompressed)
  //----------------------------------------------------------------------------
  std::string archiveUrl = address + "/" + dataPath + "/data.zip";

  ZipArchive zip;
  GTEST_ASSERT_XRDST( WaitFor( OpenArchive( zip, archiveUrl, OpenFlags::Read ) ) );

  //----------------------------------------------------------------------------
  // There are 3 files in the data.zip archive:
  //  - athena.log
  //  - paper.txt
  //  - EastAsianWidth.txt
  //----------------------------------------------------------------------------

  struct
  {
      std::string file;        // file name
      uint64_t    offset;      // offset in the file
      uint32_t    size;        // number of characters to be read
      char        buffer[100]; // the buffer
      std::string expected;    // expected result
  } testset[] =
  {
      { "athena.log",         65530, 99, {0}, "D__Jet" }, // reads past the end of the file (there are just 6 characters to read not 99)
      { "paper.txt",          1024,  65, {0}, "igh rate (the order of 100 kHz), the data are usually distributed" },
      { "EastAsianWidth.txt", 2048,  18, {0}, "8;Na # DIGIT EIGHT" }
  };

  for( int i = 0; i < 3; ++i )
  {
    std::string result;
    GTEST_ASSERT_XRDST( WaitFor(
        ReadFrom( zip, testset[i].file, testset[i].offset, testset[i].size, testset[i].buffer ) >>
          [&result]( auto& s, auto& c )
          {
            if( s.IsOK() )
              result.assign( static_cast<char*>(c.buffer), c.length );
          }
      ) );
    EXPECT_EQ( testset[i].expected, result );
  }

  GTEST_ASSERT_XRDST( WaitFor( CloseArchive( zip ) ) );
}


//------------------------------------------------------------------------------
// Read test
//------------------------------------------------------------------------------
void FileTest::WriteTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string filePath = dataPath + "/testFile.dat";
  std::string fileUrl = address + "/";
  fileUrl += filePath;

  //----------------------------------------------------------------------------
  // Fetch some data and checksum
  //----------------------------------------------------------------------------
  const uint32_t MB = 1024*1024;
  char *buffer1 = new char[4*MB];
  char *buffer2 = new char[4*MB];
  char *buffer3 = new char[4*MB];
  char *buffer4 = new char[4*MB];
  uint32_t bytesRead1 = 0;
  uint32_t bytesRead2 = 0;
  File f1, f2;

  EXPECT_EQ( XrdClTests::Utils::GetRandomBytes( buffer1, 4*MB ), 4*MB );
  EXPECT_EQ( XrdClTests::Utils::GetRandomBytes( buffer2, 4*MB ), 4*MB );
  uint32_t crc1 = XrdClTests::Utils::ComputeCRC32( buffer1, 4*MB );
  crc1 = XrdClTests::Utils::UpdateCRC32( crc1, buffer2, 4*MB );

  //----------------------------------------------------------------------------
  // Write the data
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f1.Open( fileUrl, OpenFlags::Delete | OpenFlags::Update,
                           Access::UR | Access::UW ));

  EXPECT_TRUE( f1.Write( 0, 4*MB, buffer1 ).IsOK() );
  EXPECT_TRUE( f1.Write( 4*MB, 4*MB, buffer2 ).IsOK() );
  EXPECT_TRUE( f1.Sync().IsOK() );
  EXPECT_TRUE( f1.Close().IsOK() );

  //----------------------------------------------------------------------------
  // Read the data and verify the checksums
  //----------------------------------------------------------------------------
  StatInfo *statInfo = 0;
  EXPECT_TRUE( f2.Open( fileUrl, OpenFlags::Read ).IsOK() );
  EXPECT_TRUE( f2.Stat( false, statInfo ).IsOK() );
  EXPECT_TRUE( statInfo );
  EXPECT_EQ( statInfo->GetSize(), 8*MB );
  EXPECT_TRUE( f2.Read( 0, 4*MB, buffer3, bytesRead1 ).IsOK() );
  EXPECT_TRUE( f2.Read( 4*MB, 4*MB, buffer4, bytesRead2 ).IsOK() );
  EXPECT_EQ( bytesRead1, 4*MB );
  EXPECT_EQ( bytesRead2, 4*MB );
  uint32_t crc2 = XrdClTests::Utils::ComputeCRC32( buffer3, 4*MB );
  crc2 = XrdClTests::Utils::UpdateCRC32( crc2, buffer4, 4*MB );
  EXPECT_TRUE( f2.Close().IsOK() );
  EXPECT_EQ( crc1, crc2 );

  //----------------------------------------------------------------------------
  // Truncate test
  //----------------------------------------------------------------------------
  EXPECT_TRUE( f1.Open( fileUrl, OpenFlags::Delete | OpenFlags::Update,
                           Access::UR | Access::UW ).IsOK() );
  EXPECT_TRUE( f1.Truncate( 20*MB ).IsOK() );
  EXPECT_TRUE( f1.Close().IsOK() );
  FileSystem fs( url );
  StatInfo *response = 0;
  EXPECT_TRUE( fs.Stat( filePath, response ).IsOK() );
  EXPECT_TRUE( response );
  EXPECT_EQ( response->GetSize(), 20*MB );
  EXPECT_TRUE( fs.Rm( filePath ).IsOK() );
  delete [] buffer1;
  delete [] buffer2;
  delete [] buffer3;
  delete [] buffer4;
  delete response;
  delete statInfo;
}

//------------------------------------------------------------------------------
// WriteV test
//------------------------------------------------------------------------------
void FileTest::WriteVTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string filePath = dataPath + "/testFile.dat";
  std::string fileUrl = address + "/";
  fileUrl += filePath;

  //----------------------------------------------------------------------------
  // Fetch some data and checksum
  //----------------------------------------------------------------------------
  const uint32_t MB = 1024*1024;
  char *buffer1 = new char[4*MB];
  char *buffer2 = new char[4*MB];
  char *buffer3 = new char[8*MB];
  uint32_t bytesRead1 = 0;
  File f1, f2;

  EXPECT_EQ( XrdClTests::Utils::GetRandomBytes( buffer1, 4*MB ), 4*MB );
  EXPECT_EQ( XrdClTests::Utils::GetRandomBytes( buffer2, 4*MB ), 4*MB );
  uint32_t crc1 = XrdClTests::Utils::ComputeCRC32( buffer1, 4*MB );
  crc1 = XrdClTests::Utils::UpdateCRC32( crc1, buffer2, 4*MB );

  //----------------------------------------------------------------------------
  // Prepare IO vector
  //----------------------------------------------------------------------------
  int iovcnt = 2;
  iovec iov[iovcnt];
  iov[0].iov_base = buffer1;
  iov[0].iov_len  = 4*MB;
  iov[1].iov_base = buffer2;
  iov[1].iov_len  = 4*MB;

  //----------------------------------------------------------------------------
  // Write the data
  //----------------------------------------------------------------------------
  EXPECT_TRUE( f1.Open( fileUrl, OpenFlags::Delete | OpenFlags::Update,
                           Access::UR | Access::UW ).IsOK() );
  EXPECT_TRUE( f1.WriteV( 0, iov, iovcnt ).IsOK() );
  EXPECT_TRUE( f1.Sync().IsOK() );
  EXPECT_TRUE( f1.Close().IsOK() );

  //----------------------------------------------------------------------------
  // Read the data and verify the checksums
  //----------------------------------------------------------------------------
  StatInfo *statInfo = 0;
  EXPECT_TRUE( f2.Open( fileUrl, OpenFlags::Read ).IsOK() );
  EXPECT_TRUE( f2.Stat( false, statInfo ).IsOK() );
  EXPECT_TRUE( statInfo );
  EXPECT_EQ( statInfo->GetSize(), 8*MB );
  EXPECT_TRUE( f2.Read( 0, 8*MB, buffer3, bytesRead1 ).IsOK() );
  EXPECT_EQ( bytesRead1, 8*MB );

  uint32_t crc2 = XrdClTests::Utils::ComputeCRC32( buffer3, 8*MB );
  EXPECT_TRUE( f2.Close().IsOK() );
  EXPECT_EQ( crc1, crc2 );

  FileSystem fs( url );
  EXPECT_TRUE( fs.Rm( filePath ).IsOK() );
  delete [] buffer1;
  delete [] buffer2;
  delete [] buffer3;
  delete statInfo;
}

//------------------------------------------------------------------------------
// Vector read test
//------------------------------------------------------------------------------
void FileTest::VectorReadTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;
  std::string localDataPath;

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );
  EXPECT_TRUE( testEnv->GetString( "LocalDataPath", localDataPath ) );


  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string filePath = dataPath + "/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat";
  std::string fileUrl = address + "/";
  fileUrl += filePath;
  localDataPath += "/srv1" + filePath;
  localDataPath = realpath(localDataPath.c_str(), NULL);

  //----------------------------------------------------------------------------
  // Fetch some data and checksum
  //----------------------------------------------------------------------------
  const uint32_t MB = 1024*1024;
  char *buffer1 = new char[10*MB];
  char *buffer2 = new char[10*256000];
  char *buffer1Comp = new char[10*MB];
  char *buffer2Comp = new char[10*256000];
  File f, fLocal;

  //----------------------------------------------------------------------------
  // Build the chunk list
  //----------------------------------------------------------------------------
  ChunkList chunkList1;
  ChunkList chunkList2;
  for( int i = 0; i < 10; ++i )
  {
    chunkList1.push_back( ChunkInfo( (i+1)*1*MB, 1*MB ) );
    chunkList2.push_back( ChunkInfo( (i+1)*1*MB, 256000 ) );
  }

  //----------------------------------------------------------------------------
  // Open the file
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f.Open( fileUrl, OpenFlags::Read ) );
  GTEST_ASSERT_XRDST( fLocal.Open( localDataPath, OpenFlags::Read ) );

  // remote file vread1
  VectorReadInfo *info = 0;
  GTEST_ASSERT_XRDST( f.VectorRead( chunkList1, buffer1, info ) );
  EXPECT_EQ( info->GetSize(), 10*MB );
  delete info;

  // local file vread1
  info = 0;
  GTEST_ASSERT_XRDST( fLocal.VectorRead( chunkList1, buffer1Comp, info ) );
  EXPECT_EQ( info->GetSize(), 10*MB );
  delete info;

  // checksum comparison
  uint32_t crc = 0, crcComp = 0;
  crc = XrdClTests::Utils::ComputeCRC32( buffer1, 10*MB );
  crcComp = XrdClTests::Utils::ComputeCRC32( buffer1Comp, 10*MB );
  EXPECT_EQ( crc, crcComp );

  // remote vread2
  info = 0;
  GTEST_ASSERT_XRDST( f.VectorRead( chunkList2, buffer2, info ) );
  EXPECT_EQ( info->GetSize(), 10*256000 );
  delete info;

  // local vread2
  info = 0;
  GTEST_ASSERT_XRDST( f.VectorRead( chunkList2, buffer2Comp, info ) );
  EXPECT_EQ( info->GetSize(), 10*256000 );
  delete info;

  // checksum comparison again
  crc = XrdClTests::Utils::ComputeCRC32( buffer2, 10*256000 );
  crcComp = XrdClTests::Utils::ComputeCRC32( buffer2Comp, 10*256000 );
  EXPECT_EQ( crc, crcComp );

  // cleanup
  GTEST_ASSERT_XRDST( f.Close() );
  GTEST_ASSERT_XRDST( fLocal.Close() );
  delete [] buffer1;
  delete [] buffer2;
  delete [] buffer1Comp;
  delete [] buffer2Comp;
}

void gen_random_str(char *s, const int len)
{
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len - 1; ++i)
    {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    s[len - 1] = 0;
}

//------------------------------------------------------------------------------
// Vector write test
//------------------------------------------------------------------------------
void FileTest::VectorWriteTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string filePath = dataPath + "/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat";
  std::string fileUrl = address + "/";
  fileUrl += filePath;

  //----------------------------------------------------------------------------
  // Build a random chunk list for vector write
  //----------------------------------------------------------------------------

  const uint32_t MB = 1024*1024;
  const uint32_t limit = 10*MB;

  time_t seed = time( 0 );
  srand( seed );
  DefaultEnv::GetLog()->Info( UtilityMsg,
      "Carrying out the VectorWrite test with seed: %d", seed );

  // figure out how many chunks are we going to write/read
  size_t nbChunks = rand() % 100 + 1;

  XrdCl::ChunkList chunks;
  size_t   min_offset = 0;
  uint32_t expectedCrc32 = 0;
  size_t   totalSize = 0;

  for( size_t i = 0; i < nbChunks; ++i )
  {
    // figure out the offset
    size_t offset = min_offset + rand() % ( limit - min_offset + 1 );

    // figure out the size
    size_t size = MB + rand() % ( MB + 1 );
    if( offset + size >= limit )
      size = limit - offset;

    // generate random string of given size
    char *buffer = new char[size];
    gen_random_str( buffer, size );

    // calculate expected checksum
    expectedCrc32 = XrdClTests::Utils::UpdateCRC32( expectedCrc32, buffer, size );
    totalSize += size;
    chunks.push_back( XrdCl::ChunkInfo( offset, size, buffer ) );

    min_offset = offset + size;
    if( min_offset >= limit )
      break;
  }

  //----------------------------------------------------------------------------
  // Open the file
  //----------------------------------------------------------------------------
  File f;
  GTEST_ASSERT_XRDST( f.Open( fileUrl, OpenFlags::Update ) );

  //----------------------------------------------------------------------------
  // First do a VectorRead so we can revert to the original state
  //----------------------------------------------------------------------------
  char *buffer1 = new char[totalSize];
  VectorReadInfo *info1 = 0;
  GTEST_ASSERT_XRDST( f.VectorRead( chunks, buffer1, info1 ) );

  //----------------------------------------------------------------------------
  // Then do the VectorWrite
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f.VectorWrite( chunks ) );

  //----------------------------------------------------------------------------
  // Now do a vector read and verify that the checksum is the same
  //----------------------------------------------------------------------------
  char *buffer2 = new char[totalSize];
  VectorReadInfo *info2 = 0;
  GTEST_ASSERT_XRDST( f.VectorRead( chunks, buffer2, info2 ) );

  EXPECT_EQ( info2->GetSize(), totalSize );
  uint32_t crc32 = XrdClTests::Utils::ComputeCRC32( buffer2, totalSize );
  EXPECT_EQ( crc32, expectedCrc32 );

  //----------------------------------------------------------------------------
  // And finally revert to the original state
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f.VectorWrite( info1->GetChunks() ) );
  GTEST_ASSERT_XRDST( f.Close() );

  delete info1;
  delete info2;
  delete [] buffer1;
  delete [] buffer2;
  for( auto itr = chunks.begin(); itr != chunks.end(); ++itr )
    delete[] (char*)itr->buffer;
}

void FileTest::VirtualRedirectorTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  EXPECT_TRUE( testEnv->GetString( "MainServerURL", address ) );
  EXPECT_TRUE( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  EXPECT_TRUE( url.IsValid() );

  std::string mlUrl1 = address + "/" + dataPath + "/metalink/mlFileTest1.meta4";
  std::string mlUrl2 = address + "/" + dataPath + "/metalink/mlFileTest2.meta4";
  std::string mlUrl3 = address + "/" + dataPath + "/metalink/mlFileTest3.meta4";
  std::string mlUrl4 = address + "/" + dataPath + "/metalink/mlFileTest4.meta4";

  File f1, f2, f3, f4;

  std::string srv1Hostname;
  EXPECT_TRUE( testEnv->GetString( "Server1URL", srv1Hostname ) );
  const std::string fileUrl = "root://" + srv1Hostname + "/" + dataPath + "/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat";

  const std::string key = "LastURL";
  std::string value;

  //----------------------------------------------------------------------------
  // Open the 1st metalink file
  // (the metalink contains just one file with a correct location)
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f1.Open( mlUrl1, OpenFlags::Read ) );
  EXPECT_TRUE( f1.GetProperty( key, value ) );
  URL lastUrl( value );
  EXPECT_EQ( lastUrl.GetLocation(), fileUrl );
  GTEST_ASSERT_XRDST( f1.Close() );

  //----------------------------------------------------------------------------
  // Open the 2nd metalink file
  // (the metalink contains 2 files, the one with higher priority does not exist)
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f2.Open( mlUrl2, OpenFlags::Read ) );
  EXPECT_TRUE( f2.GetProperty( key, value ) );
  URL lastUrl2( value );
  EXPECT_EQ( lastUrl2.GetLocation(), fileUrl );
  GTEST_ASSERT_XRDST( f2.Close() );

  //----------------------------------------------------------------------------
  // Open the 3rd metalink file
  // (the metalink contains 2 files, both don't exist)
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST_NOTOK( f3.Open( mlUrl3, OpenFlags::Read ), errErrorResponse );

  //----------------------------------------------------------------------------
  // Open the 4th metalink file
  // (the metalink contains 2 files, both exist)
  //----------------------------------------------------------------------------

  std::string srv3Hostname;
  EXPECT_TRUE( testEnv->GetString( "Server3URL", srv3Hostname ) );
  std::string replica1 = "root://" + srv3Hostname + "/" + dataPath + "/3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat";

  std::string srv2Hostname;
  EXPECT_TRUE( testEnv->GetString( "Server2URL", srv2Hostname ) );
  const std::string replica2 = "root://" + srv2Hostname + "/" + dataPath + "/3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat";

  GTEST_ASSERT_XRDST( f4.Open( mlUrl4, OpenFlags::Read ) );
  EXPECT_TRUE( f4.GetProperty( key, value ) );
  URL lastUrl3( value );
  EXPECT_EQ( lastUrl3.GetLocation(), replica1 );
  GTEST_ASSERT_XRDST( f4.Close() );
  //----------------------------------------------------------------------------
  // Delete the replica that has been selected by the virtual redirector
  //----------------------------------------------------------------------------
  FileSystem fs( replica1 );
  GTEST_ASSERT_XRDST( fs.Rm( dataPath + "/3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat" ) );
  //----------------------------------------------------------------------------
  // Now reopen the file
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( f4.Open( mlUrl4, OpenFlags::Read ) );
  EXPECT_TRUE( f4.GetProperty( key, value ) );
  URL lastUrl4( value );
  EXPECT_EQ( lastUrl4.GetLocation(), replica2 );
  GTEST_ASSERT_XRDST( f4.Close() );
  //----------------------------------------------------------------------------
  // Recreate the deleted file
  //----------------------------------------------------------------------------
  CopyProcess  process;
  PropertyList properties, results;
  properties.Set( "source",       replica2 );
  properties.Set( "target",       replica1 );
  GTEST_ASSERT_XRDST( process.AddJob( properties, &results ) );
  GTEST_ASSERT_XRDST( process.Prepare() );
  GTEST_ASSERT_XRDST( process.Run(0) );
}

void FileTest::XAttrTest()
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

  std::string filePath = dataPath + "/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat";
  std::string fileUrl = address + "/" + filePath;

  URL url( address );
  EXPECT_TRUE( url.IsValid() );


  File file;
  GTEST_ASSERT_XRDST( file.Open( fileUrl, OpenFlags::Update ) );

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
  GTEST_ASSERT_XRDST( file.SetXAttr( attrs, result1 ) );

  auto itr2 = result1.begin();
  for( ; itr2 != result1.end() ; ++itr2 )
    GTEST_ASSERT_XRDST( itr2->status );

  //----------------------------------------------------------------------------
  // Test GetXAttr
  //----------------------------------------------------------------------------
  std::vector<std::string> names;
  itr1 = attributes.begin();
  for( ; itr1 != attributes.end() ; ++itr1 )
    names.push_back( itr1->first );

  std::vector<XAttr> result2;
  GTEST_ASSERT_XRDST( file.GetXAttr( names, result2 ) );

  auto itr3 = result2.begin();
  for( ; itr3 != result2.end() ; ++itr3 )
  {
    GTEST_ASSERT_XRDST( itr3->status );
    auto match = attributes.find( itr3->name );
    EXPECT_NE( match, attributes.end() );
    EXPECT_EQ( match->second, itr3->value );
  }

  //----------------------------------------------------------------------------
  // Test ListXAttr
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( file.ListXAttr( result2 ) );

  itr3 = result2.begin();
  for( ; itr3 != result2.end() ; ++itr3 )
  {
    GTEST_ASSERT_XRDST( itr3->status );
    auto match = attributes.find( itr3->name );
    EXPECT_NE( match, attributes.end() );
    EXPECT_EQ( match->second, itr3->value );
  }

  //----------------------------------------------------------------------------
  // Test DelXAttr
  //----------------------------------------------------------------------------
  GTEST_ASSERT_XRDST( file.DelXAttr( names, result1 ) );

  itr2 = result1.begin();
  for( ; itr2 != result1.end() ; ++itr2 )
    GTEST_ASSERT_XRDST( itr2->status );

  GTEST_ASSERT_XRDST( file.Close() );
}
