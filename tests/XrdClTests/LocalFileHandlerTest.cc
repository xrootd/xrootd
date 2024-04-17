//------------------------------------------------------------------------------
// Copyright (c) 2017 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH 
// Author: Paul-Niklas Kramp <p.n.kramp@gsi.de>
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
#include "CppUnitXrdHelpers.hh"
#include "XrdCl/XrdClFile.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class LocalFileHandlerTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( LocalFileHandlerTest );
      CPPUNIT_TEST( OpenCloseTest );
      CPPUNIT_TEST( ReadTest );
      CPPUNIT_TEST( ReadWithOffsetTest );
      CPPUNIT_TEST( WriteTest );
      CPPUNIT_TEST( WriteWithOffsetTest );
      CPPUNIT_TEST( WriteMkdirTest );
      CPPUNIT_TEST( TruncateTest );
      CPPUNIT_TEST( VectorReadTest );
      CPPUNIT_TEST( VectorWriteTest );
      CPPUNIT_TEST( SyncTest );
      CPPUNIT_TEST( WriteVTest );
      CPPUNIT_TEST( XAttrTest );
    CPPUNIT_TEST_SUITE_END();
    void CreateTestFileFunc( std::string url, std::string content = "GenericTestFile" );
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
};
CPPUNIT_TEST_SUITE_REGISTRATION( LocalFileHandlerTest );

//----------------------------------------------------------------------------
// Create the file to be tested
//----------------------------------------------------------------------------
void LocalFileHandlerTest::CreateTestFileFunc( std::string url, std::string content ){
   mode_t openmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
   int fd = open( url.c_str(), O_RDWR | O_CREAT | O_TRUNC, openmode );
   int rc = write( fd, content.c_str(), content.size() );
   CPPUNIT_ASSERT_EQUAL( rc, int( content.size() ) );
   rc = close( fd );
   CPPUNIT_ASSERT_EQUAL( rc, 0 );
}

void LocalFileHandlerTest::SyncTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/lfilehandlertestfilesync";
   CreateTestFileFunc( targetURL );

   //----------------------------------------------------------------------------
   // Open and Sync File
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR | Access::UW | Access::GR | Access::OR;
   File *file = new File();
   CPPUNIT_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   CPPUNIT_ASSERT_XRDST( file->Sync() );
   CPPUNIT_ASSERT_XRDST( file->Close() );
   CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );
   delete file;
}

void LocalFileHandlerTest::OpenCloseTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/lfilehandlertestfileopenclose";
   CreateTestFileFunc( targetURL );

   //----------------------------------------------------------------------------
   // Open existing file
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File *file = new File();
   CPPUNIT_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   CPPUNIT_ASSERT( file->IsOpen() );
   CPPUNIT_ASSERT_XRDST( file->Close() );
   CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );

   //----------------------------------------------------------------------------
   // Try open non-existing file
   //----------------------------------------------------------------------------
   CPPUNIT_ASSERT( !file->Open( targetURL, flags, mode ).IsOK() );
   CPPUNIT_ASSERT( !file->IsOpen() );

   //----------------------------------------------------------------------------
   // Try close non-opened file, return has to be error
   //----------------------------------------------------------------------------
   CPPUNIT_ASSERT( file->Close().status == stError );
   delete file;
}

void LocalFileHandlerTest::WriteTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/lfilehandlertestfilewrite";
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
   CPPUNIT_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   CPPUNIT_ASSERT( file->IsOpen() );
   CPPUNIT_ASSERT_XRDST( file->Write( 0, writeSize, toBeWritten.c_str()) );
   CPPUNIT_ASSERT_XRDST( file->Close() );

   //----------------------------------------------------------------------------
   // Read file with POSIX calls to confirm correct write
   //----------------------------------------------------------------------------
   int fd = open( targetURL.c_str(), O_RDWR );
   int rc = read( fd, buffer, int( writeSize ) );
   CPPUNIT_ASSERT_EQUAL( rc, int( writeSize ) );
   std::string read( (char *)buffer, writeSize );
   CPPUNIT_ASSERT( toBeWritten == read );
   CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );
   delete[] buffer;
   delete file;
}

void LocalFileHandlerTest::WriteWithOffsetTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/lfilehandlertestfilewriteoffset";
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
   CPPUNIT_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   CPPUNIT_ASSERT( file->IsOpen() );
   CPPUNIT_ASSERT_XRDST( file->Write( offset, writeSize, toBeWritten.c_str()) );
   CPPUNIT_ASSERT_XRDST( file->Close() );

   //----------------------------------------------------------------------------
   // Read file with POSIX calls to confirm correct write
   //----------------------------------------------------------------------------
   int fd = open( targetURL.c_str(), O_RDWR );
   int rc = read( fd, buffer, offset );
   CPPUNIT_ASSERT_EQUAL( rc, int( offset ) );
   std::string read( (char *)buffer, offset );
   CPPUNIT_ASSERT( notToBeOverwritten == read );
   CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );
   delete[] (char*)buffer;
   delete file;
}

void LocalFileHandlerTest::WriteMkdirTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/testdir/further/muchfurther/evenfurther/lfilehandlertestfilewrite";
   std::string toBeWritten = "tenBytes10";
   uint32_t writeSize = toBeWritten.size();
   char *buffer = new char[writeSize];

   //----------------------------------------------------------------------------
   // Open and Write File
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update | OpenFlags::MakePath | OpenFlags::New;
   Access::Mode mode = Access::UR|Access::UW|Access::UX;
   File *file = new File();
   CPPUNIT_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   CPPUNIT_ASSERT( file->IsOpen() );
   CPPUNIT_ASSERT_XRDST( file->Write( 0, writeSize, toBeWritten.c_str()) );
   CPPUNIT_ASSERT_XRDST( file->Close() );

   //----------------------------------------------------------------------------
   // Read file with POSIX calls to confirm correct write
   //----------------------------------------------------------------------------
   int fd = open( targetURL.c_str(), O_RDWR );
   int rc = read( fd, buffer, writeSize );
   CPPUNIT_ASSERT_EQUAL( rc, int( writeSize ) );
   std::string read( buffer, writeSize );
   CPPUNIT_ASSERT( toBeWritten == read );
   CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );
   delete[] buffer;
   delete file;
}

void LocalFileHandlerTest::ReadTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/lfilehandlertestfileread";
   std::string toBeWritten = "tenBytes10";
   uint32_t offset = 0;
   uint32_t writeSize = toBeWritten.size();
   char *buffer = new char[writeSize];
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
   CPPUNIT_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   CPPUNIT_ASSERT( file->IsOpen() );
   CPPUNIT_ASSERT_XRDST( file->Read( offset, writeSize, buffer, bytesRead ) );
   CPPUNIT_ASSERT_XRDST( file->Close() );

   std::string read( (char*)buffer, writeSize );
   CPPUNIT_ASSERT( toBeWritten == read );
   CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );

   delete[] buffer;
   delete file;
}

void LocalFileHandlerTest::ReadWithOffsetTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/lfilehandlertestfileread";
   std::string toBeWritten = "tenBytes10";
   uint32_t offset = 3;
   std::string expectedRead = "Byte";
   uint32_t readsize = expectedRead.size();
   char *buffer = new char[readsize];
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
   CPPUNIT_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   CPPUNIT_ASSERT( file->IsOpen() );
   CPPUNIT_ASSERT_XRDST( file->Read( offset, readsize, buffer, bytesRead ) );
   CPPUNIT_ASSERT_XRDST( file->Close() );

   std::string read( buffer, readsize );
   CPPUNIT_ASSERT( expectedRead == read );
   CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );
   delete[] buffer;
   delete file;
}

void LocalFileHandlerTest::TruncateTest(){
   using namespace XrdCl;
   //----------------------------------------------------------------------------
   // Initialize
   //----------------------------------------------------------------------------
   std::string targetURL = "/tmp/lfilehandlertestfiletruncate";

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
   CPPUNIT_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   CPPUNIT_ASSERT_XRDST( file->Truncate( truncateSize ) );
   char *buffer = new char[truncateSize + 3];
   CPPUNIT_ASSERT_XRDST( file->Read( 0, truncateSize + 3, buffer, bytesRead ) );
   CPPUNIT_ASSERT_EQUAL( truncateSize, bytesRead );
   CPPUNIT_ASSERT_XRDST( file->Close() );
   CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );
   delete file;
   delete[] buffer;
}

void LocalFileHandlerTest::VectorReadTest()
{
   using namespace XrdCl;

   //----------------------------------------------------------------------------
   // Initialize
   //----------------------------------------------------------------------------
   std::string targetURL = "/tmp/lfilehandlertestfilevectorread";
   CreateTestFileFunc( targetURL );
   VectorReadInfo *info = 0;
   ChunkList chunks;

   //----------------------------------------------------------------------------
   // Prepare VectorRead
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File file;
   CPPUNIT_ASSERT_XRDST( file.Open( targetURL, flags, mode ) );

   //----------------------------------------------------------------------------
   // VectorRead no cursor
   //----------------------------------------------------------------------------

   chunks.push_back( ChunkInfo( 0, 5, new char[5] ) );
   chunks.push_back( ChunkInfo( 10, 5, new char[5] ) );
   CPPUNIT_ASSERT_XRDST( file.VectorRead( chunks, NULL, info ) );
   CPPUNIT_ASSERT_XRDST( file.Close() );
   CPPUNIT_ASSERT( info->GetSize() == 10 );
   CPPUNIT_ASSERT_EQUAL( 0, memcmp( "Gener",
                                    info->GetChunks()[0].buffer,
                                    info->GetChunks()[0].length ) );
   CPPUNIT_ASSERT_EQUAL( 0, memcmp( "tFile",
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

   CPPUNIT_ASSERT_XRDST( file.Open( targetURL, flags, mode ) );
   CPPUNIT_ASSERT_XRDST( file.VectorRead( chunks, buffer, info ) );
   CPPUNIT_ASSERT_XRDST( file.Close() );
   CPPUNIT_ASSERT( info->GetSize() == 10 );
   CPPUNIT_ASSERT_EQUAL( 0, memcmp( "GenertFile",
                                    info->GetChunks()[0].buffer,
                                    info->GetChunks()[0].length ) );
   CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );

   delete[] buffer;
   delete info;
}

void LocalFileHandlerTest::VectorWriteTest()
{
   using namespace XrdCl;

   //----------------------------------------------------------------------------
   // Initialize
   //----------------------------------------------------------------------------
   std::string targetURL = "/tmp/lfilehandlertestfilevectorwrite";
   CreateTestFileFunc( targetURL );
   ChunkList chunks;

   //----------------------------------------------------------------------------
   // Prepare VectorWrite
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File file;
   CPPUNIT_ASSERT_XRDST( file.Open( targetURL, flags, mode ) );

   //----------------------------------------------------------------------------
   // VectorWrite
   //----------------------------------------------------------------------------

   ChunkInfo chunk( 0, 5, new char[5] );
   memset( chunk.buffer, 'A', chunk.length );
   chunks.push_back( chunk );
   chunk = ChunkInfo( 10, 5, new char[5] );
   memset( chunk.buffer, 'B', chunk.length );
   chunks.push_back( chunk );

   CPPUNIT_ASSERT_XRDST( file.VectorWrite( chunks ) );

   //----------------------------------------------------------------------------
   // Verify with VectorRead
   //----------------------------------------------------------------------------

   VectorReadInfo *info = 0;
   char *buffer = new char[10];
   CPPUNIT_ASSERT_XRDST( file.VectorRead( chunks, buffer, info ) );

   CPPUNIT_ASSERT_EQUAL( 0, memcmp( buffer, "AAAAABBBBB", 10 ) );

   CPPUNIT_ASSERT_XRDST( file.Close() );
   CPPUNIT_ASSERT( info->GetSize() == 10 );

   delete[] (char*)chunks[0].buffer;
   delete[] (char*)chunks[1].buffer;
   delete[] buffer;
   delete   info;

   CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );
}

void LocalFileHandlerTest::WriteVTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  std::string targetURL = "/tmp/lfilehandlertestfilewritev";
  CreateTestFileFunc( targetURL );

  //----------------------------------------------------------------------------
  // Prepare WriteV
  //----------------------------------------------------------------------------
  OpenFlags::Flags flags = OpenFlags::Update;
  Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
  File file;
  CPPUNIT_ASSERT_XRDST( file.Open( targetURL, flags, mode ) );

  char str[] = "WriteVTest";
  std::vector<char> buffer( 10 );
  std::copy( str, str + sizeof( str ) - 1, buffer.begin() );
  int iovcnt = 2;
  iovec iov[iovcnt];
  iov[0].iov_base = buffer.data();
  iov[0].iov_len  = 6;
  iov[1].iov_base = buffer.data() + 6;
  iov[1].iov_len  = 4;
  CPPUNIT_ASSERT_XRDST( file.WriteV( 7, iov, iovcnt ) );

  uint32_t bytesRead = 0;
  buffer.resize( 17 );
  CPPUNIT_ASSERT_XRDST( file.Read( 0, 17, buffer.data(), bytesRead ) );
  CPPUNIT_ASSERT( buffer.size() == 17 );
  std::string expected = "GenericWriteVTest";
  CPPUNIT_ASSERT( std::string( buffer.data(), buffer.size() ) == expected );
  CPPUNIT_ASSERT_XRDST( file.Close() );
  CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );
}

void LocalFileHandlerTest::XAttrTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  // (we do the test in /data as /tmp might be on tpmfs,
  //  which does not support xattrs)
  //----------------------------------------------------------------------------
  std::string targetURL = "/data/lfilehandlertestfilexattr";
  CreateTestFileFunc( targetURL );

  File f;
  CPPUNIT_ASSERT_XRDST( f.Open( targetURL, OpenFlags::Update ) );

  //----------------------------------------------------------------------------
  // Test XAttr Set
  //----------------------------------------------------------------------------
  std::vector<xattr_t> attrs;
  attrs.push_back( xattr_t( "version", "v3.3.3" ) );
  attrs.push_back( xattr_t( "description", "a very important file" ) );
  attrs.push_back( xattr_t( "checksum", "0x22334455" ) );

  std::vector<XAttrStatus> st_resp;

  CPPUNIT_ASSERT_XRDST( f.SetXAttr( attrs, st_resp ) );

  std::vector<XAttrStatus>::iterator itr1;
  for( itr1 = st_resp.begin(); itr1 != st_resp.end(); ++itr1 )
    CPPUNIT_ASSERT_XRDST( itr1->status );

  //----------------------------------------------------------------------------
  // Test XAttr Get
  //----------------------------------------------------------------------------
  std::vector<std::string> names;
  names.push_back( "version" );
  names.push_back( "description" );
  std::vector<XAttr> resp;
  CPPUNIT_ASSERT_XRDST( f.GetXAttr( names, resp ) );

  CPPUNIT_ASSERT_XRDST( resp[0].status );
  CPPUNIT_ASSERT_XRDST( resp[1].status );

  CPPUNIT_ASSERT( resp.size() == 2 );
  int vid = resp[0].name == "version" ? 0 : 1;
  int did = vid == 0 ? 1 : 0;
  CPPUNIT_ASSERT( resp[vid].name == "version" &&
                  resp[vid].value == "v3.3.3" );
  CPPUNIT_ASSERT( resp[did].name == "description" &&
                  resp[did].value == "a very important file" );

  //----------------------------------------------------------------------------
  // Test XAttr Del
  //----------------------------------------------------------------------------
  names.clear();
  names.push_back( "description" );
  st_resp.clear();
  CPPUNIT_ASSERT_XRDST( f.DelXAttr( names, st_resp ) );
  CPPUNIT_ASSERT( st_resp.size() == 1 );
  CPPUNIT_ASSERT_XRDST( st_resp[0].status );

  //----------------------------------------------------------------------------
  // Test XAttr List
  //----------------------------------------------------------------------------
  resp.clear();
  CPPUNIT_ASSERT_XRDST( f.ListXAttr( resp ) );
  CPPUNIT_ASSERT( resp.size() == 2 );
  vid = resp[0].name == "version" ? 0 : 1;
  int cid = vid == 0 ? 1 : 0;
  CPPUNIT_ASSERT( resp[vid].name == "version" &&
                  resp[vid].value == "v3.3.3" );
  CPPUNIT_ASSERT( resp[cid].name == "checksum" &&
                  resp[cid].value == "0x22334455" );

  //----------------------------------------------------------------------------
  // Cleanup
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f.Close() );
  CPPUNIT_ASSERT( remove( targetURL.c_str() ) == 0 );
}
