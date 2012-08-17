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

#include <cppunit/extensions/HelperMacros.h>
#include "TestEnv.hh"
#include "Utils.hh"
#include "CppUnitXrdHelpers.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClXRootDMsgHandler.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class FileCopyTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( FileCopyTest );
      CPPUNIT_TEST( DownloadTest );
      CPPUNIT_TEST( UploadTest );
      CPPUNIT_TEST( MultiStreamDownloadTest );
      CPPUNIT_TEST( MultiStreamUploadTest );
    CPPUNIT_TEST_SUITE_END();
    void DownloadTestFunc();
    void UploadTestFunc();
    void DownloadTest();
    void UploadTest();
    void MultiStreamDownloadTest();
    void MultiStreamUploadTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( FileCopyTest );

//------------------------------------------------------------------------------
// Download test
//------------------------------------------------------------------------------
void FileCopyTest::DownloadTestFunc()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string remoteFile;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "RemoteFile",    remoteFile ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string fileUrl = address + "/" + remoteFile;

  const uint32_t  MB = 1024*1024;
  char           *buffer = new char[4*MB];
  StatInfo       *stat = 0;
  File            f;

  //----------------------------------------------------------------------------
  // Open and stat the file
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( f.Open( fileUrl, OpenFlags::Read ) );

  CPPUNIT_ASSERT_XRDST( f.Stat( false, stat ) );
  CPPUNIT_ASSERT( stat );
  CPPUNIT_ASSERT( stat->TestFlags( StatInfo::IsReadable ) );

  //----------------------------------------------------------------------------
  // Fetch the data
  //----------------------------------------------------------------------------
  uint64_t totalRead = 0;
  uint32_t bytesRead = 0;
  uint32_t computedCRC32 = Utils::GetInitialCRC32();

  while( 1 )
  {
    CPPUNIT_ASSERT_XRDST( f.Read( totalRead, 4*MB, buffer, bytesRead ) );
    if( bytesRead == 0 )
      break;
    totalRead += bytesRead;
    computedCRC32 = Utils::UpdateCRC32( computedCRC32, buffer, bytesRead );
  }

  //----------------------------------------------------------------------------
  // Verify checksums and close the file
  //----------------------------------------------------------------------------
  Buffer  arg;
  Buffer *cksResponse = 0;
  arg.FromString( remoteFile );
  FileSystem fs( url );

  //----------------------------------------------------------------------------
  // Locate a disk server containing the file
  //----------------------------------------------------------------------------
  LocationInfo *locations = 0;
  CPPUNIT_ASSERT_XRDST( fs.DeepLocate( remoteFile, OpenFlags::Refresh, locations ) );
  CPPUNIT_ASSERT( locations );
  CPPUNIT_ASSERT( locations->GetSize() != 0 );
  FileSystem fs1( locations->Begin()->GetAddress() );
  delete locations;

  //----------------------------------------------------------------------------
  // Get the checksum
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( fs1.Query( QueryCode::Checksum,
                                   arg, cksResponse ) );
  CPPUNIT_ASSERT( cksResponse );

  uint32_t remoteCRC32 = 0;
  CPPUNIT_ASSERT( Utils::CRC32TextToInt( remoteCRC32,
                                         cksResponse->ToString() ) );
  CPPUNIT_ASSERT( remoteCRC32 == computedCRC32 );
  CPPUNIT_ASSERT( totalRead == stat->GetSize() );
  CPPUNIT_ASSERT_XRDST( f.Close() );
  delete [] buffer;
  delete stat;
  delete cksResponse;
}

//------------------------------------------------------------------------------
// Upload test
//------------------------------------------------------------------------------
void FileCopyTest::UploadTestFunc()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;
  std::string localFile;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath", dataPath ) );
  CPPUNIT_ASSERT( testEnv->GetString( "LocalFile", localFile ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string fileUrl = address + "/" + dataPath + "/testUpload.dat";
  std::string remoteFile = dataPath + "/testUpload.dat";

  const uint32_t  MB = 1024*1024;
  char           *buffer = new char[4*MB];
  File            f;

  //----------------------------------------------------------------------------
  // Open
  //----------------------------------------------------------------------------
  int fd = open( localFile.c_str(), O_RDONLY );
  CPPUNIT_ASSERT_MESSAGE( strerror(errno), fd != -1 );
  CPPUNIT_ASSERT_XRDST( f.Open( fileUrl,
                                OpenFlags::Delete|OpenFlags::Append ) );

  //----------------------------------------------------------------------------
  // Read the data
  //----------------------------------------------------------------------------
  uint64_t offset        = 0;
  ssize_t  bytesRead;
  uint32_t computedCRC32 = Utils::GetInitialCRC32();

  while( (bytesRead = read( fd, buffer, 4*MB )) > 0 )
  {
    computedCRC32 = Utils::UpdateCRC32( computedCRC32, buffer, bytesRead );
    CPPUNIT_ASSERT_XRDST( f.Write( offset, bytesRead, buffer ) );
    offset += bytesRead;
  }

  CPPUNIT_ASSERT( bytesRead >= 0 );
  close( fd );
  CPPUNIT_ASSERT_XRDST( f.Close() );
  delete [] buffer;

  //----------------------------------------------------------------------------
  // Find out which server has the file
  //----------------------------------------------------------------------------
  FileSystem  fs( url );
  LocationInfo *locations = 0;
  CPPUNIT_ASSERT_XRDST( fs.DeepLocate( remoteFile, OpenFlags::Refresh, locations ) );
  CPPUNIT_ASSERT( locations );
  CPPUNIT_ASSERT( locations->GetSize() != 0 );
  FileSystem fs1( locations->Begin()->GetAddress() );
  delete locations;

  //----------------------------------------------------------------------------
  // Verify the size
  //----------------------------------------------------------------------------
  StatInfo   *stat = 0;
  CPPUNIT_ASSERT_XRDST( fs1.Stat( remoteFile, stat ) );
  CPPUNIT_ASSERT( stat );
  CPPUNIT_ASSERT( stat->GetSize() == offset );
  delete stat;

  //----------------------------------------------------------------------------
  // Verify checksums
  //----------------------------------------------------------------------------
  Buffer  arg;
  Buffer *cksResponse = 0;
  arg.FromString( remoteFile );

  CPPUNIT_ASSERT_XRDST( fs1.Query( QueryCode::Checksum, arg, cksResponse ) );
  CPPUNIT_ASSERT( cksResponse );
  uint32_t remoteCRC32 = 0;
  CPPUNIT_ASSERT( Utils::CRC32TextToInt( remoteCRC32,
                                         cksResponse->ToString() ) );
  CPPUNIT_ASSERT( remoteCRC32 == computedCRC32 );

  delete cksResponse;

  //----------------------------------------------------------------------------
  // Delete the file
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( fs.Rm( dataPath + "/testUpload.dat" ) );
}

//------------------------------------------------------------------------------
// Upload test
//------------------------------------------------------------------------------
void FileCopyTest::UploadTest()
{
  UploadTestFunc();
}

void FileCopyTest::MultiStreamUploadTest()
{
  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
  env->PutInt( "SubStreamsPerChannel", 4 );
  UploadTestFunc();
}

//------------------------------------------------------------------------------
// Download test
//------------------------------------------------------------------------------
void FileCopyTest::DownloadTest()
{
  DownloadTestFunc();
}

void FileCopyTest::MultiStreamDownloadTest()
{
  XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
  env->PutInt( "SubStreamsPerChannel", 4 );
  DownloadTestFunc();
}
