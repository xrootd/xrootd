//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include <cppunit/extensions/HelperMacros.h>
#include "TestEnv.hh"
#include "Utils.hh"
#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClPostMaster.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClMessageUtils.hh"
#include "XrdCl/XrdClXRootDMsgHandler.hh"

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class FileCopyTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( FileCopyTest );
      CPPUNIT_TEST( DownloadTest );
      CPPUNIT_TEST( UploadTest );
    CPPUNIT_TEST_SUITE_END();
    void DownloadTest();
    void UploadTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( FileCopyTest );

//------------------------------------------------------------------------------
// Download test
//------------------------------------------------------------------------------
void FileCopyTest::DownloadTest()
{
  using namespace XrdClient;

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

  const uint32_t  MB = 1024*1024;
  char            buffer[4*MB];
  StatInfo       *stat = 0;
  File            f;

  //----------------------------------------------------------------------------
  // Open and stat the file
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( f.Open( fileUrl, OpenFlags::Read ).IsOK() );

  CPPUNIT_ASSERT( f.Stat( false, stat ).IsOK() );
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
    CPPUNIT_ASSERT( f.Read( totalRead, 4*MB, buffer, bytesRead ).IsOK() );
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
  arg.FromString( path );
  Query query( url );
  XRootDStatus st = query.ServerQuery( QueryCode::Checksum, arg, cksResponse );
  CPPUNIT_ASSERT( st.IsOK() );
  CPPUNIT_ASSERT( cksResponse );

  uint32_t remoteCRC32 = 0;
  CPPUNIT_ASSERT( Utils::CRC32TextToInt( remoteCRC32,
                                         cksResponse->ToString() ) );
  CPPUNIT_ASSERT( remoteCRC32 == computedCRC32 );
  CPPUNIT_ASSERT( totalRead == stat->GetSize() );
  CPPUNIT_ASSERT( f.Close().IsOK() );
  delete stat;
}

//------------------------------------------------------------------------------
// Read test
//------------------------------------------------------------------------------
void FileCopyTest::UploadTest()
{
  using namespace XrdClient;
}
