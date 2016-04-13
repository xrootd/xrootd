/*
 * MetalinkTest.cc
 *
 *  Created on: Feb 23, 2016
 *      Author: simonm
 */



#include <cppunit/extensions/HelperMacros.h>
#include "TestEnv.hh"
#include "CppUnitXrdHelpers.hh"

#include "XrdCl/XrdClCopyProcess.hh"
#include "XrdCl/XrdClFileSystem.hh"

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class MetalinkTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( MetalinkTest );
      CPPUNIT_TEST( CopySimpleTest );
      CPPUNIT_TEST( Copy2SourcesTest );
      CPPUNIT_TEST( CopyCkSumTest );
      CPPUNIT_TEST( CopyCkSum2SourcesTest );
      CPPUNIT_TEST( CopyMultipleCkSumTest );
    CPPUNIT_TEST_SUITE_END();

    void CopySimpleTest();
    void Copy2SourcesTest();
    void CopyCkSumTest();
    void CopyCkSum2SourcesTest();
    void CopyMultipleCkSumTest();
    void DoTest( XrdCl::PropertyList & properties, const std::string & dataPath, const std::string & address );
};

CPPUNIT_TEST_SUITE_REGISTRATION( MetalinkTest );



void MetalinkTest::DoTest( XrdCl::PropertyList & properties, const std::string & dataPath, const std::string & address )
{
  using namespace XrdCl;
  //----------------------------------------------------------------------------
  // Initialize and run the copy
  //----------------------------------------------------------------------------
  CopyProcess  process;
  PropertyList results;
  properties.Set( "metalink", true );

  CPPUNIT_ASSERT_XRDST( process.AddJob( properties, &results ) );
  CPPUNIT_ASSERT_XRDST( process.Prepare() );
  CPPUNIT_ASSERT_XRDST( process.Run(0) );

  //----------------------------------------------------------------------------
  // Cleanup
  //----------------------------------------------------------------------------
  FileSystem fs( address );
  CPPUNIT_ASSERT_XRDST( fs.Rm( dataPath + "/output.dat" ) );
}

//------------------------------------------------------------------------------
// Simple copy test
//------------------------------------------------------------------------------
void MetalinkTest::CopySimpleTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath",    dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string metalinkURL = address + "/" + dataPath + "/metalink/input1.metalink"; //< single source, no checksum
  std::string meta4URL    = address + "/" + dataPath + "/metalink/input1.meta4";    //< single source, no checksum
  std::string targetURL   = address + "/" + dataPath + "/input1.metalink";          //< use metalink file name (will be automatically replaced)
  std::string target4URL  = address + "/" + dataPath + "/input1.meta4";             //< use metalink file name (will be automatically replaced)

  //----------------------------------------------------------------------------
  // Run the test
  //----------------------------------------------------------------------------

  // metalink 3.0
  PropertyList properties1;
  properties1.Set( "source", metalinkURL );
  properties1.Set( "target", targetURL   );
  DoTest( properties1, dataPath, address );

  //metalink 4.0
  PropertyList properties2;
  properties2.Set( "source", meta4URL    );
  properties2.Set( "target", target4URL  );
  DoTest( properties2, dataPath, address );
}

//------------------------------------------------------------------------------
// Copy test - 2 sources
//------------------------------------------------------------------------------
void MetalinkTest::Copy2SourcesTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath",    dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string metalinkURL = address + "/" + dataPath + "/metalink/input2.metalink"; //< 2 source files (1st one does not exists), no checksum
  std::string meta4URL    = address + "/" + dataPath + "/metalink/input2.meta4";    //< 2 source files (1st one does not exists), no checksum
  std::string targetURL   = address + "/" + dataPath + "/output.dat";

  //----------------------------------------------------------------------------
  // Run the test
  //----------------------------------------------------------------------------

  // metalink 3.0
  PropertyList properties1;
  properties1.Set( "source", metalinkURL );
  properties1.Set( "target", targetURL    );
  DoTest( properties1, dataPath, address  );

  // metalink 4.0
  PropertyList properties2;
  properties2.Set( "source", meta4URL );
  properties2.Set( "target", targetURL    );
  DoTest( properties2, dataPath, address  );
}

//------------------------------------------------------------------------------
// Copy test with checksum
//------------------------------------------------------------------------------
void MetalinkTest::CopyCkSumTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath",    dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string metalinkURL = address + "/" + dataPath + "/metalink/input3.metalink"; //< single source, checksum
  std::string meta4URL    = address + "/" + dataPath + "/metalink/input3.meta4";    //< single source, checksum
  std::string targetURL   = address + "/" + dataPath + "/input3.metalink";          //< use metalink file name (will be automatically replaced)
  std::string target4URL  = address + "/" + dataPath + "/input3.meta4";             //< use metalink file name (will be automatically replaced)

  //----------------------------------------------------------------------------
  // Run the test
  //----------------------------------------------------------------------------

  // metalink 3.0
  PropertyList properties1;
  properties1.Set( "source", metalinkURL );
  properties1.Set( "target", targetURL    );
  properties1.Set( "checkSumMode", "end2end" );
  properties1.Set( "checkSumType", "zcrc32"  );
  DoTest( properties1, dataPath, address );

  // metalink 4.0
  PropertyList properties2;
  properties2.Set( "source", meta4URL );
  properties2.Set( "target", target4URL    );
  properties2.Set( "checkSumMode", "end2end" );
  properties2.Set( "checkSumType", "zcrc32"  );
  DoTest( properties2, dataPath, address );
}

//------------------------------------------------------------------------------
// Copy test with checksum and 2 sources
//------------------------------------------------------------------------------
void MetalinkTest::CopyCkSum2SourcesTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath",    dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string metalinkURL = address + "/" + dataPath + "/metalink/input4.metalink"; //< 2 source files, 1st one doesn't much the checksum
  std::string meta4URL    = address + "/" + dataPath + "/metalink/input4.meta4";    //< 2 source files, 1st one doesn't much the checksum
  std::string targetURL   = address;

  //----------------------------------------------------------------------------
  // Run the test
  //----------------------------------------------------------------------------

  // metalink 3.0
  PropertyList properties1;
  properties1.Set( "source", metalinkURL );
  properties1.Set( "target", targetURL    );
  properties1.Set( "checkSumMode", "end2end" );
  properties1.Set( "checkSumType", "zcrc32"  );
  DoTest( properties1, dataPath, address );

  // metalink 3.0
  PropertyList properties2;
  properties2.Set( "source", meta4URL );
  properties2.Set( "target", targetURL    );
  properties2.Set( "checkSumMode", "end2end" );
  properties2.Set( "checkSumType", "zcrc32"  );
  DoTest( properties2, dataPath, address );
}

//------------------------------------------------------------------------------
// Copy test with multiple checksums
//------------------------------------------------------------------------------
void MetalinkTest::CopyMultipleCkSumTest()
{
  using namespace XrdCl;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Env *testEnv = TestEnv::GetEnv();

  std::string address;
  std::string dataPath;

  CPPUNIT_ASSERT( testEnv->GetString( "MainServerURL", address ) );
  CPPUNIT_ASSERT( testEnv->GetString( "DataPath",    dataPath ) );

  URL url( address );
  CPPUNIT_ASSERT( url.IsValid() );

  std::string metalinkURL = address + "/" + dataPath + "/metalink/input5.metalink"; //< single source, multiple checksum (adler32 + md5 + zcrc32)
  std::string meta4URL    = address + "/" + dataPath + "/metalink/input5.meta4";    //< single source, multiple checksum (adler32 + md5 + zcrc32)
  std::string targetURL   = address + "/" + dataPath + "/output.dat";               //< use metalink file name (will be automatically replaced)

  //----------------------------------------------------------------------------
  // Run the test
  //----------------------------------------------------------------------------

  // metalink 3.0
  PropertyList properties1;
  properties1.Set( "source", metalinkURL );
  properties1.Set( "target", targetURL    );
  properties1.Set( "checkSumMode", "end2end" );
  properties1.Set( "checkSumType", "zcrc32"  );
  DoTest( properties1, dataPath, address );

  // metalink 4.0
  PropertyList properties2;
  properties2.Set( "source", meta4URL );
  properties2.Set( "target", targetURL    );
  properties2.Set( "checkSumMode", "end2end" );
  properties2.Set( "checkSumType", "zcrc32"  );
  DoTest( properties2, dataPath, address );
}

