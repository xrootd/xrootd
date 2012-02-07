//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include <cppunit/extensions/HelperMacros.h>
#include "TestEnv.hh"
#include "Utils.hh"
#include "XrdCl/XrdClFile.hh"

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class FileTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( FileTest );
      CPPUNIT_TEST( ReadTest );
    CPPUNIT_TEST_SUITE_END();
    void ReadTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( FileTest );

//------------------------------------------------------------------------------
// Read test
//------------------------------------------------------------------------------
void FileTest::ReadTest()
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

  std::string fileUrl = address + "/";
  fileUrl += dataPath + "/cb4aacf1-6f28-42f2-b68a-90a73460f424.dat";

  //----------------------------------------------------------------------------
  // Fetch some data and checksum
  //----------------------------------------------------------------------------
  const uint32_t MB = 1024*1024;
  char *buffer1 = new char[4*MB];
  char *buffer2 = new char[4*MB];
  File f;
  CPPUNIT_ASSERT( f.Open( fileUrl, OpenFlags::Read ).IsOK() );
  CPPUNIT_ASSERT( f.Read( 10*MB, 4*MB, buffer1 ).IsOK() );
  CPPUNIT_ASSERT( f.Read( 20*MB, 4*MB, buffer2 ).IsOK() );
  uint32_t crc = Utils::ComputeCRC32( buffer1, 4*MB );
  crc = Utils::UpdateCRC32( crc, buffer2, 4*MB );
  CPPUNIT_ASSERT( crc == 450018373 );
  delete [] buffer1;
  delete [] buffer2;
}
