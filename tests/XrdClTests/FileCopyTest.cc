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
  std::string lastUrl;
  CPPUNIT_ASSERT( f.GetProperty( "LastURL", lastUrl ) );
  CPPUNIT_ASSERT_XRDST( Utils::GetRemoteCheckSum( remoteSum, "zcrc32",
                                                  URL( lastUrl ) ) );
  CPPUNIT_ASSERT( remoteSum == transferSum );

  delete stat;
  delete crc32Sum;
  delete[] buffer;
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
                                OpenFlags::Delete|OpenFlags::Update ) );

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

  //----------------------------------------------------------------------------
  // Compare the checksums
  //----------------------------------------------------------------------------
  char crcBuff[9];
  XrdCksData crc; crc.Set( (const void *)crc32Sum->Final(), 4 ); crc.Get( crcBuff, 9 );
  std::string transferSum = "zcrc32:"; transferSum += crcBuff;

  std::string remoteSum, lastUrl;
  f.GetProperty( "LastURL", lastUrl );
  CPPUNIT_ASSERT_XRDST( Utils::GetRemoteCheckSum( remoteSum, "zcrc32",
                                                  lastUrl ) );
  CPPUNIT_ASSERT( remoteSum == transferSum );

  //----------------------------------------------------------------------------
  // Delete the file
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( fs.Rm( dataPath + "/testUpload.dat" ) );

  delete stat;
  delete crc32Sum;
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
      virtual void JobProgress( uint16_t jobNum,
                                uint64_t bytesProcessed,
                                uint64_t bytesTotal )
      {
        if( bytesProcessed > 128*1024*1024 )
          pCancel = true;
      }

      //------------------------------------------------------------------------
      // Determine whether the job should be canceled
      //------------------------------------------------------------------------
      virtual bool ShouldCancel( uint16_t jobNum ) { return pCancel; }

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

  std::string metamanager;
  std::string manager1;
  std::string manager2;
  std::string sourceFile;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", metamanager ) );
  CPPUNIT_ASSERT( testEnv->GetString( "Manager1URL",      manager1 ) );
  CPPUNIT_ASSERT( testEnv->GetString( "Manager2URL",      manager2 ) );
  CPPUNIT_ASSERT( testEnv->GetString( "RemoteFile",     sourceFile ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath",         dataPath ) );

  std::string sourceURL    = manager1 + "/" + sourceFile;
  std::string targetPath   = dataPath + "/tpcFile";
  std::string targetURL    = manager2 + "/" + targetPath;
  std::string metalinkURL  = metamanager + "/" + dataPath + "/metalink/mlTpcTest.meta4";
  std::string metalinkURL2 = metamanager + "/" + dataPath + "/metalink/mlZipTest.meta4";
  std::string zipURL       = metamanager + "/" + dataPath + "/data.zip";
  std::string zipURL2      = metamanager + "/" + dataPath + "/large.zip";
  std::string fileInZip    = "paper.txt";
  std::string fileInZip2   = "bible.txt";
  std::string xcpSourceURL = metamanager + "/" + dataPath + "/1db882c8-8cd6-4df1-941f-ce669bad3458.dat";
  std::string localFile    = "/data/localfile.dat";

  CopyProcess  process1, process2, process3, process4, process5, process6, process7, process8, process9,
               process10, process11, process12, process13, process14, process15, process16, process17;
  PropertyList properties, results;
  FileSystem fs( manager2 );

  //----------------------------------------------------------------------------
  // Copy from a ZIP archive
  //----------------------------------------------------------------------------
  if( !thirdParty )
  {
    results.Clear();
    properties.Set( "source",       zipURL    );
    properties.Set( "target",       targetURL );
    properties.Set( "zipArchive",   true      );
    properties.Set( "zipSource",    fileInZip );
    CPPUNIT_ASSERT_XRDST( process6.AddJob( properties, &results ) );
    CPPUNIT_ASSERT_XRDST( process6.Prepare() );
    CPPUNIT_ASSERT_XRDST( process6.Run(0) );
    CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );
    properties.Clear();

    //--------------------------------------------------------------------------
    // Copy from a ZIP archive (compressed) and validate the zcrc32 checksum
    //--------------------------------------------------------------------------
    results.Clear();
    properties.Set( "source",       zipURL2 );
    properties.Set( "target",       targetURL );
    properties.Set( "checkSumMode",   "end2end"     );
    properties.Set( "checkSumType",   "zcrc32"      );
    properties.Set( "zipArchive",   true      );
    properties.Set( "zipSource",    fileInZip2 );
    CPPUNIT_ASSERT_XRDST( process10.AddJob( properties, &results ) );
    CPPUNIT_ASSERT_XRDST( process10.Prepare() );
    CPPUNIT_ASSERT_XRDST( process10.Run(0) );
    CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );
    properties.Clear();

    //--------------------------------------------------------------------------
    // Copy with `--rm-bad-cksum`
    //--------------------------------------------------------------------------
    results.Clear();
    properties.Set( "source",         sourceURL );
    properties.Set( "target",         targetURL );
    properties.Set( "checkSumMode",   "end2end" );
    properties.Set( "checkSumType",   "auto"    );
    properties.Set( "checkSumPreset", "bad-value" ); //< provide wrong checksum value, so the check fails and the file gets removed
    properties.Set( "rmOnBadCksum",   true        );
    CPPUNIT_ASSERT_XRDST( process12.AddJob( properties, &results ) );
    CPPUNIT_ASSERT_XRDST( process12.Prepare() );
    CPPUNIT_ASSERT_XRDST_NOTOK( process12.Run(0), XrdCl::errCheckSumError );
    XrdCl::StatInfo *info = 0;
    XrdCl::XRootDStatus status = fs.Stat( targetPath, info );
    CPPUNIT_ASSERT_XRDST( status.status == XrdCl::stError && status.code == XrdCl::errNotFound );
    properties.Clear();

    //--------------------------------------------------------------------------
    // Copy with `--zip-mtln-cksum`
    //--------------------------------------------------------------------------
    results.Clear();
    properties.Set( "source",         metalinkURL2 );
    properties.Set( "target",         targetURL    );
    properties.Set( "checkSumMode",   "end2end"    );
    properties.Set( "checkSumType",   "zcrc32"     );
    XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
    env->PutInt( "ZipMtlnCksum", 1 );
    CPPUNIT_ASSERT_XRDST( process13.AddJob( properties, &results ) );
    CPPUNIT_ASSERT_XRDST( process13.Prepare() );
    CPPUNIT_ASSERT_XRDST_NOTOK( process13.Run(0), XrdCl::errCheckSumError );
    env->PutInt( "ZipMtlnCksum", 0 );
    CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );

    //--------------------------------------------------------------------------
    // Copy with
    //   `--xrate`
    //   `--xrate-threshold`
    //--------------------------------------------------------------------------
    results.Clear();
    properties.Clear();
    properties.Set( "source",          sourceURL        );
    properties.Set( "target",          targetURL        );
    properties.Set( "xrate",           1024 * 1024 * 32 ); //< limit the transfer rate to 32MB/s
    properties.Set( "xrateThreshold", 1024 * 1024 * 30 ); //< fail the job if the transfer rate drops under 30MB/s
    CPPUNIT_ASSERT_XRDST( process14.AddJob( properties, &results ) );
    CPPUNIT_ASSERT_XRDST( process14.Prepare() );
    CPPUNIT_ASSERT_XRDST( process14.Run(0) );
    CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );

    //--------------------------------------------------------------------------
    // Now test the cp-timeout
    //--------------------------------------------------------------------------
    results.Clear();
    properties.Clear();
    properties.Set( "source",    sourceURL   );
    properties.Set( "target",    targetURL   );
    properties.Set( "xrate",     1024 * 1024 ); //< limit the transfer rate to 1MB/s (the file is 1GB big so the transfer will take 1024 seconds)
    properties.Set( "cpTimeout", 10          ); //< timeout the job after 10 seconds
    CPPUNIT_ASSERT_XRDST( process15.AddJob( properties, &results ) );
    CPPUNIT_ASSERT_XRDST( process15.Prepare() );
    CPPUNIT_ASSERT_XRDST_NOTOK( process15.Run(0), XrdCl::errOperationExpired );
    CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );

    //--------------------------------------------------------------------------
    // Test posc for local files
    //--------------------------------------------------------------------------
    results.Clear();
    properties.Clear();
    std::string localtrg = "file://localhost/data/tpcFile.dat";
    properties.Set( "source",    sourceURL );
    properties.Set( "target",    localtrg  );
    properties.Set( "posc",      true      );
    CancelProgressHandler progress16; //> abort the copy after 100MB
    CPPUNIT_ASSERT_XRDST( process16.AddJob( properties, &results ) );
    CPPUNIT_ASSERT_XRDST( process16.Prepare() );
    CPPUNIT_ASSERT_XRDST_NOTOK( process16.Run( &progress16 ), errOperationInterrupted );
    XrdCl::FileSystem localfs( "file://localhost" );
    XrdCl::StatInfo *ptr = nullptr;
    CPPUNIT_ASSERT_XRDST_NOTOK( localfs.Stat( "/data/tpcFile.dat", ptr ), XrdCl::errLocalError );

    //--------------------------------------------------------------------------
    // Test --retry and --retry-policy
    //--------------------------------------------------------------------------
    results.Clear();
    properties.Clear();
    properties.Set( "xrate",     1024 * 1024 * 32 ); //< limit the transfer rate to 32MB/s
    properties.Set( "cpTimeout", 20               ); //< timeout the job after 20 seconds
    properties.Set( "source",    sourceURL        );
    properties.Set( "target",    targetURL        );
    env->PutInt( "CpRetry", 1 );
    env->PutString( "CpRetryPolicy", "continue" );
    CPPUNIT_ASSERT_XRDST( process17.AddJob( properties, &results ) );
    CPPUNIT_ASSERT_XRDST( process17.Prepare() );
    CPPUNIT_ASSERT_XRDST( process17.Run(0) );
    CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );
    env->PutInt( "CpRetry", XrdCl::DefaultCpRetry );
    env->PutString( "CpRetryPolicy", XrdCl::DefaultCpRetryPolicy );
  }

  //----------------------------------------------------------------------------
  // Copy from a Metalink
  //----------------------------------------------------------------------------
  results.Clear();
  properties.Clear();
  properties.Set( "source",       metalinkURL );
  properties.Set( "target",       targetURL   );
  properties.Set( "checkSumMode", "end2end"   );
  properties.Set( "checkSumType", "zcrc32"    );
  CPPUNIT_ASSERT_XRDST( process5.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process5.Prepare() );
  CPPUNIT_ASSERT_XRDST( process5.Run(0) );
  CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );
  properties.Clear();

  // XCp test
  results.Clear();
  properties.Set( "source",         xcpSourceURL  );
  properties.Set( "target",         targetURL     );
  properties.Set( "checkSumMode",   "end2end"     );
  properties.Set( "checkSumType",   "zcrc32"      );
  properties.Set( "xcp",            true          );
  properties.Set( "nbXcpSources",   3             );
  CPPUNIT_ASSERT_XRDST( process7.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process7.Prepare() );
  CPPUNIT_ASSERT_XRDST( process7.Run(0) );
  CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );
  properties.Clear();

  //----------------------------------------------------------------------------
  // Copy to local fs
  //----------------------------------------------------------------------------
  results.Clear();
  properties.Set( "source", sourceURL );
  properties.Set( "target", "file://localhost" + localFile );
  properties.Set( "checkSumMode", "end2end" );
  properties.Set( "checkSumType", "zcrc32"  );
  CPPUNIT_ASSERT_XRDST( process8.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process8.Prepare() );
  CPPUNIT_ASSERT_XRDST( process8.Run(0) );
  properties.Clear();

  //----------------------------------------------------------------------------
  // Copy from local fs with extended attributes
  //----------------------------------------------------------------------------

  // set extended attributes in the local source file
  File lf;
  CPPUNIT_ASSERT_XRDST( lf.Open( "file://localhost" + localFile, OpenFlags::Write ) );
  std::vector<xattr_t> attrs; attrs.push_back( xattr_t( "foo", "bar" ) );
  std::vector<XAttrStatus> result;
  CPPUNIT_ASSERT_XRDST( lf.SetXAttr( attrs, result ) );
  CPPUNIT_ASSERT( result.size() == 1 );
  CPPUNIT_ASSERT_XRDST( result.front().status );
  CPPUNIT_ASSERT_XRDST( lf.Close() );

  results.Clear();
  properties.Set( "source", "file://localhost" + localFile );
  properties.Set( "target", targetURL );
  properties.Set( "checkSumMode", "end2end" );
  properties.Set( "checkSumType", "zcrc32"  );
  properties.Set( "preserveXAttr", true );
  CPPUNIT_ASSERT_XRDST( process9.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process9.Prepare() );
  CPPUNIT_ASSERT_XRDST( process9.Run(0) );
  properties.Clear();

  // now test if the xattrs were preserved
  std::vector<XAttr> xattrs;
  CPPUNIT_ASSERT_XRDST( fs.ListXAttr( targetPath, xattrs ) );
  CPPUNIT_ASSERT( xattrs.size() == 1 );
  XAttr &xattr = xattrs.front();
  CPPUNIT_ASSERT_XRDST( xattr.status );
  CPPUNIT_ASSERT( xattr.name == "foo" && xattr.value == "bar" );

  //----------------------------------------------------------------------------
  // Cleanup
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );
  CPPUNIT_ASSERT( remove( localFile.c_str() ) == 0 );

  //----------------------------------------------------------------------------
  // Initialize and run the copy
  //----------------------------------------------------------------------------
  properties.Set( "source",       sourceURL );
  properties.Set( "target",       targetURL );
  properties.Set( "checkSumMode", "end2end" );
  properties.Set( "checkSumType", "zcrc32"  );
  if( thirdParty )
    properties.Set( "thirdParty",   "only"    );
  CPPUNIT_ASSERT_XRDST( process1.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process1.Prepare() );
  CPPUNIT_ASSERT_XRDST( process1.Run(0) );
  CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );
  properties.Clear(); 

  //----------------------------------------------------------------------------
  // Copy with `auto` checksum
  //----------------------------------------------------------------------------
  results.Clear();
  properties.Set( "source",       sourceURL );
  properties.Set( "target",       targetURL );
  properties.Set( "checkSumMode", "end2end" );
  properties.Set( "checkSumType", "auto"    );
  if( thirdParty )
    properties.Set( "thirdParty",   "only"    );
  CPPUNIT_ASSERT_XRDST( process11.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process11.Prepare() );
  CPPUNIT_ASSERT_XRDST( process11.Run(0) );
  CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );
  properties.Clear();

  // the further tests are only valid for third party copy for now
  if( !thirdParty )
    return;

  //----------------------------------------------------------------------------
  // Abort the copy after 100MB
  //----------------------------------------------------------------------------
//  CancelProgressHandler progress;
//  CPPUNIT_ASSERT_XRDST( process2.AddJob( properties, &results ) );
//  CPPUNIT_ASSERT_XRDST( process2.Prepare() );
//  CPPUNIT_ASSERT_XRDST_NOTOK( process2.Run(&progress), errErrorResponse );
//  CPPUNIT_ASSERT_XRDST( fs.Rm( targetPath ) );

  //----------------------------------------------------------------------------
  // Copy from a non-existent source
  //----------------------------------------------------------------------------
  results.Clear();
  properties.Set( "source",      "root://localhost:9999//test" );
  properties.Set( "target",      targetURL );
  properties.Set( "initTimeout", 10 );
  properties.Set( "thirdParty",  "only"    );
  CPPUNIT_ASSERT_XRDST( process3.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process3.Prepare() );
  XrdCl::XRootDStatus status = process3.Run(0);
  CPPUNIT_ASSERT( !status.IsOK() && ( status.code == errOperationExpired || status.code == errConnectionError ) );

  //----------------------------------------------------------------------------
  // Copy to a non-existent target
  //----------------------------------------------------------------------------
  results.Clear();
  properties.Set( "source",      sourceURL );
  properties.Set( "target",      "root://localhost:9999//test" );
  properties.Set( "initTimeout", 10 );
  properties.Set( "thirdParty",  "only"    );
  CPPUNIT_ASSERT_XRDST( process4.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process4.Prepare() );
  status = process4.Run(0);
  CPPUNIT_ASSERT( !status.IsOK() && ( status.code == errOperationExpired || status.code == errConnectionError ) );
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
