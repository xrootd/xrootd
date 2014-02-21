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
#include "CppUnitXrdHelpers.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClXRootDMsgHandler.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClCheckSumManager.hh"
#include "XrdCl/XrdClCopyProcess.hh"

#include "XrdCks/XrdCks.hh"
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCks/XrdCksData.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace XrdClTests;

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
      CPPUNIT_TEST( ThirdPartyCopyTest );
      CPPUNIT_TEST( NormalCopyTest );
    CPPUNIT_TEST_SUITE_END();
    void DownloadTestFunc();
    void UploadTestFunc();
    void DownloadTest();
    void UploadTest();
    void MultiStreamDownloadTest();
    void MultiStreamUploadTest();
    void CopyTestFunc( bool thirdParty = true );
    void ThirdPartyCopyTest();
    void NormalCopyTest();
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
  uint64_t    totalRead = 0;
  uint32_t    bytesRead = 0;

  CheckSumManager *man      = DefaultEnv::GetCheckSumManager();
  XrdCksCalc      *crc32Sum = man->GetCalculator("zcrc32");
  CPPUNIT_ASSERT( crc32Sum );

  while( 1 )
  {
    CPPUNIT_ASSERT_XRDST( f.Read( totalRead, 4*MB, buffer, bytesRead ) );
    if( bytesRead == 0 )
      break;
    totalRead += bytesRead;
    crc32Sum->Update( buffer, bytesRead );
  }

  //----------------------------------------------------------------------------
  // Compare the checksums
  //----------------------------------------------------------------------------
  char crcBuff[9];
  XrdCksData crc; crc.Set( (const void *)crc32Sum->Final(), 4 ); crc.Get( crcBuff, 9 );
  std::string transferSum = "zcrc32:"; transferSum += crcBuff;

  std::string remoteSum;
  CPPUNIT_ASSERT_XRDST( Utils::GetRemoteCheckSum( remoteSum, "zcrc32",
                                                  f.GetDataServer(),
                                                  remoteFile ) );
  CPPUNIT_ASSERT( remoteSum == transferSum );
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
  int fd = -1;
  CPPUNIT_ASSERT_ERRNO( (fd=open( localFile.c_str(), O_RDONLY )) > 0 )
  CPPUNIT_ASSERT_XRDST( f.Open( fileUrl,
                                OpenFlags::Delete|OpenFlags::Append ) );

  //----------------------------------------------------------------------------
  // Read the data
  //----------------------------------------------------------------------------
  uint64_t offset        = 0;
  ssize_t  bytesRead;

  CheckSumManager *man      = DefaultEnv::GetCheckSumManager();
  XrdCksCalc      *crc32Sum = man->GetCalculator("zcrc32");
  CPPUNIT_ASSERT( crc32Sum );

  while( (bytesRead = read( fd, buffer, 4*MB )) > 0 )
  {
    crc32Sum->Update( buffer, bytesRead );
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
  // Compare the checksums
  //----------------------------------------------------------------------------
  char crcBuff[9];
  XrdCksData crc; crc.Set( (const void *)crc32Sum->Final(), 4 ); crc.Get( crcBuff, 9 );
  std::string transferSum = "zcrc32:"; transferSum += crcBuff;

  std::string remoteSum;
  CPPUNIT_ASSERT_XRDST( Utils::GetRemoteCheckSum( remoteSum, "zcrc32",
                                                  f.GetDataServer(),
                                                  remoteFile ) );
  CPPUNIT_ASSERT( remoteSum == transferSum );

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

namespace
{
  //----------------------------------------------------------------------------
  // Abort handler
  //----------------------------------------------------------------------------
  class CancelProgressHandler: public XrdCl::CopyProgressHandler
  {
    public:
      //------------------------------------------------------------------------
      // Constructor/destructor
      //------------------------------------------------------------------------
      CancelProgressHandler(): pCancel( false ) {}
      virtual ~CancelProgressHandler() {};

      //------------------------------------------------------------------------
      // Job progress
      //------------------------------------------------------------------------
      virtual void JobProgress( uint64_t bytesProcessed,
                                uint64_t bytesTotal )
      {
        if( bytesProcessed > 128*1024*1024 )
          pCancel = true;
      }

      //------------------------------------------------------------------------
      // Determine whether the job should be canceled
      //------------------------------------------------------------------------
      virtual bool ShouldCancel() { return pCancel; }

    private:
      bool pCancel;
};

}

//------------------------------------------------------------------------------
// Third party copy test
//------------------------------------------------------------------------------
void FileCopyTest::CopyTestFunc( bool thirdParty )
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string manager1;
  std::string manager2;
  std::string sourceFile;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "Manager1URL", manager1 ) );
  CPPUNIT_ASSERT( testEnv->GetString( "Manager2URL", manager2 ) );
  CPPUNIT_ASSERT( testEnv->GetString( "RemoteFile",  sourceFile ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath",    dataPath ) );

  std::string sourceURL  = manager1 + "/" + sourceFile;
  std::string targetPath = dataPath + "/tpcFile";
  std::string targetURL  = manager2 + "/" + targetPath;

  //----------------------------------------------------------------------------
  // Initialize and run the copy
  //----------------------------------------------------------------------------
  CopyProcess  process1, process2, process3, process4;
  PropertyList properties, results;
  properties.Set( "source",       sourceURL );
  properties.Set( "target",       targetURL );
  properties.Set( "checkSumMode", "end2end" );
  properties.Set( "checkSumType", "zcrc32"  );

  if( thirdParty )
    properties.Set( "thirdParty",   "only"    );

  CPPUNIT_ASSERT_XRDST( process1.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process1.Prepare() );
  CPPUNIT_ASSERT_XRDST( process1.Run(0) );

  //----------------------------------------------------------------------------
  // Cleanup
  //----------------------------------------------------------------------------
  FileSystem fs( manager2 );
  CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );

  // the further tests are only valid for third party copy for now
  if( !thirdParty )
    return;

  //----------------------------------------------------------------------------
  // Abort the copy after 100MB
  //----------------------------------------------------------------------------
//  CancelProgressHandler progress;
//  CPPUNIT_ASSERT_XRDST( process2.AddJob( properties, &results ) );
//  CPPUNIT_ASSERT_XRDST( process2.Prepare() );
//  CPPUNIT_ASSERT_XRDST( process2.Run(&progress) ); // This really should fail
//                                                   // with errCacnel or sth
//  CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );

  //----------------------------------------------------------------------------
  // Copy from a non-existent source
  //----------------------------------------------------------------------------
  results.Clear();
  properties.Set( "source", "root://localhost:9999//test" );
  properties.Set( "initTimeout", 10 );
  CPPUNIT_ASSERT_XRDST( process3.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST_NOTOK( process3.Prepare(), errOperationExpired );
  CPPUNIT_ASSERT_XRDST( process3.Run(0) );

  //----------------------------------------------------------------------------
  // Copy to a non-existent target
  //----------------------------------------------------------------------------
  results.Clear();
  properties.Set( "source", sourceURL );
  properties.Set( "target", "root://localhost:9999//test" );
  properties.Set( "initTimeout", 10 );
  CPPUNIT_ASSERT_XRDST( process4.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process4.Prepare() );
  CPPUNIT_ASSERT_XRDST_NOTOK( process4.Run(0), errOperationExpired );
}

//------------------------------------------------------------------------------
// Third party copy test
//------------------------------------------------------------------------------
void FileCopyTest::ThirdPartyCopyTest()
{
  CopyTestFunc( true );
}

//------------------------------------------------------------------------------
// Cormal copy test
//------------------------------------------------------------------------------
void FileCopyTest::NormalCopyTest()
{
  CopyTestFunc( false );
}
