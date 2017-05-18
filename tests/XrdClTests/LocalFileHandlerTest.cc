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
      CPPUNIT_TEST( SyncTest );
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
    void SyncTest();
};
CPPUNIT_TEST_SUITE_REGISTRATION( LocalFileHandlerTest );

//----------------------------------------------------------------------------
// Create the file to be tested
//----------------------------------------------------------------------------
void LocalFileHandlerTest::CreateTestFileFunc( std::string url, std::string content ){
   mode_t openmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
   int fd = open( url.c_str(), O_RDWR | O_CREAT | O_TRUNC, openmode );
   write( fd, content.c_str(), content.size() );
   close( fd );
}

void LocalFileHandlerTest::SyncTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/lfilehandlertestfilesync";
   CreateTestFileFunc( targetURL );

   //----------------------------------------------------------------------------
   // Open and Sync File
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File *file = new File();
   CPPUNIT_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   CPPUNIT_ASSERT_XRDST( file->Sync() );
   CPPUNIT_ASSERT_XRDST( file->Close() );
   remove(targetURL.c_str());
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
   remove( targetURL.c_str() );

   //----------------------------------------------------------------------------
   // Try open non-existing file
   //----------------------------------------------------------------------------
   file->Open( targetURL, flags, mode );
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
   char *buffer = new char(writeSize);
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
   int fd = open( targetURL.c_str(), flags );
   read( fd, buffer, writeSize );
   std::string read( (char *)buffer, writeSize );
   CPPUNIT_ASSERT( toBeWritten == read );
   remove(targetURL.c_str());
   delete buffer;
   delete file;
}

void LocalFileHandlerTest::WriteWithOffsetTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/lfilehandlertestfilewriteoffset";
   std::string toBeWritten = "tenBytes10";
   std::string notToBeOverwritten = "front";
   uint32_t writeSize = toBeWritten.size();
   uint32_t offset = notToBeOverwritten.size();
   void *buffer = new char( offset );
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
   int fd = open( targetURL.c_str(), flags );
   read( fd, buffer, offset );
   std::string read( (char *)buffer, offset );
   CPPUNIT_ASSERT( notToBeOverwritten == read );
   remove(targetURL.c_str());
   delete (char*)buffer;
   delete file;
}

void LocalFileHandlerTest::WriteMkdirTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/testdir/further/muchfurther/evenfurther/lfilehandlertestfilewrite";
   std::string toBeWritten = "tenBytes10";
   uint32_t writeSize = toBeWritten.size();
   char *buffer = new char( writeSize );

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
   int fd = open( targetURL.c_str(), flags );
   read( fd, buffer, writeSize );
   std::string read( buffer, writeSize );
   CPPUNIT_ASSERT( toBeWritten == read );
   remove(targetURL.c_str());
   delete buffer;
   delete file;
}

void LocalFileHandlerTest::ReadTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/lfilehandlertestfileread";
   std::string toBeWritten = "tenBytes10";
   uint32_t offset = 0;
   uint32_t writeSize = toBeWritten.size();
   char *buffer = new char(writeSize);
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
   remove( targetURL.c_str() );

   delete buffer;
   delete file;
}

void LocalFileHandlerTest::ReadWithOffsetTest(){
   using namespace XrdCl;
   std::string targetURL = "/tmp/lfilehandlertestfileread";
   std::string toBeWritten = "tenBytes10";
   uint32_t offset = 3;
   std::string expectedRead = "Byte";
   uint32_t readsize = expectedRead.size();
   char *buffer = new char( readsize );
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
   remove( targetURL.c_str() );
   delete buffer;
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
   file->Open( targetURL, flags, mode );
   file->Truncate( truncateSize );
   char *buffer = new char( truncateSize + 3 );
   file->Read( 0, truncateSize + 3, buffer, bytesRead );
   CPPUNIT_ASSERT_EQUAL( truncateSize, bytesRead );
   file->Close();
   remove(targetURL.c_str());
   delete file;
   delete buffer;
}

void LocalFileHandlerTest::VectorReadTest(){
   using namespace XrdCl;

   //----------------------------------------------------------------------------
   // Initialize
   //----------------------------------------------------------------------------
   std::string targetURL = "/tmp/lfilehandlertestfilevectorread";
   CreateTestFileFunc( targetURL );
   VectorReadInfo *info = new VectorReadInfo();
   ChunkList chunks;

   //----------------------------------------------------------------------------
   // Prepare VectorRead
   //----------------------------------------------------------------------------
   OpenFlags::Flags flags = OpenFlags::Update;
   Access::Mode mode = Access::UR|Access::UW|Access::GR|Access::OR;
   File *file = new File();
   CPPUNIT_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );

   //----------------------------------------------------------------------------
   // VectorRead no cursor
   //----------------------------------------------------------------------------

   char *buffer1 = new char( 5 );
   char *buffer2 = new char( 5 );
   char *buffer3 = new char( 5 );
   ChunkInfo cinf( 0, 5, buffer1 );
   chunks.push_back( cinf );
   ChunkInfo cinf2( 5, 5, buffer2 );
   chunks.push_back( cinf2 );
   ChunkInfo cinf3( 10, 5, buffer3 );
   chunks.push_back( cinf3 );

   CPPUNIT_ASSERT_XRDST( file->VectorRead( chunks, NULL, info ) );
   CPPUNIT_ASSERT_XRDST( file->Close() );

   //----------------------------------------------------------------------------
   // VectorRead cursor
   //----------------------------------------------------------------------------
   char *buffer = new char( 15 );
   chunks.clear();
   char *buffer4 = new char( 5 );
   char *buffer5 = new char( 5 );
   char *buffer6 = new char( 5 );
   ChunkInfo cinf4( 0, 5, buffer4 );
   chunks.push_back( cinf4 );

   ChunkInfo cinf5( 5, 5, buffer5 );
   chunks.push_back( cinf5 );

   ChunkInfo cinf6( 10, 5, buffer6 );
   chunks.push_back( cinf6 );
   CPPUNIT_ASSERT_XRDST( file->Open( targetURL, flags, mode ) );
   CPPUNIT_ASSERT_XRDST( file->VectorRead( chunks, buffer, info ) );
   CPPUNIT_ASSERT_XRDST( file->Close() );

   remove( targetURL.c_str() );
   chunks.clear();
   delete buffer;
   delete buffer1;
   delete buffer2;
   delete buffer3;
   delete buffer4;
   delete buffer5;
   delete buffer6;
   delete file;
   delete info;
}
