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
#include "TestEnv.hh"
#include "Utils.hh"
#include "IdentityPlugIn.hh"

#include "CppUnitXrdHelpers.hh"
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
#include "XrdCl/XrdClZipArchiveReader.hh"
#include "XrdCl/XrdClConstants.hh"

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class FileTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( FileTest );
      CPPUNIT_TEST( RedirectReturnTest );
      CPPUNIT_TEST( ReadTest );
      CPPUNIT_TEST( WriteTest );
      CPPUNIT_TEST( WriteVTest );
      CPPUNIT_TEST( VectorReadTest );
      CPPUNIT_TEST( VectorWriteTest );
      CPPUNIT_TEST( VirtualRedirectorTest );
      CPPUNIT_TEST( PlugInTest );
    CPPUNIT_TEST_SUITE_END();
    void RedirectReturnTest();
    void ReadTest();
    void WriteTest();
    void WriteVTest();
    void VectorReadTest();
    void VectorWriteTest();
    void VirtualRedirectorTest();
    void PlugInTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( FileTest );

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

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string path = dataPath + "/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat";
  std::string fileUrl = address + "/" + path;

  //----------------------------------------------------------------------------
  // Get the SID manager
  //----------------------------------------------------------------------------
  PostMaster *postMaster = DefaultEnv::GetPostMaster();
  AnyObject   sidMgrObj;
  SIDManager *sidMgr    = 0;
  Status      st;
  st = postMaster->QueryTransport( url, XRootDQuery::SIDManager, sidMgrObj );

  CPPUNIT_ASSERT( st.IsOK() );
  sidMgrObj.Get( sidMgr );

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
  CPPUNIT_ASSERT_XRDST( MessageUtils::SendMessage( url, msg, handler, params, 0 ) );
  XRootDStatus st1 = MessageUtils::WaitForResponse( handler, response );
  delete handler;
  CPPUNIT_ASSERT_XRDST_NOTOK( st1, errRedirect );
  CPPUNIT_ASSERT( !response );
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

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string filePath = dataPath + "/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat";
  std::string fileUrl = address + "/";
  fileUrl += filePath;

  //----------------------------------------------------------------------------
  // Fetch some data and checksum
  //----------------------------------------------------------------------------
  const uint32_t MB = 1024*1024;
  char *buffer1 = new char[40*MB];
  char *buffer2 = new char[40*MB];
  uint32_t bytesRead1 = 0;
  uint32_t bytesRead2 = 0;
  File f;
  StatInfo *stat;

  //----------------------------------------------------------------------------
  // Open the file
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f.Open( fileUrl, OpenFlags::Read ) );

  //----------------------------------------------------------------------------
  // Stat1
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f.Stat( false, stat ) );
  CPPUNIT_ASSERT( stat );
  CPPUNIT_ASSERT( stat->GetSize() == 1048576000 );
  CPPUNIT_ASSERT( stat->TestFlags( StatInfo::IsReadable ) );
  delete stat;
  stat = 0;

  //----------------------------------------------------------------------------
  // Stat2
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f.Stat( true, stat ) );
  CPPUNIT_ASSERT( stat );
  CPPUNIT_ASSERT( stat->GetSize() == 1048576000 );
  CPPUNIT_ASSERT( stat->TestFlags( StatInfo::IsReadable ) );
  delete stat;

  //----------------------------------------------------------------------------
  // Read test
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f.Read( 10*MB, 40*MB, buffer1, bytesRead1 ) );
  CPPUNIT_ASSERT_XRDST( f.Read( 1008576000, 40*MB, buffer2, bytesRead2 ) );
  CPPUNIT_ASSERT( bytesRead1 == 40*MB );
  CPPUNIT_ASSERT( bytesRead2 == 40000000 );

  uint32_t crc = Utils::ComputeCRC32( buffer1, 40*MB );
  CPPUNIT_ASSERT( crc == 3303853367UL );

  crc = Utils::ComputeCRC32( buffer2, 40000000 );
  CPPUNIT_ASSERT( crc == 898701504UL );

  delete [] buffer1;
  delete [] buffer2;

  CPPUNIT_ASSERT_XRDST( f.Close() );

  //----------------------------------------------------------------------------
  // Read ZIP archive test (uncompressed)
  //----------------------------------------------------------------------------
  std::string archiveUrl = address + "/" + dataPath + "/data.zip";

  File zipsrc;
  ZipArchiveReader zip( zipsrc );
  CPPUNIT_ASSERT_XRDST( zip.Open( archiveUrl ) );

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
    uint32_t bytesRead;
    CPPUNIT_ASSERT_XRDST( zip.Read( testset[i].file, testset[i].offset, testset[i].size, testset[i].buffer, bytesRead ) );
    std::string result( testset[i].buffer, bytesRead );
    CPPUNIT_ASSERT( testset[i].expected == result );
  }

  CPPUNIT_ASSERT_XRDST( zip.Close() );
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

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

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

  CPPUNIT_ASSERT( Utils::GetRandomBytes( buffer1, 4*MB ) == 4*MB );
  CPPUNIT_ASSERT( Utils::GetRandomBytes( buffer2, 4*MB ) == 4*MB );
  uint32_t crc1 = Utils::ComputeCRC32( buffer1, 4*MB );
  crc1 = Utils::UpdateCRC32( crc1, buffer2, 4*MB );

  //----------------------------------------------------------------------------
  // Write the data
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( f1.Open( fileUrl, OpenFlags::Delete | OpenFlags::Update,
                           Access::UR | Access::UW ).IsOK() );
  CPPUNIT_ASSERT( f1.Write( 0, 4*MB, buffer1 ).IsOK() );
  CPPUNIT_ASSERT( f1.Write( 4*MB, 4*MB, buffer2 ).IsOK() );
  CPPUNIT_ASSERT( f1.Sync().IsOK() );
  CPPUNIT_ASSERT( f1.Close().IsOK() );

  //----------------------------------------------------------------------------
  // Read the data and verify the checksums
  //----------------------------------------------------------------------------
  StatInfo *stat = 0;
  CPPUNIT_ASSERT( f2.Open( fileUrl, OpenFlags::Read ).IsOK() );
  CPPUNIT_ASSERT( f2.Stat( false, stat ).IsOK() );
  CPPUNIT_ASSERT( stat );
  CPPUNIT_ASSERT( stat->GetSize() == 8*MB );
  CPPUNIT_ASSERT( f2.Read( 0, 4*MB, buffer3, bytesRead1 ).IsOK() );
  CPPUNIT_ASSERT( f2.Read( 4*MB, 4*MB, buffer4, bytesRead2 ).IsOK() );
  CPPUNIT_ASSERT( bytesRead1 == 4*MB );
  CPPUNIT_ASSERT( bytesRead2 == 4*MB );
  uint32_t crc2 = Utils::ComputeCRC32( buffer3, 4*MB );
  crc2 = Utils::UpdateCRC32( crc2, buffer4, 4*MB );
  CPPUNIT_ASSERT( f2.Close().IsOK() );
  CPPUNIT_ASSERT( crc1 == crc2 );

  //----------------------------------------------------------------------------
  // Truncate test
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( f1.Open( fileUrl, OpenFlags::Delete | OpenFlags::Update,
                           Access::UR | Access::UW ).IsOK() );
  CPPUNIT_ASSERT( f1.Truncate( 20*MB ).IsOK() );
  CPPUNIT_ASSERT( f1.Close().IsOK() );
  FileSystem fs( url );
  StatInfo *response = 0;
  CPPUNIT_ASSERT( fs.Stat( filePath, response ).IsOK() );
  CPPUNIT_ASSERT( response );
  CPPUNIT_ASSERT( response->GetSize() == 20*MB );
  CPPUNIT_ASSERT( fs.Rm( filePath ).IsOK() );
  delete [] buffer1;
  delete [] buffer2;
  delete [] buffer3;
  delete [] buffer4;
  delete response;
  delete stat;
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

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

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

  CPPUNIT_ASSERT( Utils::GetRandomBytes( buffer1, 4*MB ) == 4*MB );
  CPPUNIT_ASSERT( Utils::GetRandomBytes( buffer2, 4*MB ) == 4*MB );
  uint32_t crc1 = Utils::ComputeCRC32( buffer1, 4*MB );
  crc1 = Utils::UpdateCRC32( crc1, buffer2, 4*MB );

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
  CPPUNIT_ASSERT( f1.Open( fileUrl, OpenFlags::Delete | OpenFlags::Update,
                           Access::UR | Access::UW ).IsOK() );
  CPPUNIT_ASSERT( f1.WriteV( 0, iov, iovcnt ).IsOK() );
  CPPUNIT_ASSERT( f1.Sync().IsOK() );
  CPPUNIT_ASSERT( f1.Close().IsOK() );

  //----------------------------------------------------------------------------
  // Read the data and verify the checksums
  //----------------------------------------------------------------------------
  StatInfo *stat = 0;
  CPPUNIT_ASSERT( f2.Open( fileUrl, OpenFlags::Read ).IsOK() );
  CPPUNIT_ASSERT( f2.Stat( false, stat ).IsOK() );
  CPPUNIT_ASSERT( stat );
  CPPUNIT_ASSERT( stat->GetSize() == 8*MB );
  CPPUNIT_ASSERT( f2.Read( 0, 8*MB, buffer3, bytesRead1 ).IsOK() );
  CPPUNIT_ASSERT( bytesRead1 == 8*MB );

  uint32_t crc2 = Utils::ComputeCRC32( buffer3, 8*MB );
  CPPUNIT_ASSERT( f2.Close().IsOK() );
  CPPUNIT_ASSERT( crc1 == crc2 );

  FileSystem fs( url );
  CPPUNIT_ASSERT( fs.Rm( filePath ).IsOK() );
  delete [] buffer1;
  delete [] buffer2;
  delete [] buffer3;
  delete stat;
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

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string filePath = dataPath + "/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat";
  std::string fileUrl = address + "/";
  fileUrl += filePath;

  //----------------------------------------------------------------------------
  // Fetch some data and checksum
  //----------------------------------------------------------------------------
  const uint32_t MB = 1024*1024;
  char *buffer1 = new char[40*MB];
  char *buffer2 = new char[40*256000];
  File f;

  //----------------------------------------------------------------------------
  // Build the chunk list
  //----------------------------------------------------------------------------
  ChunkList chunkList1;
  ChunkList chunkList2;
  for( int i = 0; i < 40; ++i )
  {
    chunkList1.push_back( ChunkInfo( (i+1)*10*MB, 1*MB ) );
    chunkList2.push_back( ChunkInfo( (i+1)*10*MB, 256000 ) );
  }

  //----------------------------------------------------------------------------
  // Open the file
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f.Open( fileUrl, OpenFlags::Read ) );
  VectorReadInfo *info = 0;
  CPPUNIT_ASSERT_XRDST( f.VectorRead( chunkList1, buffer1, info ) );
  CPPUNIT_ASSERT( info->GetSize() == 40*MB );
  delete info;
  uint32_t crc = 0;
  crc = Utils::ComputeCRC32( buffer1, 40*MB );
  CPPUNIT_ASSERT( crc == 3695956670UL );

  info = 0;
  CPPUNIT_ASSERT_XRDST( f.VectorRead( chunkList2, buffer2, info ) );
  CPPUNIT_ASSERT( info->GetSize() == 40*256000 );
  delete info;
  crc = Utils::ComputeCRC32( buffer2, 40*256000 );
  CPPUNIT_ASSERT( crc == 3492603530UL );

  CPPUNIT_ASSERT_XRDST( f.Close() );

  delete [] buffer1;
  delete [] buffer2;
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

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string filePath = dataPath + "/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat";
  std::string fileUrl = address + "/";
  fileUrl += filePath;

  //----------------------------------------------------------------------------
  // Build a random chunk list for vector write
  //----------------------------------------------------------------------------

  const uint32_t MB = 1024*1024;
  const uint32_t GB = 1000*MB; // maybe that's not 100% precise but that's
                               // what we have in our testbed

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
    size_t offset = min_offset + rand() % ( GB - min_offset + 1 );

    // figure out the size
    size_t size = MB + rand() % ( MB + 1 );
    if( offset + size >= GB )
      size = GB - offset;

    // generate random string of given size
    char *buffer = new char[size];
    gen_random_str( buffer, size );

    // calculate expected checksum
    expectedCrc32 = Utils::UpdateCRC32( expectedCrc32, buffer, size );
    totalSize += size;
    chunks.push_back( XrdCl::ChunkInfo( offset, size, buffer ) );

    min_offset = offset + size;
    if( min_offset >= GB )
      break;
  }

  //----------------------------------------------------------------------------
  // Open the file
  //----------------------------------------------------------------------------
  File f;
  CPPUNIT_ASSERT_XRDST( f.Open( fileUrl, OpenFlags::Update ) );

  //----------------------------------------------------------------------------
  // First do a VectorRead so we can revert to the original state
  //----------------------------------------------------------------------------
  char *buffer1 = new char[totalSize];
  VectorReadInfo *info1 = 0;
  CPPUNIT_ASSERT_XRDST( f.VectorRead( chunks, buffer1, info1 ) );

  //----------------------------------------------------------------------------
  // Then do the VectorWrite
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f.VectorWrite( chunks ) );

  //----------------------------------------------------------------------------
  // Now do a vector read and verify that the checksum is the same
  //----------------------------------------------------------------------------
  char *buffer2 = new char[totalSize];
  VectorReadInfo *info2 = 0;
  CPPUNIT_ASSERT_XRDST( f.VectorRead( chunks, buffer2, info2 ) );

  CPPUNIT_ASSERT( info2->GetSize() == totalSize );
  uint32_t crc32 = Utils::ComputeCRC32( buffer2, totalSize );
  CPPUNIT_ASSERT( crc32 == expectedCrc32 );

  //----------------------------------------------------------------------------
  // And finally revert to the original state
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f.VectorWrite( info1->GetChunks() ) );
  CPPUNIT_ASSERT_XRDST( f.Close() );

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

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string mlUrl1 = address + "/" + dataPath + "/metalink/mlFileTest1.meta4";
  std::string mlUrl2 = address + "/" + dataPath + "/metalink/mlFileTest2.meta4";
  std::string mlUrl3 = address + "/" + dataPath + "/metalink/mlFileTest3.meta4";
  std::string mlUrl4 = address + "/" + dataPath + "/metalink/mlFileTest4.meta4";

  File f1, f2, f3, f4;

  const std::string fileUrl = "root://srv1:1094//data/a048e67f-4397-4bb8-85eb-8d7e40d90763.dat";
  const std::string key = "LastURL";
  std::string value;

  //----------------------------------------------------------------------------
  // Open the 1st metalink file
  // (the metalink contains just one file with a correct location)
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f1.Open( mlUrl1, OpenFlags::Read ) );
  CPPUNIT_ASSERT( f1.GetProperty( key, value ) );
  URL lastUrl( value );
  CPPUNIT_ASSERT( lastUrl.GetLocation() == fileUrl );
  CPPUNIT_ASSERT_XRDST( f1.Close() );

  //----------------------------------------------------------------------------
  // Open the 2nd metalink file
  // (the metalink contains 2 files, the one with higher priority does not exist)
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f2.Open( mlUrl2, OpenFlags::Read ) );
  CPPUNIT_ASSERT( f2.GetProperty( key, value ) );
  URL lastUrl2( value );
  CPPUNIT_ASSERT( lastUrl2.GetLocation() == fileUrl );
  CPPUNIT_ASSERT_XRDST( f2.Close() );

  //----------------------------------------------------------------------------
  // Open the 3rd metalink file
  // (the metalink contains 2 files, both don't exist)
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST_NOTOK( f3.Open( mlUrl3, OpenFlags::Read ), errErrorResponse );

  //----------------------------------------------------------------------------
  // Open the 4th metalink file
  // (the metalink contains 2 files, both exist)
  //----------------------------------------------------------------------------
  const std::string replica1 = "root://srv3:1094//data/3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat";
  const std::string replica2 = "root://srv2:1094//data/3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat";

  CPPUNIT_ASSERT_XRDST( f4.Open( mlUrl4, OpenFlags::Read ) );
  CPPUNIT_ASSERT( f4.GetProperty( key, value ) );
  URL lastUrl3( value );
  CPPUNIT_ASSERT( lastUrl3.GetLocation() == replica1 );
  CPPUNIT_ASSERT_XRDST( f4.Close() );
  //----------------------------------------------------------------------------
  // Delete the replica that has been selected by the virtual redirector
  //----------------------------------------------------------------------------
  FileSystem fs( replica1 );
  CPPUNIT_ASSERT_XRDST( fs.Rm( "/data/3c9a9dd8-bc75-422c-b12c-f00604486cc1.dat" ) );
  //----------------------------------------------------------------------------
  // Now reopen the file
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f4.Open( mlUrl4, OpenFlags::Read ) );
  CPPUNIT_ASSERT( f4.GetProperty( key, value ) );
  URL lastUrl4( value );
  CPPUNIT_ASSERT( lastUrl4.GetLocation() == replica2 );
  CPPUNIT_ASSERT_XRDST( f4.Close() );
  //----------------------------------------------------------------------------
  // Recreate the deleted file
  //----------------------------------------------------------------------------
  CopyProcess  process;
  PropertyList properties, results;
  properties.Set( "source",       replica2 );
  properties.Set( "target",       replica1 );
  CPPUNIT_ASSERT_XRDST( process.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process.Prepare() );
  CPPUNIT_ASSERT_XRDST( process.Run(0) );
}

//------------------------------------------------------------------------------
// Plug-in test
//------------------------------------------------------------------------------
void FileTest::PlugInTest()
{
  XrdCl::PlugInFactory *f = new IdentityFactory;
  XrdCl::DefaultEnv::GetPlugInManager()->RegisterDefaultFactory(f);
  RedirectReturnTest();
  ReadTest();
  WriteTest();
  VectorReadTest();
  XrdCl::DefaultEnv::GetPlugInManager()->RegisterDefaultFactory(0);
}
